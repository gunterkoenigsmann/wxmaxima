// -*- mode: c++; c-file-style: "linux"; c-basic-offset: 2; indent-tabs-mode: nil -*-
//
//  Copyright (C) 2004-2015 Andrej Vodopivec <andrej.vodopivec@gmail.com>
//  Copyright (C) 2014-2018 Gunter Königsmann <wxMaxima@physikbuch.de>
//  Copyright (C) 2020      Kuba Ober <kuba@bertec.com>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
//
//  SPDX-License-Identifier: GPL-2.0+

/*! \file
  
  The definition of the base class of all cells the worksheet consists of.
 */

#ifndef MATHCELL_H
#define MATHCELL_H

#include "../precomp.h"
#include "CellPtr.h"
#include "CellIterators.h"
#include "Configuration.h"
#include "StringUtils.h"
#include "TextStyle.h"
#include <wx/defs.h>
#if wxUSE_ACCESSIBILITY
#include "wx/access.h"
#endif // wxUSE_ACCESSIBILITY
#include <algorithm>
#include <map>
#include <memory>
#include <vector>
#include <type_traits>
class CellPointers;
class EditorCell;
class GroupCell;
class TextCell;
class Worksheet;
class wxXmlNode;

// Forward declarations
template <typename C, typename std::enable_if<std::is_base_of<Cell, C>::value, bool>::type = true>
inline auto OnInner(const C *cell);
template <typename C, typename std::enable_if<std::is_base_of<Cell, C>::value, bool>::type = true>
inline auto OnInner(C *cell);

/*! The supported types of math cells
 */
enum CellType : int8_t
{
  MC_TYPE_DEFAULT,
  MC_TYPE_MAIN_PROMPT, //!< Input labels
  MC_TYPE_PROMPT,      //!< Maxima questions or lisp prompts
  MC_TYPE_LABEL,       //!< An output label generated by maxima
  MC_TYPE_INPUT,       //!< A cell containing code
  MC_TYPE_WARNING,     //!< A warning output by maxima
  MC_TYPE_ERROR,       //!< An error output by maxima
  MC_TYPE_TEXT,        //!< Text that isn't passed to maxima
  MC_TYPE_ASCIIMATHS,  //!< Equations displayed in 2D
  MC_TYPE_SUBSECTION,  //!< A subsection name
  MC_TYPE_SUBSUBSECTION,  //!< A subsubsection name
  MC_TYPE_HEADING5,  //!< A subsubsection name
  MC_TYPE_HEADING6,  //!< A subsubsection name
  MC_TYPE_SECTION,     //!< A section name
  MC_TYPE_TITLE,       //!< The title of the document
  MC_TYPE_IMAGE,       //!< An image
  MC_TYPE_SLIDE,       //!< An animation created by the with_slider_* maxima commands
  MC_TYPE_GROUP        //!< A group cells that bundles several individual cells together
};

#if wxUSE_ACCESSIBILITY
class CellAccessible;
#endif

//! A class that carries information about the type of a cell.
class CellTypeInfo
{
public:
  /** The name of the cell type, e.g. "FooCell"
   *
   * Use DEFINE_CELL(FooCell) to define this class,
   * and the Cell::GetInfo() member.
   */
  virtual const wxString &GetName() const = 0;
};

/*!
  The base class all cell types the worksheet can consist of are derived from

  Every Cell is part of two double-linked lists:
   - A Cell does have a member m_previous that points to the previous item
     (or contains a NULL for the head node of the list) and a member named m_next 
     that points to the next cell (or contains a NULL if this is the end node of a list).
   - And there is m_nextToDraw that contain fractions and similar 
     items as one element if they are drawn as a single 2D object that isn't divided by
     a line break, but will contain every single element of a fraction as a separate 
     object if the fraction is broken into several lines and therefore displayed in its
     a linear form.

  Also every list of Cells can be a branch of a tree since every math cell contains
  a pointer to its parent group cell.

  Besides the cell types that are directly user visible there are cells for several
  kinds of items that are displayed in a special way like abs() statements (displayed
  as horizontal rules), subscripts, superscripts and exponents.
  Another important concept realized by a class derived from this one is
  the group cell that groups all things that are foldable in the gui like:
   - A combination of maxima input with the output, the input prompt and the output 
     label.
   - A chapter or a section and
   - Images with their title (or the input cells that generated them)
   .

  \attention Derived classes must test if m_next equals NULL and if it doesn't
  they have to delete() it.

  On systems where wxWidget supports (and is compiled with)
  accessibility features Cell is derived from wxAccessible which
  allows every element in the worksheet to identify itself to an
  eventual screen reader.

 */

