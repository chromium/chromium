// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_NODE_H_

#include "base/gtest_prod_util.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node_data.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_input_node.h"
#include "third_party/blink/renderer/core/layout/ng/svg/ng_svg_character_data.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class NGBreakToken;
class NGColumnSpannerPath;
class NGConstraintSpace;
class NGInlineChildLayoutContext;
class NGLayoutResult;
class NGOffsetMapping;
struct NGInlineItemsData;
struct SvgTextContentRange;

// Represents an anonymous block box to be laid out, that contains consecutive
// inline nodes and their descendants.
class CORE_EXPORT NGInlineNode : public NGLayoutInputNode {
 public:
  explicit NGInlineNode(LayoutBlockFlow*);
  explicit NGInlineNode(std::nullptr_t) : NGLayoutInputNode(nullptr) {}

  LayoutBlockFlow* GetLayoutBlockFlow() const {
    return To<LayoutBlockFlow>(box_.Get());
  }
  NGLayoutInputNode NextSibling() const { return nullptr; }

  const NGLayoutResult* Layout(const NGConstraintSpace&,
                               const NGBreakToken*,
                               const NGColumnSpannerPath*,
                               NGInlineChildLayoutContext* context) const;

  // Computes the value of min-content and max-content for this anonymous block
  // box. min-content is the inline size when lines wrap at every break
  // opportunity, and max-content is when lines do not wrap at all.
  MinMaxSizesResult ComputeMinMaxSizes(WritingMode container_writing_mode,
                                       const NGConstraintSpace&,
                                       const MinMaxSizesFloatInput&) const;

  // Instruct to re-compute |PrepareLayout| on the next layout.
  void InvalidatePrepareLayoutForTest() {
    LayoutBlockFlow* block_flow = GetLayoutBlockFlow();
    block_flow->ResetNGInlineNodeData();
    DCHECK(!IsPrepareLayoutFinished());
  }

  const NGInlineItemsData& ItemsData(bool is_first_line) const {
    return Data().ItemsData(is_first_line);
  }

  // There's a special intrinsic size measure quirk for images that are direct
  // children of table cells that have auto inline-size: When measuring
  // intrinsic min/max inline sizes, we pretend that it's not possible to break
  // between images, or between text and images. Note that this only applies
  // when measuring. During actual layout, on the other hand, standard breaking
  // rules are to be followed.
  // See https://quirks.spec.whatwg.org/#the-table-cell-width-calculation-quirk
  bool IsStickyImagesQuirkForContentSize() const;

  // Returns the text content to use for content sizing. This is normally the
  // same as |items_data.text_content|, except when sticky images quirk is
  // needed.
  static String TextContentForStickyImagesQuirk(const NGInlineItemsData&);

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
  // This function must be called with clean layout.
  const NGOffsetMapping* ComputeOffsetMappingIfNeeded() const;

  // Get |NGOffsetMapping| for the |layout_block_flow|. |layout_block_flow|
  // should be laid out. This function works for both new and legacy layout.
  static const NGOffsetMapping* GetOffsetMapping(
      LayoutBlockFlow* layout_block_flow);

  bool IsBidiEnabled() const { return Data().is_bidi_enabled_; }
  TextDirection BaseDirection() const { return Data().BaseDirection(); }

  bool HasInitialLetterBox() const { return Data().has_initial_letter_box_; }

  bool HasRuby() const { return Data().has_ruby_; }

  bool IsBlockLevel() { return EnsureData().is_block_level_; }

  // @return if this node can contain the "first formatted line".
  // https://www.w3.org/TR/CSS22/selector.html#first-formatted-line
  bool CanContainFirstFormattedLine() const {
    DCHECK(GetLayoutBlockFlow());
    return GetLayoutBlockFlow()->CanContainFirstFormattedLine();
  }

  bool UseFirstLineStyle() const;
  void CheckConsistency() const;

