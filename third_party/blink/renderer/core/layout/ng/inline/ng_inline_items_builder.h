// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_ITEMS_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_ITEMS_BUILDER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/inline/empty_offset_mapping_builder.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_height_metrics.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping_builder.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ComputedStyle;
class LayoutInline;
class LayoutObject;
class LayoutText;
struct NGInlineNodeData;
class NGDirtyLines;

// NGInlineItemsBuilder builds a string and a list of NGInlineItem from inlines.
//
// When appending, spaces are collapsed according to CSS Text, The white space
// processing rules
// https://drafts.csswg.org/css-text-3/#white-space-rules
//
// By calling EnterInline/ExitInline, it inserts bidirectional control
// characters as defined in:
// https://drafts.csswg.org/css-writing-modes-3/#bidi-control-codes-injection-table
//
// NGInlineItemsBuilder may optionally take an NGOffsetMappingBuilder template
// parameter to construct the white-space collapsed offset mapping, which maps
// offsets in the concatenation of all appended strings and characters to
// offsets in |text_|.
// See https://goo.gl/CJbxky for more details about offset mapping.
template <typename OffsetMappingBuilder>
class NGInlineItemsBuilderTemplate {
  STACK_ALLOCATED();

 public:
  // Create a builder that appends items to |items|.
  //
  // If |dirty_lines| is given, this builder calls its functions to mark lines
  // dirty.
  explicit NGInlineItemsBuilderTemplate(Vector<NGInlineItem>* items,
                                        NGDirtyLines* dirty_lines = nullptr)
      : items_(items), dirty_lines_(dirty_lines) {}
  ~NGInlineItemsBuilderTemplate();

  String ToString();

  // Returns whether the items contain any Bidi controls.
  bool HasBidiControls() const { return has_bidi_controls_; }

  // Returns if the inline node has no content. For example:
  // <span></span> or <span><float></float></span>.
  bool IsEmptyInline() const { return is_empty_inline_; }

  bool IsBlockLevel() const { return is_block_level_; }

  // True if changes to an item may affect different layout of earlier lines.
  // May not be able to use line caches even when the line or earlier lines are
  // not dirty.
  bool ChangesMayAffectEarlierLines() const {
    return changes_may_affect_earlier_lines_;
  }

  // Append a string from |LayoutText|.
  //
  // If |previous_data| is given, reuse existing items if they exist and are
  // reusable. Otherwise appends new items.
  void AppendText(LayoutText* layout_text,
                  const NGInlineNodeData* previous_data);

  // Append existing items from an unchanged LayoutObject.
  // Returns whether the existing items could be reused.
  // NOTE: The state of the builder remains unchanged if the append operation
  // fails (i.e. if it returns false).
  bool AppendTextReusing(const NGInlineNodeData& previous_data,
                         LayoutText* layout_text);

  // Append a string.
  // When appending, spaces are collapsed according to CSS Text, The white space
  // processing rules
  // https://drafts.csswg.org/css-text-3/#white-space-rules
  // @param style The style for the string.
  // If a nullptr, it should skip shaping. Atomic inlines and bidi controls use
  // this.
  // @param LayoutText The LayoutText for the string.
  // If a nullptr, it does not generate BidiRun. Bidi controls use this.
  void AppendText(const String& text, LayoutText* layout_text);

  // Append a break opportunity; e.g., <wbr> element.
  void AppendBreakOpportunity(LayoutObject* layout_object);

  // Append a unicode "object replacement character" for an atomic inline,
  // signaling the presence of a non-text object to the unicode bidi algorithm.
  void AppendAtomicInline(LayoutObject* layout_object);

  // Append floats and positioned objects in the same way as atomic inlines.
  // Because these objects need positions, they will be handled in
  // NGInlineLayoutAlgorithm.
  void AppendFloating(LayoutObject* layout_object);
  void AppendOutOfFlowPositioned(LayoutObject* layout_object);

  // Append a character.
  // The character is opaque to space collapsing; i.e., spaces before this
  // character and after this character can collapse as if this character does
  // not exist.
  void AppendOpaque(NGInlineItem::NGInlineItemType,
                    UChar,
                    LayoutObject* = nullptr);

  // Append a non-character item that is opaque to space collapsing.
  void AppendOpaque(NGInlineItem::NGInlineItemType,
                    LayoutObject* layout_object);

  // Append a Bidi control character, for LTR or RTL depends on the style.
  void EnterBidiContext(LayoutObject*,
                        const ComputedStyle*,
                        UChar ltr_enter,
                        UChar rtl_enter,
                        UChar exit);
  void EnterBidiContext(LayoutObject*, UChar enter, UChar exit);

  void EnterBlock(const ComputedStyle*);
  void ExitBlock();
  void EnterInline(LayoutInline*);
  void ExitInline(LayoutObject*);