// 112 bytes
class Cell: public Observed
{
#if wxUSE_ACCESSIBILITY
  friend class CellAccessible;
#endif
  friend class CellList;

  // This class can be derived from wxAccessible which has no copy constructor
  void operator=(const Cell&) = delete;
  Cell(const Cell&) = delete;

  template <typename C, typename std::enable_if<std::is_base_of<Cell, C>::value, bool>::type>
  friend inline auto OnInner(const C *cell);
  template <typename C, typename std::enable_if<std::is_base_of<Cell, C>::value, bool>::type>
  friend inline auto OnInner(C *cell);

public:

  Cell(GroupCell *group, Configuration **config);

  /*! Create a copy of this cell

    This method is purely virtual, which means every child class has to define
    its own Copy() method.
   */
  virtual std::unique_ptr<Cell> Copy() const = 0;

  //! Returns the information about this cell's type.
  virtual const CellTypeInfo &GetInfo() = 0;

  /*! Scale font sizes and line widths according to the zoom factor.

    Is used for displaying/printing/exporting of text/maths
   */
  int Scale_Px(double px) const {return (*m_configuration)->Scale_Px(px);}
  AFontSize Scale_Px(AFontSize size) const { return (*m_configuration)->Scale_Px(size); }

#if wxUSE_ACCESSIBILITY
  // The methods whose implementation within Cell should be sufficient are
  // not virtual. Should that prove not to be the case eventually, they can be
  // made virtual with due caution.
  //! Accessibility: Inform the Screen Reader which cell is the parent of this one
  wxAccStatus GetParent (Cell ** parent) const;
  //! Accessibility: Retrieve a child cell. childId=0 is the current cell
  wxAccStatus GetChild(int childId, Cell **child) const;
  //! Accessibility: Is pt inside this cell or a child cell?
  wxAccStatus HitTest (const wxPoint &pt, int *childId, Cell **child);

  //! Accessibility: Describe the current cell to a Screen Reader
  virtual wxAccStatus GetDescription(int childId, wxString *description) const;
  //! Accessibility: Does this or a child cell currently own the focus?
  virtual wxAccStatus GetFocus (int *childId, Cell **child) const;
  //! Accessibility: Describe the action this Cell performs, if any
  virtual wxAccStatus GetDefaultAction(int childId, wxString *actionName) const;
  //! Accessibility: Where is this cell to be found?
  virtual wxAccStatus GetLocation (wxRect &rect, int elementId);
  //! Accessibility: What is the contents of this cell?
  virtual wxAccStatus GetValue (int childId, wxString *strValue) const;
  virtual wxAccStatus GetRole (int childId, wxAccRole *role) const;
#endif
  

  /*! Returns the ToolTip this cell provides at a given point.
   *
   * \param point The point in worksheet coordinates, must be inside the
   *              cell or else an empty string is returned.
   *
   * \returns the tooltip text, or empty string if none.
   */
  virtual const wxString &GetToolTip(wxPoint point) const;

  //! Delete this list of cells.
  virtual ~Cell();

  //! How many cells does this cell contain?
  int CellsInListRecursive() const;
  
  //! The part of the rectangle rect that is in the region that is currently drawn
  wxRect CropToUpdateRegion(wxRect rect) const;

  //! Is part of this rectangle in the region that is currently drawn?
  bool InUpdateRegion(const wxRect &rect) const;

  //! Is this cell inside the region that is currently drawn?
  bool InUpdateRegion() const { return InUpdateRegion(GetRect()); }

  //! Do we want this cell to start with a linebreak?
  bool SoftLineBreak(bool breakLine = true)
  {
    bool result = (m_breakLine == breakLine);
    m_breakLine = breakLine;
    return result;
  }

  //! Do we want this cell to start with a pagebreak?
  void BreakPage(bool breakPage)
  { m_breakPage = breakPage; }

  //! Are we allowed to break a line here?
  bool BreakLineHere() const { return m_breakLine || m_forceBreakLine; }

  //! Does this cell begin with a manual linebreak?
  bool HasHardLineBreak() const { return m_forceBreakLine; }

  //! Does this cell begin with a forced (manually set) page break?
  bool BreakPageHere() const { return m_breakPage; }

