// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_ITEM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_ITEM_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item_segment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"
#include "third_party/blink/renderer/core/layout/ng/ng_style_variant.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/shaping/run_segmenter.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"

#include <unicode/ubidi.h>
#include <unicode/uscript.h>

namespace blink {

class LayoutObject;

// Class representing a single text node or styled inline element with text
// content segmented by style, text direction, sideways rotation, font fallback
// priority (text, symbol, emoji, etc), and script (but not by font).
// In this representation TextNodes are merged up into their parent inline
// element where possible.
class CORE_EXPORT NGInlineItem {
  DISALLOW_NEW();

 public:
  enum NGInlineItemType {
    kText,
    kControl,
    kAtomicInline,
    kOpenTag,
    kCloseTag,
    kFloating,
    kOutOfFlowPositioned,
    kListMarker,
    kBidiControl
  };

  // Whether pre- and post-context should be used for shaping.
  enum NGLayoutInlineShapeOptions {
    kNoContext = 0,
    kPreContext = 1,
    kPostContext = 2
  };

  enum NGCollapseType {
    // No collapsible spaces.
    kNotCollapsible,
    // This item is opaque to whitespace collapsing.
    kOpaqueToCollapsing,
    // This item ends with collapsible spaces.
    kCollapsible,
    // Collapsible spaces at the end of this item were collapsed.
    kCollapsed,
  };

  // The constructor and destructor can't be implicit or inlined, because they
  // require full definition of ComputedStyle.
  NGInlineItem(NGInlineItemType type,
               unsigned start,
               unsigned end,
               LayoutObject* layout_object = nullptr);
  ~NGInlineItem();

  // Copy constructor adjusting start/end and shape results.
  NGInlineItem(const NGInlineItem&,
               unsigned adjusted_start,
               unsigned adjusted_end,
               scoped_refptr<const ShapeResult>);

  NGInlineItemType Type() const { return type_; }
  const char* NGInlineItemTypeToString(int val) const;

  const ShapeResult* TextShapeResult() const { return shape_result_.get(); }
  NGLayoutInlineShapeOptions ShapeOptions() const {
    return static_cast<NGLayoutInlineShapeOptions>(shape_options_);
  }

  // If this item is "empty" for the purpose of empty block calculation.
  bool IsEmptyItem() const { return is_empty_item_; }
  void SetIsEmptyItem(bool value) { is_empty_item_ = value; }

  // If this item is either a float or OOF-positioned node. If an inline
  // formatting-context *only* contains these types of nodes we consider it
  // block-level, and run the |NGBlockLayoutAlgorithm| instead of the
  // |NGInlineLayoutAlgorithm|.
  bool IsBlockLevel() const { return is_block_level_; }
  void SetIsBlockLevel(bool value) { is_block_level_ = value; }

  // If this item should create a box fragment. Box fragments can be omitted for
  // optimization if this is false.
  bool ShouldCreateBoxFragment() const {
    if (Type() == kOpenTag || Type() == kCloseTag)
      return ToLayoutInline(layout_object_)->ShouldCreateBoxFragment();
    DCHECK_EQ(Type(), kAtomicInline);
    return false;
  }
  void SetShouldCreateBoxFragment() {
    DCHECK(Type() == kOpenTag || Type() == kCloseTag);
    ToLayoutInline(layout_object_)->SetShouldCreateBoxFragment();
  }

  unsigned StartOffset() const { return start_offset_; }
  unsigned EndOffset() const { return end_offset_; }
  unsigned Length() const { return end_offset_ - start_offset_; }

  TextDirection Direction() const { return DirectionFromLevel(BidiLevel()); }
  UBiDiLevel BidiLevel() const { return static_cast<UBiDiLevel>(bidi_level_); }
  // Resolved bidi level for the reordering algorithm. Certain items have
  // artificial bidi level for the reordering algorithm without affecting its
  // direction.
  UBiDiLevel BidiLevelForReorder() const {
    // List markers should not be reordered to protect it from being included
    // into unclosed inline boxes.
    return Type() != NGInlineItem::kListMarker ? BidiLevel() : 0;
  }

  LayoutObject* GetLayoutObject() const { return layout_object_; }

  bool IsImage() const {
    return GetLayoutObject() && GetLayoutObject()->IsLayoutImage();
  }

  void SetOffset(unsigned start, unsigned end) {
    DCHECK_GE(end, start);
    start_offset_ = start;
    end_offset_ = end;
    // Any modification to the offset will invalidate the shape result.
    shape_result_ = nullptr;
  }
  void SetEndOffset(unsigned end_offset) {
    DCHECK_GE(end_offset, start_offset_);
    end_offset_ = end_offset;
    // Any modification to the offset will invalidate the shape result.
    shape_result_ = nullptr;
  }

  bool HasStartEdge() const {
    DCHECK(Type() == kOpenTag || Type() == kCloseTag);
    // TODO(kojii): Should use break token when NG has its own tree building.
    return !GetLayoutObject()->IsInlineElementContinuation();
  }
  bool HasEndEdge() const {
    DCHECK(Type() == kOpenTag || Type() == kCloseTag);
    // TODO(kojii): Should use break token when NG has its own tree building.
    return !GetLayoutObject()->IsLayoutInline() ||
           !ToLayoutInline(GetLayoutObject())->Continuation();
  }

