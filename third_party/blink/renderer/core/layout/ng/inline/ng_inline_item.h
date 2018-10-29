// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGInlineItem_h
#define NGInlineItem_h

#include "third_party/blink/renderer/core/core_export.h"
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
    // When adding new values, make sure the bit size of |type_| is large
    // enough to store.
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
               const ComputedStyle* style = nullptr,
               LayoutObject* layout_object = nullptr);
  ~NGInlineItem();

  // Copy constructor adjusting start/end and shape results.
  NGInlineItem(const NGInlineItem&,
               unsigned adjusted_start,
               unsigned adjusted_end,
               scoped_refptr<const ShapeResult>);

  NGInlineItemType Type() const { return static_cast<NGInlineItemType>(type_); }
  const char* NGInlineItemTypeToString(int val) const;

  scoped_refptr<const ShapeResult> TextShapeResult() const {
    return shape_result_;
  }
  NGLayoutInlineShapeOptions ShapeOptions() const {
    return static_cast<NGLayoutInlineShapeOptions>(shape_options_);
  }

  // If this item is "empty" for the purpose of empty block calculation.
  bool IsEmptyItem() const { return is_empty_item_; }

  // If this item should create a box fragment. Box fragments can be omitted for
  // optimization if this is false.
  bool ShouldCreateBoxFragment() const { return should_create_box_fragment_; }
  void SetShouldCreateBoxFragment() { should_create_box_fragment_ = true; }

  unsigned StartOffset() const { return start_offset_; }
  unsigned EndOffset() const { return end_offset_; }
  unsigned Length() const { return end_offset_ - start_offset_; }

  TextDirection Direction() const { return DirectionFromLevel(BidiLevel()); }
  UBiDiLevel BidiLevel() const { return static_cast<UBiDiLevel>(bidi_level_); }
  // Resolved bidi level for the reordering algorithm. Certain items have
  // artificial bidi level for the reordering algorithm without affecting its
  // direction.
  UBiDiLevel BidiLevelForReorder() const;

  const ComputedStyle* Style() const { return style_.get(); }
  LayoutObject* GetLayoutObject() const { return layout_object_; }

  void SetOffset(unsigned start, unsigned end);
  void SetEndOffset(unsigned);

  bool HasStartEdge() const;
  bool HasEndEdge() const;

  void SetStyleVariant(NGStyleVariant style_variant) {
    style_variant_ = static_cast<unsigned>(style_variant);
  }
  NGStyleVariant StyleVariant() const {
    return static_cast<NGStyleVariant>(style_variant_);
  }

  // Get or set the whitespace collapse type at the end of this item.
  NGCollapseType EndCollapseType() const {
    return static_cast<NGCollapseType>(end_collapse_type_);
  }
  void SetEndCollapseType(NGCollapseType type);

  // True if this item was generated (not in DOM).
  // NGInlineItemsBuilder may generate break opportunitites to express the
  // context that are lost during the whitespace collapsing. This item is used
  // during the line breaking and layout, but is not supposed to generate
  // fragments.
  bool IsGenerated() const { return is_generated_; }
  void SetIsGenerated() { is_generated_ = true; }

  // Whether the end collapsible space run contains a newline.
  // Valid only when kCollapsible or kCollapsed.
  bool IsEndCollapsibleNewline() const { return is_end_collapsible_newline_; }
  void SetEndCollapseType(NGCollapseType type, bool is_newline);

  static void Split(Vector<NGInlineItem>&, unsigned index, unsigned offset);

  // Get RunSegmenter properties.
  UScriptCode Script() const;
  FontFallbackPriority GetFontFallbackPriority() const;
  OrientationIterator::RenderOrientation RenderOrientation() const;
  RunSegmenter::RunSegmenterRange CreateRunSegmenterRange() const;
  // Whether the other item has the same RunSegmenter properties or not.
  bool EqualsRunSegment(const NGInlineItem&) const;
  // Set RunSegmenter properties.
  static unsigned PopulateItemsFromRun(Vector<NGInlineItem>&,
                                       unsigned index,
                                       const RunSegmenter::RunSegmenterRange&);
  void SetRunSegment(const RunSegmenter::RunSegmenterRange&);
  static unsigned PopulateItemsFromFontOrientation(
      Vector<NGInlineItem>&,
      unsigned index,
      unsigned end_offset,
      OrientationIterator::RenderOrientation);
  void SetFontOrientation(OrientationIterator::RenderOrientation);

  void SetBidiLevel(UBiDiLevel);
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
  scoped_refptr<const ComputedStyle> style_;
  LayoutObject* layout_object_;

  // UScriptCode is -1 (USCRIPT_INVALID_CODE) to 177 as of ICU 60.
  // This can be packed to 8 bits, by handling -1 separately.
  static constexpr unsigned kScriptBits = 8;
  static constexpr unsigned kInvalidScript = (1 << kScriptBits) - 1;

  unsigned type_ : 4;
  unsigned script_ : kScriptBits;
  unsigned font_fallback_priority_ : 2;  // FontFallbackPriority.
  unsigned render_orientation_ : 1;      // RenderOrientation (excl. kInvalid.)
  unsigned bidi_level_ : 8;              // UBiDiLevel is defined as uint8_t.
  unsigned shape_options_ : 2;
  unsigned is_empty_item_ : 1;
  unsigned should_create_box_fragment_ : 1;
  unsigned style_variant_ : 2;
  unsigned end_collapse_type_ : 2;  // NGCollapseType
  unsigned is_end_collapsible_newline_ : 1;
  unsigned is_symbol_marker_ : 1;
  unsigned is_generated_ : 1;
  friend class NGInlineNode;
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

#endif  // NGInlineItem_h