  /*! Try to split this command into lines to make it fit on the screen

    \retval true = This cell was split into lines.
  */
  virtual bool BreakUp();

protected:
  //! Break up the internal cells of this cell, and mark it as broken up.
  void BreakUpAndMark();

public:
  /*! Is a part of this cell inside a certain rectangle?

    \param sm The rectangle to test for collision with this cell
    \param all
     - true means test this cell and the ones that are following it in the list
     - false means test this cell only.
   */
  bool ContainsRect(const wxRect &sm, bool all = true) const;

  /*! Is a given point inside this cell?

    \param point The point to test for collision with this cell
   */
  bool ContainsPoint(wxPoint point) const { return GetRect().Contains(point); }

  /*! Clears memory from cached items automatically regenerated when the cell is drawn
    
    The scaled version of the image will be recreated automatically once it is 
    needed.
   */
  virtual void ClearCache()
  {}

  /*! Clears the cache of the whole list of cells starting with this one.

    For details see ClearCache().
   */
  void ClearCacheList();

  /*! Draw this cell

    \param point The x and y position this cell is drawn at: All top-level cells get their
    position during recalculation. But for the cells within them the position needs a 
    second step after determining the dimension of the contents of the top-level cell.

    Example: The position of the denominator of a fraction can only be determined
    after the height of denominator and numerator are known.
   */
  virtual void Draw(wxPoint point);

  void Draw(){Draw(m_currentPoint);}

  /*! Draw this list of cells

    \param point The x and y position this cell is drawn at
   */
  void DrawList(wxPoint point);
  void DrawList(){DrawList(m_currentPoint);}

  /*! Draw a rectangle that marks this cell or this list of cells as selected

    \param all
     - true:  Draw the bounding box around this list of cells
     - false: Draw the bounding box around this cell only
     \param dc The drawing context the box is drawn in.
  */
  virtual void DrawBoundingBox(wxDC &WXUNUSED(dc), bool all = false);

  bool DrawThisCell(wxPoint point);
  bool DrawThisCell(){return DrawThisCell(m_currentPoint);}

  /*! Insert (or remove) a forced linebreak at the beginning of this cell.

    \param force
     - true: Insert a forced linebreak
     - false: Remove the forced linebreak
   */
  void ForceBreakLine(bool force = true) { m_forceBreakLine = m_breakLine = force; }

  /*! Get the height of this cell

    This value is recalculated by Recalculate()
  */
  int GetHeight() const
  { return m_height; }

  /*! Get the width of this cell

    This value is recalculated by Recalculate()
  */
  int GetWidth() const
  { return m_width; }

  /*! Get the distance between the top and the center of this cell.

    Remember that (for example with double fractions) the center does not have to be in the 
    middle of a cell even if this object is --- by definition --- center-aligned.
   */
  int GetCenter() const
  { return m_center; }

  /*! Get the distance between the center and the bottom of this cell


    Remember that (for example with double fractions) the center does not have to be in the 
    middle of an output cell even if the current object is --- by definition --- 
    center-aligned.

    This value is recalculated by Recalculate
   */
  int GetDrop() const
  { return m_height - m_center; }

  /*! 
    Returns the type of this cell.
   */
  CellType GetType() const
  { return m_type; }

  /*! Returns the maximum distance between center and bottom of this line

    Note that the center doesn't need to be exactly in the middle of an object.
    For a fraction for example the center is exactly at the middle of the 
    horizontal line.
   */
  int GetMaxDrop() const;

  /*! Returns the maximum distance between top and center of this line

    Note that the center doesn't need to be exactly in the middle of an object.
    For a fraction for example the center is exactly at the middle of the 
    horizontal line.
  */
  int GetCenterList() const;

  /*! Returns the total height of this line

    Returns GetCenterList()+GetMaxDrop()
   */
  int GetHeightList() const;

  //! How many pixels is this list of cells wide, if we don't break it into lines?
  int GetFullWidth() const;

  /*! How many pixels is the current line of this list of cells wide?

    This command returns the real line width when all line breaks are really performed. 
    See GetFullWidth().
   */
  int GetLineWidth() const;

  /*! Get the x position of the top left of this cell

    See m_currentPoint for more details.
   */
  int GetCurrentX() const
  { return m_currentPoint.x; }

  /*! Get the y position of the top left of this cell

    See m_currentPoint for more details.
   */
  int GetCurrentY() const
  { return m_currentPoint.y; }

  /*! Get the smallest rectangle this cell fits in

    \param all
      - true: Get the rectangle for this cell and the ones that follow it in the list of cells
      - false: Get the rectangle for this cell only.
   */
  virtual wxRect GetRect(bool all = false) const;

  //! True, if something that affects the cell size has changed.
  virtual bool NeedsRecalculation(AFontSize fontSize) const;
  