  void SetStyleVariant(NGStyleVariant style_variant) {
    style_variant_ = static_cast<unsigned>(style_variant);
  }
  NGStyleVariant StyleVariant() const {
    return static_cast<NGStyleVariant>(style_variant_);
  }
  const ComputedStyle* Style() const {
    // Use the |ComputedStyle| in |LayoutObject|, because not all style changes
    // re-run |CollectInlines()|.
    DCHECK(layout_object_);
    NGStyleVariant variant = StyleVariant();
    if (variant == NGStyleVariant::kStandard)
      return layout_object_->Style();
    DCHECK_EQ(variant, NGStyleVariant::kFirstLine);
    return layout_object_->FirstLineStyle();
  }

  // Get or set the whitespace collapse type at the end of this item.
  NGCollapseType EndCollapseType() const {
    return static_cast<NGCollapseType>(end_collapse_type_);
  }
  void SetEndCollapseType(NGCollapseType type) {
    DCHECK(Type() == NGInlineItem::kText || type == kOpaqueToCollapsing ||
           (Type() == NGInlineItem::kControl && type == kCollapsible));
    end_collapse_type_ = type;
  }

  // True if this item was generated (not in DOM).
  // NGInlineItemsBuilder may generate break opportunitites to express the
  // context that are lost during the whitespace collapsing. This item is used
  // during the line breaking and layout, but is not supposed to generate
  // fragments.
  bool IsGeneratedForLineBreak() const { return is_generated_for_line_break_; }
  void SetIsGeneratedForLineBreak() { is_generated_for_line_break_ = true; }

  // Whether the end collapsible space run contains a newline.
  // Valid only when kCollapsible or kCollapsed.
  bool IsEndCollapsibleNewline() const { return is_end_collapsible_newline_; }
  void SetEndCollapseType(NGCollapseType type, bool is_newline) {
    SetEndCollapseType(type);
    is_end_collapsible_newline_ = is_newline;
  }

  static void Split(Vector<NGInlineItem>&, unsigned index, unsigned offset);

  // RunSegmenter properties.
  unsigned SegmentData() const { return segment_data_; }
  static void SetSegmentData(const RunSegmenter::RunSegmenterRange& range,
                             Vector<NGInlineItem>* items);

  RunSegmenter::RunSegmenterRange CreateRunSegmenterRange() const {
    return NGInlineItemSegment::UnpackSegmentData(start_offset_, end_offset_,
                                                  segment_data_);
  }

  // Whether the other item has the same RunSegmenter properties or not.
  bool EqualsRunSegment(const NGInlineItem& other) const {
    return segment_data_ == other.segment_data_;
  }

  void SetBidiLevel(UBiDiLevel level) {
    // Invalidate ShapeResult because it depends on the resolved direction.
    if (DirectionFromLevel(level) != DirectionFromLevel(bidi_level_))
      shape_result_ = nullptr;
    bidi_level_ = level;
  }
  static unsigned SetBidiLevel(Vector<NGInlineItem>&,
                               unsigned index,
                               unsigned end_offset,
                               UBiDiLevel);

  void AssertOffset(unsigned offset) const;
  void AssertEndOffset(unsigned offset) const;

  bool IsSymbolMarker() const { return is_symbol_marker_; }
  void SetIsSymbolMarker(bool b) { is_symbol_marker_ = b; }

  String ToString() const;

 private:
  void ComputeBoxProperties();

  unsigned start_offset_;
  unsigned end_offset_;
  scoped_refptr<const ShapeResult> shape_result_;
  LayoutObject* layout_object_;

  NGInlineItemType type_;
  // |segment_data_| is valid only for |type_ == NGInlineItem::kText|.
  unsigned segment_data_ : NGInlineItemSegment::kSegmentDataBits;
  unsigned bidi_level_ : 8;              // UBiDiLevel is defined as uint8_t.
  unsigned shape_options_ : 2;
  unsigned is_empty_item_ : 1;
  unsigned is_block_level_ : 1;
  unsigned style_variant_ : 2;
  unsigned end_collapse_type_ : 2;  // NGCollapseType
  unsigned is_end_collapsible_newline_ : 1;
  unsigned is_symbol_marker_ : 1;
  unsigned is_generated_for_line_break_ : 1;
  friend class NGInlineNode;
  friend class NGInlineNodeDataEditor;
};

inline void NGInlineItem::AssertOffset(unsigned offset) const {
  DCHECK((offset >= start_offset_ && offset < end_offset_) ||
         (offset == start_offset_ && start_offset_ == end_offset_));
}

inline void NGInlineItem::AssertEndOffset(unsigned offset) const {
  DCHECK_GE(offset, start_offset_);
  DCHECK_LE(offset, end_offset_);
}

// Represents a text content with a list of NGInlineItem. A node may have an
// additional NGInlineItemsData for ::first-line pseudo element.
struct CORE_EXPORT NGInlineItemsData {
  USING_FAST_MALLOC(NGInlineItemsData);

 public:
  // Text content for all inline items represented by a single NGInlineNode.
  // Encoded either as UTF-16 or latin-1 depending on the content.
  String text_content;
  Vector<NGInlineItem> items;

  // Cache RunSegmenter segments when at least one item has multiple runs.
  // Set to nullptr when all items has only single run, which is common case for
  // most writing systems. However, in multi-script writing systems such as
  // Japanese, almost every item has multiple runs.
  std::unique_ptr<NGInlineItemSegments> segments;

  // The DOM to text content offset mapping of this inline node.
  std::unique_ptr<NGOffsetMapping> offset_mapping;

  void AssertOffset(unsigned index, unsigned offset) const {
    items[index].AssertOffset(offset);
  }
  void AssertEndOffset(unsigned index, unsigned offset) const {
    items[index].AssertEndOffset(offset);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_ITEM_H_