  OffsetMappingBuilder& GetOffsetMappingBuilder() { return mapping_builder_; }

  void SetIsSymbolMarker(bool b);

  bool ShouldAbort() const { return false; }

  // Functions change |LayoutObject| states.
  void ClearInlineFragment(LayoutObject*);
  void ClearNeedsLayout(LayoutObject*);
  void UpdateShouldCreateBoxFragment(LayoutInline*);

 private:
  static bool NeedsBoxInfo();

  Vector<NGInlineItem>* items_;
  StringBuilder text_;

  NGDirtyLines* dirty_lines_;

  // |mapping_builder_| builds the whitespace-collapsed offset mapping
  // during inline collection. It is updated whenever |text_| is modified or a
  // white space is collapsed.
  OffsetMappingBuilder mapping_builder_;

  // Keep track of inline boxes to compute ShouldCreateBoxFragment.
  struct BoxInfo {
    unsigned item_index;
    bool should_create_box_fragment;
    const ComputedStyle& style;
    NGLineHeightMetrics text_metrics;

    BoxInfo(unsigned item_index, const NGInlineItem& item);
    bool ShouldCreateBoxFragmentForChild(const BoxInfo& child) const;
    void SetShouldCreateBoxFragment(Vector<NGInlineItem>* items);
  };
  Vector<BoxInfo> boxes_;

  struct BidiContext {
    LayoutObject* node;
    UChar enter;
    UChar exit;
  };
  Vector<BidiContext> bidi_context_;

  bool has_bidi_controls_ = false;
  bool is_empty_inline_ = true;
  bool is_block_level_ = true;
  bool changes_may_affect_earlier_lines_ = false;

  // Append a character.
  // Currently this function is for adding control characters such as
  // objectReplacementCharacter, and does not support all space collapsing logic
  // as its String version does.
  // See the String version for using nullptr for ComputedStyle and
  // LayoutObject.
  void Append(NGInlineItem::NGInlineItemType,
              UChar,
              LayoutObject*);

  void AppendCollapseWhitespace(const StringView,
                                const ComputedStyle*,
                                LayoutText*);
  void AppendPreserveWhitespace(const String&,
                                const ComputedStyle*,
                                LayoutText*);
  void AppendPreserveNewline(const String&, const ComputedStyle*, LayoutText*);

  void AppendForcedBreakCollapseWhitespace(LayoutObject*);
  void AppendForcedBreak(LayoutObject*);

  void RemoveTrailingCollapsibleSpaceIfExists();
  void RemoveTrailingCollapsibleSpace(NGInlineItem*);

  void RestoreTrailingCollapsibleSpaceIfRemoved();
  void RestoreTrailingCollapsibleSpace(NGInlineItem*);

  void AppendTextItem(const StringView,
                      LayoutText* layout_object);
  void AppendTextItem(NGInlineItem::NGInlineItemType type,
                      const StringView,
                      LayoutText* layout_object);
  void AppendEmptyTextItem(LayoutText* layout_object);

  void AppendGeneratedBreakOpportunity(LayoutObject*);

  void Exit(LayoutObject*);

  bool ShouldInsertBreakOpportunityAfterLeadingPreservedSpaces(
      const String&,
      const ComputedStyle&,
      unsigned index = 0) const;
  void InsertBreakOpportunityAfterLeadingPreservedSpaces(const String&,
                                                         const ComputedStyle&,
                                                         LayoutText*,
                                                         unsigned* start);
};

template <>
CORE_EXPORT bool
NGInlineItemsBuilderTemplate<NGOffsetMappingBuilder>::AppendTextReusing(
    const NGInlineNodeData&,
    LayoutText*);

template <>
CORE_EXPORT void
NGInlineItemsBuilderTemplate<NGOffsetMappingBuilder>::ClearInlineFragment(
    LayoutObject*);

template <>
CORE_EXPORT void
NGInlineItemsBuilderTemplate<NGOffsetMappingBuilder>::ClearNeedsLayout(
    LayoutObject*);

template <>
CORE_EXPORT void NGInlineItemsBuilderTemplate<
    NGOffsetMappingBuilder>::UpdateShouldCreateBoxFragment(LayoutInline*);

extern template class CORE_EXTERN_TEMPLATE_EXPORT
    NGInlineItemsBuilderTemplate<EmptyOffsetMappingBuilder>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    NGInlineItemsBuilderTemplate<NGOffsetMappingBuilder>;

using NGInlineItemsBuilder =
    NGInlineItemsBuilderTemplate<EmptyOffsetMappingBuilder>;
using NGInlineItemsBuilderForOffsetMapping =
    NGInlineItemsBuilderTemplate<NGOffsetMappingBuilder>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_ITEMS_BUILDER_H_