  virtual wxString GetDiffPart() const;

  /*! Recalculate the size of the cell and the difference between top and center

    Must set: m_height, m_width, m_center.

    \param fontsize In exponents, super- and subscripts the font size is reduced.
    This cell therefore needs to know which font size it has to be drawn at.
  */
  virtual void Recalculate(AFontSize fontsize);

  /*! Recalculate both width and height of this list of cells.

    Is faster than a <code>RecalculateHeightList();RecalculateWidths();</code>.
   */
  void RecalculateList(AFontSize fontsize);

  //! Tell a whole list of cells that their fonts have changed
  void FontsChangedList();

  bool NeedsToRecalculateWidths() const { return m_recalculateWidths; }
  void ClearNeedsToRecalculateWidths() { m_recalculateWidths = false; }

  //! Mark all cached size information as "to be calculated".
  void ResetData();

  void ResetDataList();

  //! Mark the cached height and width information as "to be calculated".
  void ResetSize()
    { 
       m_recalculateWidths = true; 
       m_recalculate_maxCenter = true;
       m_recalculate_maxDrop = true;
       m_recalculate_maxWidth = true;
       m_recalculate_lineWidth = true;
    }

  void ResetCellListSizes()
    { 
      m_recalculate_maxCenter = true;
      m_recalculate_maxDrop = true;
      m_recalculate_maxWidth = true;
      m_recalculate_lineWidth = true;
    }

  //! Mark the cached height information of the whole list of cells as "to be calculated".
  void ResetSizeList();

  void SetBigSkip(bool skip) { m_bigSkip = skip; }
  bool HasBigSkip() const { return m_bigSkip; }

  //! Sets the text style according to the type
  virtual void SetType(CellType type);

  TextStyle GetStyle() const
  { return m_textStyle; }

  // cppcheck-suppress functionStatic
  // cppcheck-suppress functionConst
  void SetPen(double lineWidth = 1.0) const;

  //! Mark this cell as highlighted (e.G. being in a maxima box)
  void SetHighlight(bool highlight) { m_highlight = highlight; }
  //! Is this cell highlighted (e.G. inside a maxima box)
  bool GetHighlight() const { return m_highlight; }

  virtual void SetExponentFlag()
  {}

  virtual void SetValue(const wxString &WXUNUSED(text)) {}
  virtual const wxString &GetValue() const;

  //! Get the first cell in this list of cells
  Cell *first() const;

  //! Get the last cell in this list of cells
  Cell *last() const;

  struct Range {
    Cell *first, *last;
  };

  /*! Returns the first and last cells within the given rectangle, recursing
   * into the inner cells.
   */
  Range GetCellsInRect(const wxRect &rect) const;

  /*! Returns the first and last cells within the given rectangle, without recursing
   * into the inner cells.
   */
  Range GetListCellsInRect(const wxRect &rect) const;

  //! Select the cells inside this cell described by the rectangle rect.
  virtual Range GetInnerCellsInRect(const wxRect &rect) const;

  //! Is this cell an operator?
  virtual bool IsOperator() const { return false; }

  //! Do we have an operator in this line - draw () in frac...
  bool IsCompound() const;

  virtual bool IsShortNum() const { return false; }

  //! Returns the group cell this cell belongs to
  GroupCell *GetGroup() const;

  //! For the bitmap export we sometimes want to know how big the result will be...
  struct SizeInMillimeters
  {
  public:
    double x, y;
  };

  //! Returns the list's representation as a string.
  virtual wxString ListToString() const;

  /*! Returns all variable and function names used inside this list of cells.
  
    Used for detecting lookalike chars in function and variable names.
   */
  wxString VariablesAndFunctionsList() const;
  //! Convert this list to its LaTeX representation
  virtual wxString ListToMatlab() const;
  //! Convert this list to its LaTeX representation
  virtual wxString ListToTeX() const;
  //! Convert this list to a representation fit for saving in a .wxmx file
  virtual wxString ListToXML() const;

  //! Convert this list to a MathML representation
  virtual wxString ListToMathML(bool startofline = false) const;

  //! Convert this list to an OMML representation
  virtual wxString ListToOMML(bool startofline = false) const;

  //! Convert this list to an RTF representation
  virtual wxString ListToRTF(bool startofline = false) const;

  //! Returns the cell's representation as a string.
  virtual wxString ToString() const;

  /*! Returns the cell's representation as RTF.

    If this method returns wxEmptyString this might mean that this cell is 
    better handled in OMML.
   */
  virtual wxString ToRTF() const
  { return wxEmptyString; }

