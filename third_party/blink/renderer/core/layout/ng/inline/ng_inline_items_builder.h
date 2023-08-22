// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_ITEMS_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_ITEMS_BUILDER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/inline/empty_offset_mapping_builder.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping_builder.h"
#include "third_party/blink/renderer/core/layout/ng/svg/svg_inline_node_data.h"
#include "third_party/blink/renderer/platform/fonts/font_height.h"
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
  NGInlineItemsBuilderTemplate(
      LayoutBlockFlow* block_flow,
      HeapVector<NGInlineItem>* items,
      const SvgTextChunkOffsets* chunk_offsets = nullptr)
      : block_flow_(block_flow),
        items_(items),
        text_chunk_offsets_(chunk_offsets),
        is_text_combine_(block_flow_->IsLayoutNGTextCombine()) {}
  ~NGInlineItemsBuilderTemplate();

  LayoutBlockFlow* GetLayoutBlockFlow() const { return block_flow_; }

  String ToString();

  // Returns whether the items contain any Bidi controls.
  bool HasBidiControls() const { return has_bidi_controls_; }

  bool IsBlockLevel() const { return is_block_level_; }

  // True if there were any `unicode-bidi: plaintext`. In this case, changes to
  // an item may affect different layout of earlier lines. May not be able to
  // use line caches even when the line or earlier lines are not dirty.
  bool HasUnicodeBidiPlainText() const { return has_unicode_bidi_plain_text_; }

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
  NGInlineItem& AppendBreakOpportunity(LayoutObject* layout_object);

  // Append a unicode "object replacement character" for an atomic inline,
  // signaling the presence of a non-text object to the unicode bidi algorithm.
  void AppendAtomicInline(LayoutObject* layout_object);
  void AppendBlockInInline(LayoutObject* layout_object);

  // Append floats and positioned objects in the same way as atomic inlines.
  // Because these objects need positions, they will be handled in
  // NGInlineLayoutAlgorithm.
  void AppendFloating(LayoutObject* layout_object);
  void AppendOutOfFlowPositioned(LayoutObject* layout_object);

  // Append a character.
  // The character is opaque to space collapsing; i.e., spaces before this
  // character and after this character can collapse as if this character does
  // not exist.
  NGInlineItem& AppendOpaque(NGInlineItem::NGInlineItemType,
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

  // Set collected inline items data to |data|.
  void DidFinishCollectInlines(NGInlineNodeData* data);

  OffsetMappingBuilder& GetOffsetMappingBuilder() { return mapping_builder_; }

  void SetHasInititialLetterBox();
  void SetIsSymbolMarker();

  bool ShouldAbort() const { return false; }

  // Functions change |LayoutObject| states.
  bool ShouldUpdateLayoutObject() const;
  void ClearInlineFragment(LayoutObject*);
  void ClearNeedsLayout(LayoutObject*);
  void UpdateShouldCreateBoxFragment(LayoutInline*);

  // The following structs are public to modify VectorTraits in WTF namespace.
  struct BidiContext {
    DISALLOW_NEW();

   public:
    void Trace(Visitor*) const;

    Member<LayoutObject> node;
    UChar enter;
    UChar exit;
  };

  // Keep track of inline boxes to compute ShouldCreateBoxFragment.
  struct BoxInfo {
    DISALLOW_NEW();

    Member<const ComputedStyle> style;
    unsigned item_index;
    bool should_create_box_fragment;
    FontHeight text_metrics;

    void Trace(Visitor* visitor) const { visitor->Trace(style); }

    BoxInfo(unsigned item_index, const NGInlineItem& item);
    bool ShouldCreateBoxFragmentForChild(const BoxInfo& child) const;
    void SetShouldCreateBoxFragment(HeapVector<NGInlineItem>* items);
  };

 private:
  static bool NeedsBoxInfo();

  LayoutBlockFlow* const block_flow_;
  HeapVector<NGInlineItem>* items_;
  StringBuilder text_;

  // |mapping_builder_| builds the whitespace-collapsed offset mapping
  // during inline collection. It is updated whenever |text_| is modified or a
  // white space is collapsed.
  OffsetMappingBuilder mapping_builder_;

  HeapVector<BoxInfo> boxes_;
  HeapVector<BidiContext> bidi_context_;

  const SvgTextChunkOffsets* text_chunk_offsets_;

  const bool is_text_combine_;
  bool has_bidi_controls_ = false;
  bool has_floats_ = false;
  bool has_initial_letter_box_ = false;
  bool has_ruby_ = false;
  bool is_block_level_ = true;
  bool has_unicode_bidi_plain_text_ = false;
  bool is_bisect_line_break_disabled_ = false;
  bool is_score_line_break_disabled_ = false;

  // Append a character.
  // Currently this function is for adding control characters such as
  // objectReplacementCharacter, and does not support all space collapsing logic
  // as its String version does.
  // See the String version for using nullptr for ComputedStyle and
  // LayoutObject.
  NGInlineItem& Append(NGInlineItem::NGInlineItemType, UChar, LayoutObject*);

  void AppendCollapseWhitespace(const StringView,
                                const ComputedStyle*,
                                LayoutText*);
  void AppendPreserveWhitespace(const String&,
                                const ComputedStyle*,
                                LayoutText*);
  void AppendPreserveNewline(const String&, const ComputedStyle*, LayoutText*);

  void AppendForcedBreakCollapseWhitespace(LayoutObject*);
  void AppendForcedBreak(LayoutObject*);
  bool AppendTextChunks(const String& string, LayoutText& layout_text);
  void ExitAndEnterSvgTextChunk(LayoutText& layout_text);
  void EnterSvgTextChunk(const ComputedStyle* style);

  void DidAppendTextReusing(const NGInlineItem& item);
  void DidAppendForcedBreak();

  void RemoveTrailingCollapsibleSpaceIfExists();
  void RemoveTrailingCollapsibleSpace(NGInlineItem*);

  void RestoreTrailingCollapsibleSpaceIfRemoved();
  void RestoreTrailingCollapsibleSpace(NGInlineItem*);

  void AppendTextItem(const StringView, LayoutText* layout_object);
  NGInlineItem& AppendTextItem(NGInlineItem::NGInlineItemType type,
                               const StringView,
                               LayoutText* layout_object);
  void AppendEmptyTextItem(LayoutText* layout_object);

  void AppendGeneratedBreakOpportunity(LayoutObject*);

  void Exit(LayoutObject*);

  bool MayBeBidiEnabled() const;

  bool ShouldInsertBreakOpportunityAfterLeadingPreservedSpaces(
      const String&,
      const ComputedStyle&,
      unsigned index = 0) const;
  void InsertBreakOpportunityAfterLeadingPreservedSpaces(const String&,
                                                         const ComputedStyle&,
                                                         LayoutText*,
                                                         unsigned* start);

  friend class NGInlineItemsBuilderTest;
};

template <>
CORE_EXPORT bool
NGInlineItemsBuilderTemplate<NGOffsetMappingBuilder>::AppendTextReusing(
    const NGInlineNodeData&,
    LayoutText*);

template <>
CORE_EXPORT bool NGInlineItemsBuilderTemplate<
    NGOffsetMappingBuilder>::ShouldUpdateLayoutObject() const;

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

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(
    blink::NGInlineItemsBuilder::BidiContext)
WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(
    blink::NGInlineItemsBuilderForOffsetMapping::BidiContext)
WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(
    blink::NGInlineItemsBuilder::BoxInfo)
WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(
    blink::NGInlineItemsBuilderForOffsetMapping::BoxInfo)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_ITEMS_BUILDER_H_