  // This function is available after PrepareLayout(), only for SVG <text>.
  const Vector<std::pair<unsigned, NGSvgCharacterData>>& SvgCharacterDataList()
      const;
  // This function is available after PrepareLayout(), only for SVG <text>.
  const HeapVector<SvgTextContentRange>& SvgTextLengthRangeList() const;
  // This function is available after PrepareLayout(), only for SVG <text>.
  const HeapVector<SvgTextContentRange>& SvgTextPathRangeList() const;

  String ToString() const;

  struct FloatingObject {
    DISALLOW_NEW();

    void Trace(Visitor* visitor) const {}

    const ComputedStyle& float_style;
    const ComputedStyle& style;
    LayoutUnit float_inline_max_size_with_margin;
  };

  static bool NeedsShapingForTesting(const NGInlineItem& item);

  // Prepare inline and text content for layout. Must be called before
  // calling the Layout method.
  void PrepareLayoutIfNeeded() const;

 protected:
  FRIEND_TEST_ALL_PREFIXES(NGInlineNodeTest, SegmentBidiChangeSetsNeedsLayout);

  bool IsPrepareLayoutFinished() const;

  void PrepareLayout(NGInlineNodeData* previous_data) const;

  void CollectInlines(NGInlineNodeData*,
                      NGInlineNodeData* previous_data = nullptr) const;
  const SvgTextChunkOffsets* FindSvgTextChunks(LayoutBlockFlow& block,
                                               NGInlineNodeData& data) const;
  void SegmentText(NGInlineNodeData*) const;
  void SegmentScriptRuns(NGInlineNodeData*) const;
  void SegmentFontOrientation(NGInlineNodeData*) const;
  void SegmentBidiRuns(NGInlineNodeData*) const;
  void ShapeText(NGInlineItemsData*,
                 const String* previous_text = nullptr,
                 const HeapVector<NGInlineItem>* previous_items = nullptr,
                 const Font* override_font = nullptr) const;
  void ShapeTextForFirstLineIfNeeded(NGInlineNodeData*) const;
  void ShapeTextIncludingFirstLine(
      NGInlineNodeData* data,
      const String* previous_text,
      const HeapVector<NGInlineItem>* previous_items) const;
  void AssociateItemsWithInlines(NGInlineNodeData*) const;

  NGInlineNodeData* MutableData() const {
    return To<LayoutBlockFlow>(box_.Get())->GetNGInlineNodeData();
  }
  const NGInlineNodeData& Data() const {
    DCHECK(IsPrepareLayoutFinished() &&
           !GetLayoutBlockFlow()->NeedsCollectInlines());
    return *To<LayoutBlockFlow>(box_.Get())->GetNGInlineNodeData();
  }
  // Same as |Data()| but can access even when |NeedsCollectInlines()| is set.
  const NGInlineNodeData& MaybeDirtyData() const {
    DCHECK(IsPrepareLayoutFinished());
    return *To<LayoutBlockFlow>(box_.Get())->GetNGInlineNodeData();
  }
  const NGInlineNodeData& EnsureData() const;

  void AdjustFontForTextCombineUprightAll() const;

  static void ComputeOffsetMapping(LayoutBlockFlow* layout_block_flow,
                                   NGInlineNodeData* data);

  friend class NGLineBreakerTest;
};

inline bool NGInlineNode::IsStickyImagesQuirkForContentSize() const {
  if (UNLIKELY(GetDocument().InQuirksMode())) {
    const ComputedStyle& style = Style();
    if (UNLIKELY(style.Display() == EDisplay::kTableCell &&
                 !style.LogicalWidth().IsSpecified()))
      return true;
  }
  return false;
}

template <>
struct DowncastTraits<NGInlineNode> {
  static bool AllowFrom(const NGLayoutInputNode& node) {
    return node.IsInline();
  }
};

}  // namespace blink

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(
    blink::NGInlineNode::FloatingObject)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_NODE_H_