  //! Converts an OMML tag to the corresponding RTF snippet
  static wxString OMML2RTF(wxXmlNode *node);

  //! Converts OMML math to RTF math
  static wxString OMML2RTF(wxString ommltext);

  /*! Returns the cell's representation as OMML

    If this method returns wxEmptyString this might mean that this cell is 
    better handled in RTF; The OOML can later be translated to the 
    respective RTF maths commands using OMML2RTF.

    Don't know why OMML was implemented in a world that already knows MathML,
    though.
   */
  virtual wxString ToOMML() const;
  //! Convert this cell to its Matlab representation
  virtual wxString ToMatlab() const;
  //! Convert this cell to its LaTeX representation
  virtual wxString ToTeX() const;
  //! Convert this cell to a representation fit for saving in a .wxmx file
  virtual wxString ToXML() const;
  //! Convert this cell to a representation fit for saving in a .wxmx file
  virtual wxString ToMathML() const;

  //! Escape a string for RTF
  static wxString RTFescape(wxString, bool MarkDown = false);

  //! Escape a string for XML
  static wxString XMLescape(wxString);

  // cppcheck-suppress functionStatic
  // cppcheck-suppress functionConst

  /*! Undo breaking this cell into multiple lines

    Some cells have different representations when they contain a line break.
    Examples for this are fractions or a set of parenthesis.

    This function tries to return a cell to the single-line form.
   */
  virtual void Unbreak();

  /*! Unbreak this line

    Some cells have different representations when they contain a line break.
    Examples for this are fractions or a set of parenthesis.

    This function tries to return a list of cells to the single-line form.
  */
  virtual void UnbreakList();

  Cell *GetPrevious() const { return m_previous; }

  //! Get the next cell in the list.
  Cell *GetNext() const { return m_next.get(); }
  /*! Get the next cell that needs to be drawn

    In case of potential 2d objects like fractions either the fraction needs to be
    drawn as a single 2D object or the nominator, the cell containing the "/" and 
    the denominator are pointed to by GetNextToDraw() as single separate objects.
   */
  virtual Cell *GetNextToDraw() const = 0;

  /*! Tells this cell which one should be the next cell to be drawn

    If the cell is displayed as 2d object this sets the pointer to the next cell.

    If the cell is broken into lines this sets the pointer of the last of the 
    list of cells this cell is displayed as.
   */
  virtual void SetNextToDraw(Cell *next) = 0;
  template <typename T, typename Del,
            typename std::enable_if<std::is_base_of<Cell, T>::value, bool>::type = true>
  void SetNextToDraw(const std::unique_ptr<T, Del> &ptr) { SetNextToDraw(ptr.get()); }

  /*! Determine if this cell contains text that isn't code

    \return true, if this is a text cell, a title cell, a section, a subsection or a sub(n)section cell.
   */
  bool IsComment() const
  {
    return m_type == MC_TYPE_TEXT || m_type == MC_TYPE_SECTION ||
           m_type == MC_TYPE_SUBSECTION || m_type == MC_TYPE_SUBSUBSECTION ||
           m_type == MC_TYPE_HEADING5 || m_type == MC_TYPE_HEADING6 || m_type == MC_TYPE_TITLE;
  }

  /*! Whether this cell is not to be drawn.
   *
   * Currently the following items fall into this category:
   * - parenthesis around fractions or similar things that clearly can be recognized as atoms
   * - plus signs within numbers
   * - The output in folded GroupCells
   */
  bool IsHidden() const { return m_isHidden; }

  virtual void Hide(bool hide = true) { m_isHidden = hide; }

  bool IsEditable(bool input = false) const
  {
    return (m_type == MC_TYPE_INPUT &&
            m_previous && m_previous->m_type == MC_TYPE_MAIN_PROMPT)
           || (!input && IsComment());
  }

  //! Can this cell be popped out interactively in gnuplot?
  virtual bool CanPopOut() const { return false; }
  
  /*! Retrieve the gnuplot source data for this image 

    wxEmptyString means: No such data.
   */
  virtual wxString GnuplotSource() const {return wxEmptyString;}

  //! Processes a key event.
  virtual void ProcessEvent(wxKeyEvent &WXUNUSED(event))
  {}

  /*! Add a semicolon to a code cell, if needed.

    Defined in GroupCell and EditorCell
  */
  virtual bool AddEnding()
  { return false; }

  virtual void SelectPointText(wxPoint point);
      
  virtual void SelectRectText(wxPoint one, wxPoint two);
  
