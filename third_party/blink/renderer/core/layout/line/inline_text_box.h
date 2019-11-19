/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2009, 2010, 2011 Apple Inc.
 *               All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_INLINE_TEXT_BOX_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_INLINE_TEXT_BOX_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_text.h"
#include "third_party/blink/renderer/core/layout/line/inline_box.h"
#include "third_party/blink/renderer/platform/text/text_run.h"
#include "third_party/blink/renderer/platform/text/truncation.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class DocumentMarker;
class GraphicsContext;
class TextMarkerBase;

class CORE_EXPORT InlineTextBox : public InlineBox {
 public:
  InlineTextBox(LineLayoutItem item, int start, uint16_t length)
      : InlineBox(item),
        prev_text_box_(nullptr),
        next_text_box_(nullptr),
        start_(start),
        len_(length),
        truncation_(kCNoTruncation) {
    SetIsText(true);
  }

  LineLayoutText GetLineLayoutItem() const {
    return LineLayoutText(InlineBox::GetLineLayoutItem());
  }

  void Destroy() final;

  InlineTextBox* PrevForSameLayoutObject() const { return prev_text_box_; }
  InlineTextBox* NextForSameLayoutObject() const { return next_text_box_; }
  void SetNextForSameLayoutObject(InlineTextBox* n) { next_text_box_ = n; }
  void SetPreviousForSameLayoutObject(InlineTextBox* p) { prev_text_box_ = p; }
  void ManuallySetStartLenAndLogicalWidth(unsigned start,
                                          unsigned len,
                                          LayoutUnit logical_width);

  // FIXME: These accessors should DCHECK(!isDirty()). See
  // https://bugs.webkit.org/show_bug.cgi?id=97264
  unsigned Start() const { return start_; }
  unsigned end() const { return len_ ? start_ + len_ - 1 : start_; }
  unsigned Len() const { return len_; }

  void OffsetRun(int delta);

  uint16_t Truncation() const { return truncation_; }

  void MarkDirty() final;

  using InlineBox::HasHyphen;
  using InlineBox::SetHasHyphen;
  using InlineBox::CanHaveLeadingExpansion;
  using InlineBox::SetCanHaveLeadingExpansion;

  static inline bool CompareByStart(const InlineTextBox* first,
                                    const InlineTextBox* second) {
    return first->Start() < second->Start();
  }

  LayoutUnit BaselinePosition(FontBaseline) const final;
  LayoutUnit LineHeight() const final;

  bool GetEmphasisMarkPosition(const ComputedStyle&,
                               TextEmphasisPosition&) const;

  LayoutUnit OffsetTo(FontVerticalPositionType, FontBaseline) const;
  LayoutUnit VerticalPosition(FontVerticalPositionType, FontBaseline) const;

  LayoutRect LogicalOverflowRect() const;
  void SetLogicalOverflowRect(const LayoutRect&);
  LayoutUnit LogicalTopVisualOverflow() const {
    return LogicalOverflowRect().Y();
  }
  LayoutUnit LogicalBottomVisualOverflow() const {
    return LogicalOverflowRect().MaxY();
  }

  // charactersWithHyphen, if provided, must not be destroyed before the
  // TextRun.
  TextRun ConstructTextRun(
      const ComputedStyle&,
      StringBuilder* characters_with_hyphen = nullptr) const;
  TextRun ConstructTextRun(
      const ComputedStyle&,
      StringView,
      int maximum_length,
      StringBuilder* characters_with_hyphen = nullptr) const;

#if DCHECK_IS_ON()
  void DumpBox(StringBuilder&) const override;
#endif
  const char* BoxName() const override;
  String DebugName() const override;

  String GetText() const;

 public:
  TextRun ConstructTextRunForInspector(const ComputedStyle&) const;
  LayoutRect FrameRect() const {
    return LayoutRect(X(), Y(), Width(), Height());
  }

  virtual LayoutRect LocalSelectionRect(
      int start_pos,
      int end_pos,
      bool include_newline_space_width = true) const;
  void SelectionStartEnd(int& s_pos, int& e_pos) const;

  virtual void PaintDocumentMarker(GraphicsContext&,
                                   const LayoutPoint& box_origin,
                                   const DocumentMarker&,
                                   const ComputedStyle&,
                                   const Font&,
                                   bool grammar) const;
  virtual void PaintTextMarkerForeground(const PaintInfo&,
                                         const LayoutPoint& box_origin,
                                         const TextMarkerBase&,
                                         const ComputedStyle&,
                                         const Font&) const;
  virtual void PaintTextMarkerBackground(const PaintInfo&,
                                         const LayoutPoint& box_origin,
                                         const TextMarkerBase&,
                                         const ComputedStyle&,
                                         const Font&) const;

  void Move(const LayoutSize&) final;

 protected:
  void Paint(const PaintInfo&,
             const LayoutPoint&,
             LayoutUnit line_top,
             LayoutUnit line_bottom) const override;
  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation&,
                   const PhysicalOffset& accumulated_offset,
                   LayoutUnit line_top,
                   LayoutUnit line_bottom) override;

 private:
  void DeleteLine() final;
  void ExtractLine() final;
  void AttachLine() final;

 public:
  bool IsSelected() const final;
  bool HasWrappedSelectionNewline() const;
  float NewlineSpaceWidth() const;

 private:
  bool IsBoxEndIncludedInSelection() const;
  void SetTruncation(uint16_t);

  void ClearTruncation() final;
  LayoutUnit PlaceEllipsisBox(bool flow_is_ltr,
                              LayoutUnit visible_left_edge,
                              LayoutUnit visible_right_edge,
                              LayoutUnit ellipsis_width,
                              LayoutUnit& truncated_width,
                              InlineBox** found_box,
                              LayoutUnit logical_left_offset) final;

 public:
  bool IsLineBreak() const final;

  void SetExpansion(int new_expansion) {
    logical_width_ -= Expansion();
    InlineBox::SetExpansion(new_expansion);
    logical_width_ += new_expansion;
  }

 private:
  bool IsInlineTextBox() const final { return true; }

 public:
  int CaretMinOffset() const final;
  int CaretMaxOffset() const final;

  // Returns the x position relative to the left start of the text line.
  LayoutUnit TextPos() const;

 public:
  virtual int OffsetForPosition(LayoutUnit x,
                                IncludePartialGlyphsOption,
                                BreakGlyphsOption) const;
  virtual LayoutUnit PositionForOffset(int offset) const;

  // Returns false for offset after line break.
  bool ContainsCaretOffset(int offset) const;

  // Fills a vector with the pixel width of each character.
  void CharacterWidths(Vector<float>&) const;

 private:
  // The next/previous box that also uses our LayoutObject.
  InlineTextBox* prev_text_box_;
  InlineTextBox* next_text_box_;

  int start_;
  uint16_t len_;

  // Where to truncate when text overflow is applied.  We use special constants
  // to denote no truncation (the whole run paints) and full truncation (nothing
  // paints at all).
  uint16_t truncation_;

 private:
  TextRun::ExpansionBehavior GetExpansionBehavior() const {
    return (CanHaveLeadingExpansion() ? TextRun::kAllowLeadingExpansion
                                      : TextRun::kForbidLeadingExpansion) |
           (Expansion() && NextLeafChild() ? TextRun::kAllowTrailingExpansion
                                           : TextRun::kForbidTrailingExpansion);
  }
};

DEFINE_INLINE_BOX_TYPE_CASTS(InlineTextBox);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_INLINE_TEXT_BOX_H_
