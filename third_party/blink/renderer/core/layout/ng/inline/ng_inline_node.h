// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_NODE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node_data.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_input_node.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class NGBlockBreakToken;
class NGConstraintSpace;
class NGDirtyLines;
class NGInlineChildLayoutContext;
class NGInlineNodeLegacy;
class NGLayoutResult;
class NGOffsetMapping;
struct MinMaxSize;
struct NGInlineItemsData;

// Represents an anonymous block box to be laid out, that contains consecutive
// inline nodes and their descendants.
class CORE_EXPORT NGInlineNode : public NGLayoutInputNode {
 public:
  NGInlineNode(LayoutBlockFlow*);

  LayoutBlockFlow* GetLayoutBlockFlow() const {
    return To<LayoutBlockFlow>(box_);
  }
  NGLayoutInputNode NextSibling() { return nullptr; }

  // True in quirks mode or limited-quirks mode, which require line-height
  // quirks.
  // https://quirks.spec.whatwg.org/#the-line-height-calculation-quirk
  bool InLineHeightQuirksMode() const {
    return GetDocument().InLineHeightQuirksMode();
  }

  scoped_refptr<const NGLayoutResult> Layout(
      const NGConstraintSpace&,
      const NGBreakToken*,
      NGInlineChildLayoutContext* context);

  // Find the container of reusable line boxes. Returns nullptr if there are no
  // reusable line boxes.
  const NGPaintFragment* ReusableLineBoxContainer(const NGConstraintSpace&);

  // Computes the value of min-content and max-content for this anonymous block
  // box. min-content is the inline size when lines wrap at every break
  // opportunity, and max-content is when lines do not wrap at all.
  MinMaxSize ComputeMinMaxSize(WritingMode container_writing_mode,
                               const MinMaxSizeInput&,
                               const NGConstraintSpace* = nullptr);

  // Instruct to re-compute |PrepareLayout| on the next layout.
  void InvalidatePrepareLayoutForTest() {
    LayoutBlockFlow* block_flow = GetLayoutBlockFlow();
    block_flow->ResetNGInlineNodeData();
    DCHECK(!IsPrepareLayoutFinished());
    // There shouldn't be paint fragment if NGInlineNodeData does not exist.
    block_flow->SetPaintFragment(nullptr, nullptr);
  }

  const NGInlineItemsData& ItemsData(bool is_first_line) const {
    return Data().ItemsData(is_first_line);
  }

  // Returns the text content to use for content sizing. This is normally the
  // same as |items_data.text_content|, except when sticky images quirk is
  // needed.
  String TextContentForContentSize(const NGInlineItemsData& items_data) const;

  // Clear associated fragments for LayoutObjects.
  // They are associated when NGPaintFragment is constructed, but when clearing,
  // NGInlineItem provides easier and faster logic.
  static void ClearAssociatedFragments(const NGPhysicalFragment& fragment,
                                       const NGBlockBreakToken* break_token);

  // Returns true if we don't need to collect inline items after replacing
  // |layout_text| after deleting replacing subtext from |offset| to |length|
  // |new_text| is new text of |layout_text|.
  // This is optimized version of |PrepareLayout()|.
  static bool SetTextWithOffset(LayoutText* layout_text,
                                scoped_refptr<StringImpl> new_text,
                                unsigned offset,
                                unsigned length);

  // Returns the DOM to text content offset mapping of this block. If it is not
  // computed before, compute and store it in NGInlineNodeData.
  // This funciton must be called with clean layout.
  const NGOffsetMapping* ComputeOffsetMappingIfNeeded();

  // Get |NGOffsetMapping| for the |layout_block_flow|. |layout_block_flow|
  // should be laid out. This function works for both new and legacy layout.
  static const NGOffsetMapping* GetOffsetMapping(
      LayoutBlockFlow* layout_block_flow);

  bool IsBidiEnabled() const { return Data().is_bidi_enabled_; }
  TextDirection BaseDirection() const { return Data().BaseDirection(); }