  virtual void PasteFromClipboard(bool primary = false);
  
  virtual bool CopyToClipboard() const
  { return false; }

  virtual bool CutToClipboard()
  { return false; }

  virtual void SelectAll()
  {}

  virtual bool CanCopy() const
  { return false; }

  virtual wxPoint PositionToPoint(int WXUNUSED(pos) = -1)
  { return wxPoint(-1, -1); }

  virtual bool IsDirty() const
  { return false; }

  virtual void SwitchCaretDisplay()
  {}

  virtual void SetFocus(bool WXUNUSED(focus))
  {}

  //! Sets the foreground color
  void SetForeground();

  virtual bool IsActive() const
  { return false; }

  /*! Define which Cell is the GroupCell this list of cells belongs to

    Also automatically sets this cell as the "parent" of all cells of the list.
   */
  void SetGroupList(GroupCell *group);

  //! Define which Sell is the GroupCell this list of cells belongs to
  virtual void SetGroup(GroupCell *group);
  //! Sets the TextStyle of this cell
  virtual void SetStyle(TextStyle style)
  {
    m_textStyle = style;
    ResetData();
  }
  //! Is this cell possibly output of maxima?
  bool IsMath() const;

  //! 0 for ordinary cells, 1 for slide shows and diagrams displayed with a 1-pixel border
  virtual int GetImageBorderWidth() const { return 0; }

  //! Copy common data (used when copying a cell)
  void CopyCommonData(const Cell & cell);

  //! Return a copy of the list of cells beginning with this one.
  std::unique_ptr<Cell> CopyList() const;

  //! Return a copy of the given list of cells.
  static std::unique_ptr<Cell> CopyList(const Cell *cell);

  //! Remove this cell's tooltip
  void ClearToolTip();
  //! Set the tooltip to a given temporary string - the cell will move from it
  void SetToolTip(wxString &&toolTip);
  //! Set the tooltip of this math cell - it must be exist at least as long
  //! as the cell does. Translation results behave that way. I.e. it must be
  //! a static string!
  void SetToolTip(const wxString *tooltip);
  //! Add another tooltip to this cell
  void AddToolTip(const wxString &tip);
  //! Tells this cell where it is placed on the worksheet
  virtual void SetCurrentPoint(wxPoint point) { m_currentPoint = point; }
  //! Tells this cell where it is placed on the worksheet
  void SetCurrentPoint(int x, int y) { m_currentPoint = {x, y}; }
  //! Where is this cell placed on the worksheet?
  wxPoint GetCurrentPoint() const {return m_currentPoint;}
  bool ContainsToolTip() const { return m_containsToolTip; }

  /*! Whether this cell is broken into two or more lines.
   *
   * Long abs(), conjugate(), fraction and similar cells can be displayed as 2D objects,
   * but will be displayed in their linear form (and therefore broken into lines) if they
   * end up to be wider than the screen. In this case m_isBrokenIntoLines is true.
   */
  bool IsBrokenIntoLines() const { return m_isBrokenIntoLines; }

  /*! Do we want to begin this cell with a center dot if it is part of a product?
   *
   * Maxima will represent a product like (a*b*c) by a list like the following:
   * [*,a,b,c]. This would result us in converting (a*b*c) to the following LaTeX
   * code: \\left(\\cdot a ß\\cdot b \\cdot c\\right) which obviously is one \\cdot too
   * many => we need parenthesis cells to set this flag for the first cell in
   * their "inner cell" list.
   */
  bool GetSuppressMultiplicationDot() const { return m_suppressMultiplicationDot; }
  void SetSuppressMultiplicationDot(bool val) { m_suppressMultiplicationDot = val; }
  //! Whether this is a hidable multiplication sign
  bool GetHidableMultSign() const { return m_isHidableMultSign; }
  void SetHidableMultSign(bool val) { m_isHidableMultSign = val; }

  /*! What should end up if placing this cell on the clipboard?

    AltCopyTexts for example make sense for subCells: a_n looks like a[n], even if both 
    are lookalikes and the cell therefore needs to know what to put on the 
    clipboard if this cell were copied. They also make sense in many other
    places we may never have thought about. But since we seriously want to 
    save memory space on the ubiuitous TextCells it might be scary to apply 
    this principle to them, at least if you know that text you copy from the
    internet to a terminal might contain additional commands with TextSize=0...
  */
  virtual void SetAltCopyText(const wxString &text);
  //! Get the text set using SetAltCopyText - may be empty.
  virtual const wxString &GetAltCopyText() const { return wxm::emptyString; }

#if wxUSE_ACCESSIBILITY
  CellAccessible *GetAccessible();
#endif

protected:
  std::unique_ptr<Cell> MakeVisiblyInvalidCell() const;
public:
  static std::unique_ptr<Cell> MakeVisiblyInvalidCell(Configuration **config);

protected:
//** Bases and internal members (16 bytes)
//**
// VTable  *__vtable;
// Observed __observed;

//** 8-byte objects (8 bytes)
//**
  /*! The point in the work sheet at which this cell begins.

    The begin of a cell is defined as
     - x=the left border of the cell
     - y=the vertical center of the cell. Which (per example in the case of a fraction)
       might not be the physical center but the vertical position of the horizontal line
       between numerator and denominator.

    The current point is recalculated 
     - for GroupCells by GroupCell::RecalculateHeight
     - for EditorCells by it's GroupCell's RecalculateHeight and
     - for Cells when they are drawn.
  */
  wxPoint m_currentPoint{-1, -1};

//** 8/4-byte objects (40 + 8* bytes)
//**
private:
  //! The next cell in the list of cells, or null if it's the last cell.
  std::unique_ptr<Cell> m_next;

  //! The previous cell in the list of cells, or null if it's the list head.
  Cell *m_previous = {};

#if wxUSE_ACCESSIBILITY
  std::unique_ptr<CellAccessible> m_accessible;
#endif

protected:
  /*! The GroupCell this list of cells belongs to.
    Reads NULL, if no parent cell has been set - which is treated as an Error by GetGroup():
    every math cell has a GroupCell it belongs to.
  */
  CellPtr<GroupCell> m_group;

  Configuration **m_configuration;

  //! This tooltip is owned by us when m_ownsToolTip is true. Otherwise,
  //! it points to a "static" string.
  const wxString *m_toolTip /* initialized in the constructor */;

//** 4-byte objects (28 bytes)
//**
protected:
  //! The height of this cell.
  int m_height = -1;
  //! The width of this cell; is recalculated by RecalculateHeight.
  int m_width = -1;
  /*! Caches the width of the list starting with this cell.

     - Will contain -1, if it has not yet been calculated.
     - Won't be recalculated on appending new cells to the list.
   */
  mutable int m_fullWidth = -1;
  /*! Caches the width of the rest of the line this cell is part of.

     - Will contain -1, if it has not yet been calculated.
     - Won't be recalculated on appending new cells to the list.
   */
  mutable int m_lineWidth = -1;
  int m_center = -1;
  mutable int m_maxCenter = -1;
  mutable int m_maxDrop = -1;
protected:
//** 2-byte objects (2 bytes)
//**
  //! The font size is smaller in super- and subscripts.
  AFontSize m_fontSize_Scaled = {};

//** 1-byte objects (2 bytes)
//**
  CellType m_type = MC_TYPE_DEFAULT;
  TextStyle m_textStyle = TS_DEFAULT;

private:
//** Bitfield objects (3 bytes)
//**
  void InitBitFields()
  { // Keep the initailization order below same as the order
    // of bit fields in this class!
    m_ownsToolTip = false;
    m_bigSkip = false;
    m_isBrokenIntoLines = false;
    m_isBrokenIntoLines_old = false;
    m_isHidden = false;
    m_isHidableMultSign = false;
    m_suppressMultiplicationDot = false;
    m_recalculateWidths = true;
    m_recalculate_maxCenter = true;
    m_recalculate_maxDrop = true;
    m_recalculate_maxWidth = true;
    m_recalculate_lineWidth = true;
    m_containsToolTip = false;
    m_breakPage = false;
    m_breakLine = false;
    m_forceBreakLine = false;
    m_highlight = false;
  }

  // In the boolean bit fields below, InitBitFields is an indication that
  // the InitBitFields() method initializes a given field. It should be
  // only added once such initialization is in place. It makes it easier
  // to verify that all bit fields are initialized.

  //! Whether the cell owns its m_tooltip - otherwise it points to a static string.
  bool m_ownsToolTip : 1 /* InitBitFields */;
  bool m_bigSkip : 1 /* InitBitFields */;
  bool m_isBrokenIntoLines : 1 /* InitBitFields */;
  bool m_isBrokenIntoLines_old : 1 /* InitBitFields */;
  bool m_isHidden : 1 /* InitBitFields */;
  bool m_isHidableMultSign : 1 /* InitBitFields */;
  bool m_suppressMultiplicationDot : 1 /* InitBitFields */;