  bool IsEmptyInline() { return EnsureData().is_empty_inline_; }

  bool IsBlockLevel() { return EnsureData().is_block_level_; }

  // @return if this node can contain the "first formatted line".
  // https://www.w3.org/TR/CSS22/selector.html#first-formatted-line
  bool CanContainFirstFormattedLine() const {
    DCHECK(GetLayoutBlockFlow());
    return GetLayoutBlockFlow()->CanContainFirstFormattedLine();
  }

  bool UseFirstLineStyle() const;
  void CheckConsistency() const;

  String ToString() const;

 protected:
  bool IsPrepareLayoutFinished() const;

  // Prepare inline and text content for layout. Must be called before
  // calling the Layout method.
  void PrepareLayoutIfNeeded();
  void PrepareLayout(std::unique_ptr<NGInlineNodeData> previous_data,
                     NGDirtyLines* dirty_lines);

  void CollectInlines(NGInlineNodeData*,
                      NGInlineNodeData* previous_data = nullptr,
                      NGDirtyLines* dirty_lines = nullptr);
  void SegmentText(NGInlineNodeData*);
  void SegmentScriptRuns(NGInlineNodeData*);
  void SegmentFontOrientation(NGInlineNodeData*);
  void SegmentBidiRuns(NGInlineNodeData*);
  void ShapeText(NGInlineItemsData*,
                 const String* previous_text = nullptr,
                 const Vector<NGInlineItem>* previous_items = nullptr);
  void ShapeTextForFirstLineIfNeeded(NGInlineNodeData*);
  void AssociateItemsWithInlines(NGInlineNodeData*);

  bool MarkLineBoxesDirty(LayoutBlockFlow*, const NGPaintFragment*);

  NGInlineNodeData* MutableData() {
    return To<LayoutBlockFlow>(box_)->GetNGInlineNodeData();
  }
  const NGInlineNodeData& Data() const {
    DCHECK(IsPrepareLayoutFinished() &&
           !GetLayoutBlockFlow()->NeedsCollectInlines());
    return *To<LayoutBlockFlow>(box_)->GetNGInlineNodeData();
  }
  // Same as |Data()| but can access even when |NeedsCollectInlines()| is set.
  const NGInlineNodeData& MaybeDirtyData() const {
    DCHECK(IsPrepareLayoutFinished());
    return *To<LayoutBlockFlow>(box_)->GetNGInlineNodeData();
  }
  const NGInlineNodeData& EnsureData();

  static String TextContentForStickyImagesQuirk(const NGInlineItemsData&);

  static void ComputeOffsetMapping(LayoutBlockFlow* layout_block_flow,
                                   NGInlineNodeData* data);

  friend class NGLineBreakerTest;
  friend class NGInlineNodeLegacy;
};

inline String NGInlineNode::TextContentForContentSize(
    const NGInlineItemsData& items_data) const {
  const String& text_content = items_data.text_content;
  if (UNLIKELY(text_content.IsEmpty()))
    return text_content;

  // There's a special intrinsic size measure quirk for images that are direct
  // children of table cells that have auto inline-size: When measuring
  // intrinsic min/max inline sizes, we pretend that it's not possible to break
  // between images, or between text and images. Note that this only applies
  // when measuring. During actual layout, on the other hand, standard breaking
  // rules are to be followed.
  // See https://quirks.spec.whatwg.org/#the-table-cell-width-calculation-quirk
  if (UNLIKELY(GetDocument().InQuirksMode())) {
    const ComputedStyle& style = Style();
    if (UNLIKELY(style.Display() == EDisplay::kTableCell &&
                 style.LogicalWidth().IsIntrinsicOrAuto())) {
      return TextContentForStickyImagesQuirk(items_data);
    }
  }

  return text_content;
}

template <>
struct DowncastTraits<NGInlineNode> {
  static bool AllowFrom(const NGLayoutInputNode& node) {
    return node.IsInline();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_NODE_H_