  //! true, if this cell clearly needs recalculation
  bool m_recalculateWidths : 1 /* InitBitFields */;
  mutable bool m_recalculate_maxCenter : 1 /* InitBitFields */;
  mutable bool m_recalculate_maxDrop : 1 /* InitBitFields */;
  mutable bool m_recalculate_maxWidth : 1 /* InitBitFields */;
  mutable bool m_recalculate_lineWidth : 1 /* InitBitFields */;
  bool m_containsToolTip : 1 /* InitBitFields */;
  bool m_breakPage : 1 /* InitBitFields */;
  //! Are we allowed to add a line break before this cell?
  bool m_breakLine : 1 /* InitBitFields */;
  bool m_forceBreakLine : 1 /* InitBitFields */;
  bool m_highlight : 1 /* InitBitFields */;

protected:
  //! Iterator to the beginning of the inner cell range. The end iterator is default-constructed.
  virtual InnerCellIterator InnerBegin() const;

  inline Worksheet *GetWorksheet() const;

  //! To be called if the font has changed.
  virtual void FontsChanged()
  {
    ResetSize();
    ResetData();
  }

  const wxString &GetLocalToolTip() const;

  CellPointers *GetCellPointers() const;

private:
  void RecalcCenterListAndMaxDropCache();
};

// The static cast here requires Cell to be defined
template <typename T>
template <typename PtrT, typename std::enable_if<std::is_pointer<PtrT>::value, bool>::type>
inline PtrT CellPtr<T>::CastAs() const noexcept { return dynamic_cast<PtrT>(static_cast<Cell*>(base_get())); }

#if wxUSE_ACCESSIBILITY
class CellAccessible final : public wxAccessible
{
public:
  explicit CellAccessible(Cell *const forCell) : m_cell(forCell) {}

  //! Accessibility: Inform the Screen Reader which cell is the parent of this one
  wxAccStatus GetParent (wxAccessible **parent) override;
  //! Accessibility: How many childs of this cell GetChild() can retrieve?
  wxAccStatus GetChildCount (int *childCount) override;
  //! Accessibility: Retrieve a child cell. childId=0 is the current cell
  wxAccStatus GetChild (int childId, wxAccessible **child) override;
  //! Accessibility: Is pt inside this cell or a child cell?
  wxAccStatus HitTest (const wxPoint &pt,
                      int *childId, wxAccessible **childObject) override;
  //! Accessibility: Describe the current cell to a Screen Reader
  wxAccStatus GetDescription(int childId, wxString *description) override;
  //! Accessibility: Describe the action this Cell performs, if any
  wxAccStatus GetDefaultAction(int childId, wxString *actionName) override;
  //! Accessibility: Does this or a child cell currently own the focus?
  wxAccStatus GetFocus (int *childId, wxAccessible **child) override;
  //! Accessibility: Where is this cell to be found?
  wxAccStatus GetLocation (wxRect &rect, int elementId) override;
  //! Accessibility: What is the contents of this cell?
  wxAccStatus GetValue (int childId, wxString *strValue) override;
  wxAccStatus GetRole (int childId, wxAccRole *role) override;
private:
  Cell *const m_cell;
};
#endif

//! Returns an iterable the goes over the cell list, starting with given, possibly null, cell.
template <typename C, typename std::enable_if<std::is_base_of<Cell, C>::value, bool>::type = true>
inline auto OnList(const C *cell) { return CellListAdapter<const C>(cell); }
template <typename C, typename std::enable_if<std::is_base_of<Cell, C>::value, bool>::type = true>
inline auto OnList(C *cell)       { return CellListAdapter<C>(cell); }

//! Returns an iterable that goes over the cell draw list, starting with given, possibly null, cell.
template <typename C, typename std::enable_if<std::is_base_of<Cell, C>::value, bool>::type = true>
inline auto OnDrawList(const C *cell) { return CellDrawListAdapter<const C>(cell); }
template <typename C, typename std::enable_if<std::is_base_of<Cell, C>::value, bool>::type = true>
inline auto OnDrawList(C *cell)       { return CellDrawListAdapter<C>(cell); }

//! Returns an iterable that goes over the inner cells of this cell.
template <typename C, typename std::enable_if<std::is_base_of<Cell, C>::value, bool>::type>
inline auto OnInner(const C *cell) { return InnerCellAdapter(cell->InnerBegin()); }
template <typename C, typename std::enable_if<std::is_base_of<Cell, C>::value, bool>::type>
inline auto OnInner(C *cell) { return InnerCellAdapter(cell->InnerBegin()); }

#endif // MATHCELL_H
