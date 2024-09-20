// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/layout/inline/inline_node.h"

#include <memory>
#include <numeric>

#include "base/containers/adapters.h"
#include "base/debug/dump_without_crashing.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/core/dom/text_diff_range.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/core/layout/constraint_space.h"
#include "third_party/blink/renderer/core/layout/constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/inline/initial_letter_utils.h"
#include "third_party/blink/renderer/core/layout/inline/inline_break_token.h"
#include "third_party/blink/renderer/core/layout/inline/inline_item.h"
#include "third_party/blink/renderer/core/layout/inline/inline_item_result_ruby_column.h"
#include "third_party/blink/renderer/core/layout/inline/inline_items_builder.h"
#include "third_party/blink/renderer/core/layout/inline/inline_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/inline/inline_text_auto_space.h"
#include "third_party/blink/renderer/core/layout/inline/line_breaker.h"
#include "third_party/blink/renderer/core/layout/inline/line_info.h"
#include "third_party/blink/renderer/core/layout/inline/offset_mapping.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_counter.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_text_combine.h"
#include "third_party/blink/renderer/core/layout/legacy_layout_tree_walking.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/layout/list/layout_inline_list_item.h"
#include "third_party/blink/renderer/core/layout/list/layout_list_item.h"
#include "third_party/blink/renderer/core/layout/list/list_marker.h"
#include "third_party/blink/renderer/core/layout/positioned_float.h"
#include "third_party/blink/renderer/core/layout/space_utils.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/layout/svg/svg_inline_node_data.h"
#include "third_party/blink/renderer/core/layout/svg/svg_text_layout_attributes_builder.h"
#include "third_party/blink/renderer/core/layout/unpositioned_float.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style_base_constants.h"
#include "third_party/blink/renderer/platform/fonts/font_performance.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_shaper.h"
#include "third_party/blink/renderer/platform/fonts/shaping/ng_shape_cache.h"
#include "third_party/blink/renderer/platform/fonts/shaping/run_segmenter.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_spacing.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/heap/collection_support/clear_collection_scope.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/bidi_paragraph.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/text_offset_map.h"

namespace blink {

namespace {

template <typename Span1, typename Span2>
unsigned MismatchInternal(const Span1& span1, const Span2& span2) {
  const auto old_new = base::ranges::mismatch(span1, span2);
  return static_cast<unsigned>(old_new.first - span1.begin());
}

unsigned Mismatch(const String& old_text, const String& new_text) {
  if (old_text.Is8Bit()) {
    const auto old_span8 = old_text.Span8();
    if (new_text.Is8Bit()) {
      return MismatchInternal(old_span8, new_text.Span8());
    }
    return MismatchInternal(old_span8, new_text.Span16());
  }
  const auto old_span16 = old_text.Span16();
  if (new_text.Is8Bit()) {
    return MismatchInternal(old_span16, new_text.Span8());
  }
  return MismatchInternal(old_span16, new_text.Span16());
}

template <typename Span1, typename Span2>
unsigned MismatchFromEnd(const Span1& span1, const Span2& span2) {
  const auto old_new =
      base::ranges::mismatch(base::Reversed(span1), base::Reversed(span2));
  return static_cast<unsigned>(old_new.first - span1.rbegin());
}

unsigned MismatchFromEnd(StringView old_text, StringView new_text) {
  if (old_text.Is8Bit()) {
    if (new_text.Is8Bit()) {
      return MismatchFromEnd(old_text.Span8(), new_text.Span8());
    }
    return MismatchFromEnd(old_text.Span8(), new_text.Span16());
  }
  if (new_text.Is8Bit()) {
    return MismatchFromEnd(old_text.Span16(), new_text.Span8());
  }
  return MismatchFromEnd(old_text.Span16(), new_text.Span16());
}

// Returns sum of |ShapeResult::Width()| in |data.items|. Note: All items
// should be text item other type of items are not allowed.
float CalculateWidthForTextCombine(const InlineItemsData& data) {
  return std::accumulate(
      data.items.begin(), data.items.end(), 0.0f,
      [](float sum, const InlineItem& item) {
        DCHECK(item.Type() == InlineItem::kText ||
               item.Type() == InlineItem::kBidiControl ||
               item.Type() == InlineItem::kControl)
            << item.Type();
        if (auto* const shape_result = item.TextShapeResult())
          return shape_result->Width() + sum;
        return 0.0f;
      });
}

// Estimate the number of InlineItem to minimize the vector expansions.
unsigned EstimateInlineItemsCount(const LayoutBlockFlow& block) {
  unsigned count = 0;
  for (LayoutObject* child = block.FirstChild(); child;
       child = child->NextSibling()) {
    ++count;
  }
  return count * 4;
}

// Estimate the number of units and ranges in OffsetMapping to minimize vector
// and hash map expansions.
unsigned EstimateOffsetMappingItemsCount(const LayoutBlockFlow& block) {
  // Cancels out the factor 4 in EstimateInlineItemsCount() to get the number of
  // LayoutObjects.
  // TODO(layout-dev): Unify the two functions and make them less hacky.
  return EstimateInlineItemsCount(block) / 4;
}

// Wrapper over ShapeText that re-uses existing shape results for items that
// haven't changed.
class ReusingTextShaper final {
  STACK_ALLOCATED();

 public:
  ReusingTextShaper(InlineItemsData* data,
                    const HeapVector<InlineItem>* reusable_items,
                    const bool allow_shape_cache)
      : data_(*data),
        reusable_items_(reusable_items),
        shaper_(data->text_content),
        allow_shape_cache_(allow_shape_cache) {}

  void SetOptions(ShapeOptions options) { options_ = options; }

  const ShapeResult* Shape(const InlineItem& start_item,
                           const Font& font,
                           unsigned end_offset) {
    auto ShapeFunc = [&]() -> const ShapeResult* {
      return ShapeWithoutCache(start_item, font, end_offset);
    };
    if (allow_shape_cache_) {
      DCHECK(RuntimeEnabledFeatures::LayoutNGShapeCacheEnabled());
      return font.GetNGShapeCache().GetOrCreate(
          shaper_.GetText(), start_item.Direction(), ShapeFunc);
    }
    return ShapeFunc();
  }

 private:
  const ShapeResult* ShapeWithoutCache(const InlineItem& start_item,
                                       const Font& font,
                                       unsigned end_offset) {
    const unsigned start_offset = start_item.StartOffset();
    DCHECK_LT(start_offset, end_offset);

    if (!reusable_items_)
      return Reshape(start_item, font, start_offset, end_offset);

    // TODO(yosin): We should support segment text
    if (data_.segments)
      return Reshape(start_item, font, start_offset, end_offset);

    HeapVector<Member<const ShapeResult>> reusable_shape_results =
        CollectReusableShapeResults(start_offset, end_offset, font,
                                    start_item.Direction());
    ClearCollectionScope clear_scope(&reusable_shape_results);

    if (reusable_shape_results.empty())
      return Reshape(start_item, font, start_offset, end_offset);

    ShapeResult* shape_result =
        ShapeResult::CreateEmpty(*reusable_shape_results.front());
    unsigned offset = start_offset;
    for (const ShapeResult* reusable_shape_result : reusable_shape_results) {
      // In case of pre-wrap having break opportunity after leading space,
      // |offset| can be greater than |reusable_shape_result->StartIndex()|.
      // e.g. <div style="white-space:pre">&nbsp; abc</div>, deleteChar(0, 1)
      // See xternal/wpt/editing/run/delete.html?993-993
      if (offset < reusable_shape_result->StartIndex()) {
        AppendShapeResult(*Reshape(start_item, font, offset,
                                   reusable_shape_result->StartIndex()),
                          shape_result);
        offset = shape_result->EndIndex();
        options_.han_kerning_start = false;
      }
      DCHECK_LT(offset, reusable_shape_result->EndIndex());
      DCHECK(shape_result->NumCharacters() == 0 ||
             shape_result->EndIndex() == offset);
      reusable_shape_result->CopyRange(
          offset, std::min(reusable_shape_result->EndIndex(), end_offset),
          shape_result);
      offset = shape_result->EndIndex();
      if (offset == end_offset)
        return shape_result;
    }
    DCHECK_LT(offset, end_offset);
    AppendShapeResult(*Reshape(start_item, font, offset, end_offset),
                      shape_result);
    return shape_result;
  }

  void AppendShapeResult(const ShapeResult& shape_result, ShapeResult* target) {
    DCHECK(target->NumCharacters() == 0 ||
           target->EndIndex() == shape_result.StartIndex());
    shape_result.CopyRange(shape_result.StartIndex(), shape_result.EndIndex(),
                           target);
  }

  HeapVector<Member<const ShapeResult>> CollectReusableShapeResults(
      unsigned start_offset,
      unsigned end_offset,
      const Font& font,
      TextDirection direction) {
    DCHECK_LT(start_offset, end_offset);
    HeapVector<Member<const ShapeResult>> shape_results;
    if (!reusable_items_)
      return shape_results;
    for (auto item = std::lower_bound(
             reusable_items_->begin(), reusable_items_->end(), start_offset,
             [](const InlineItem& item, unsigned offset) {
               return item.EndOffset() <= offset;
             });
         item != reusable_items_->end(); ++item) {
      if (end_offset <= item->StartOffset())
        break;
      if (item->EndOffset() < start_offset)
        continue;
      // This is trying to reuse `ShapeResult` only by the string match. Check
      // if it's reusable for the given style. crbug.com/40879986
      const ShapeResult* const shape_result = item->TextShapeResult();
      if (!shape_result || item->Direction() != direction)
        continue;
      if (RuntimeEnabledFeatures::ReuseShapeResultsByFontsEnabled()) {
        if (item->Style()->GetFont() != font) {
          continue;
        }
      } else {
        if (shape_result->PrimaryFont() != font.PrimaryFont()) {
          continue;
        }
      }
      if (shape_result->IsAppliedSpacing())
        continue;
      shape_results.push_back(shape_result);
    }
    return shape_results;
  }

  const ShapeResult* Reshape(const InlineItem& start_item,
                             const Font& font,
                             unsigned start_offset,
                             unsigned end_offset) {
    DCHECK_LT(start_offset, end_offset);
    const TextDirection direction = start_item.Direction();
    if (data_.segments) {
      return data_.segments->ShapeText(&shaper_, &font, direction, start_offset,
                                       end_offset,
                                       data_.ToItemIndex(start_item), options_);
    }
    RunSegmenter::RunSegmenterRange range =
        start_item.CreateRunSegmenterRange();
    range.end = end_offset;
    return shaper_.Shape(&font, direction, start_offset, end_offset, range,
                         options_);
  }

  InlineItemsData& data_;
  const HeapVector<InlineItem>* const reusable_items_;
  HarfBuzzShaper shaper_;
  ShapeOptions options_;
  const bool allow_shape_cache_;
};

const Font& ScaledFont(const LayoutText& layout_text) {
  if (const auto* svg_text = DynamicTo<LayoutSVGInlineText>(layout_text)) {
    return svg_text->ScaledFont();
  }
  return layout_text.StyleRef().GetFont();
}

// The function is templated to indicate the purpose of collected inlines:
// - With EmptyOffsetMappingBuilder: updating layout;
// - With OffsetMappingBuilder: building offset mapping on clean layout.
//
// This allows code sharing between the two purposes with slightly different
// behaviors. For example, we clear a LayoutObject's need layout flags when
// updating layout, but don't do that when building offset mapping.
//
// There are also performance considerations, since template saves the overhead
// for condition checking and branching.
template <typename ItemsBuilder>
void CollectInlinesInternal(ItemsBuilder* builder,
                            const InlineNodeData* previous_data) {
  LayoutBlockFlow* const block = builder->GetLayoutBlockFlow();
  builder->EnterBlock(block->Style());
  LayoutObject* node = GetLayoutObjectForFirstChildNode(block);

  const LayoutObject* symbol =
      LayoutListItem::FindSymbolMarkerLayoutText(block);
  const LayoutObject* inline_list_item_marker = nullptr;
  while (node) {
    if (auto* counter = DynamicTo<LayoutCounter>(node)) {
      // TODO(crbug.com/561873): PrimaryFont should not be nullptr.
      if (counter->Style()->GetFont().PrimaryFont()) {
        // According to
        // https://w3c.github.io/csswg-drafts/css-counter-styles/#simple-symbolic,
        // disclosure-* should have special rendering paths.
        if (counter->IsDirectionalSymbolMarker()) {
          const String& text = counter->TransformedText();
          // We assume the text representation length for a predefined symbol
          // marker is always 1.
          if (text.length() <= 1) {
            builder->AppendText(counter, previous_data);
            builder->SetIsSymbolMarker();
          } else {
            // The text must be in the following form:
            // Symbol, separator, symbol, separator, symbol, ...
            builder->AppendText(text.Substring(0, 1), counter);
            builder->SetIsSymbolMarker();
            const AtomicString& separator = counter->Separator();
            for (wtf_size_t i = 1; i < text.length();) {
              if (separator.length() > 0) {
                DCHECK_EQ(separator, text.Substring(i, separator.length()));
                builder->AppendText(separator, counter);
                i += separator.length();
                DCHECK_LT(i, text.length());
              }
              builder->AppendText(text.Substring(i, 1), counter);
              builder->SetIsSymbolMarker();
              ++i;
            }
          }
        } else {
          builder->AppendText(counter, previous_data);
        }
      }
      builder->ClearNeedsLayout(counter);
    } else if (auto* layout_text = DynamicTo<LayoutText>(node)) {
      // TODO(crbug.com/561873): PrimaryFont should not be nullptr.
      if (ScaledFont(*layout_text).PrimaryFont()) {
        builder->AppendText(layout_text, previous_data);
        if (symbol == layout_text || inline_list_item_marker == layout_text) {
          builder->SetIsSymbolMarker();
        }
      }
      builder->ClearNeedsLayout(layout_text);
    } else if (node->IsFloating()) {
      builder->AppendFloating(node);
      if (builder->ShouldAbort())
        return;

      builder->ClearInlineFragment(node);
    } else if (node->IsOutOfFlowPositioned()) {
      builder->AppendOutOfFlowPositioned(node);
      if (builder->ShouldAbort())
        return;

      builder->ClearInlineFragment(node);
    } else if (node->IsAtomicInlineLevel()) {
      if (node->IsLayoutOutsideListMarker()) {
        // LayoutListItem produces the 'outside' list marker as an inline
        // block. This is an out-of-flow item whose position is computed
        // automatically.
        builder->AppendOpaque(InlineItem::kListMarker, node);
      } else if (node->IsInitialLetterBox()) [[unlikely]] {
        builder->AppendOpaque(InlineItem::kInitialLetterBox,
                              kObjectReplacementCharacter, node);
        builder->SetHasInititialLetterBox();
      } else {
        // For atomic inlines add a unicode "object replacement character" to
        // signal the presence of a non-text object to the unicode bidi
        // algorithm.
        builder->AppendAtomicInline(node);
      }
      builder->ClearInlineFragment(node);
    } else if (auto* layout_inline = DynamicTo<LayoutInline>(node)) {
      if (auto* inline_list_item = DynamicTo<LayoutInlineListItem>(node)) {
        inline_list_item->UpdateMarkerTextIfNeeded();
        inline_list_item_marker =
            LayoutListItem::FindSymbolMarkerLayoutText(inline_list_item);
      }
      builder->UpdateShouldCreateBoxFragment(layout_inline);

      builder->EnterInline(layout_inline);

      // Traverse to children if they exist.
      if (LayoutObject* child = layout_inline->FirstChild()) {
        node = child;
        continue;
      }

      // An empty inline node.
      builder->ExitInline(layout_inline);
      builder->ClearNeedsLayout(layout_inline);
    } else {
      DCHECK(!node->IsInline());
      builder->AppendBlockInInline(node);
      builder->ClearInlineFragment(node);
    }

    // Find the next sibling, or parent, until we reach |block|.
    while (true) {
      if (LayoutObject* next = node->NextSibling()) {
        node = next;
        break;
      }
      node = GetLayoutObjectForParentNode(node);
      if (node == block || !node) {
        // Set |node| to |nullptr| to break out of the outer loop.
        node = nullptr;
        break;
      }
      DCHECK(node->IsInline());
      builder->ExitInline(node);
      builder->ClearNeedsLayout(node);
    }
  }
  builder->ExitBlock();
}

// Returns whether this text should break shaping. Even within a box, text runs
// that have different shaping properties need to break shaping.
inline bool ShouldBreakShapingBeforeText(const InlineItem& item,
                                         const InlineItem& start_item,
                                         const ComputedStyle& start_style,
                                         const Font& start_font,
                                         TextDirection start_direction) {
  DCHECK_EQ(item.Type(), InlineItem::kText);
  DCHECK(item.Style());
  const ComputedStyle& style = *item.Style();
  if (&style != &start_style) {
    const Font& font = style.GetFont();
    if (&font != &start_font && font != start_font)
      return true;
  }

  // The resolved direction and run segment properties must match to shape
  // across for HarfBuzzShaper.
  return item.Direction() != start_direction ||
         !item.EqualsRunSegment(start_item);
}

// Returns whether the start of this box should break shaping.
inline bool ShouldBreakShapingBeforeBox(const InlineItem& item) {
  DCHECK_EQ(item.Type(), InlineItem::kOpenTag);
  DCHECK(item.Style());
  const ComputedStyle& style = *item.Style();

  // These properties values must break shaping.
  // https://drafts.csswg.org/css-text-3/#boundary-shaping
  if ((style.MayHavePadding() && !style.PaddingInlineStart().IsZero()) ||
      (style.MayHaveMargin() && !style.MarginInlineStart().IsZero()) ||
      style.BorderInlineStartWidth() ||
      style.VerticalAlign() != EVerticalAlign::kBaseline) {
    return true;
  }

  return false;
}

// Returns whether the end of this box should break shaping.
inline bool ShouldBreakShapingAfterBox(const InlineItem& item) {
  DCHECK_EQ(item.Type(), InlineItem::kCloseTag);
  DCHECK(item.Style());
  const ComputedStyle& style = *item.Style();

  // These properties values must break shaping.
  // https://drafts.csswg.org/css-text-3/#boundary-shaping
  if ((style.MayHavePadding() && !style.PaddingInlineEnd().IsZero()) ||
      (style.MayHaveMargin() && !style.MarginInlineEnd().IsZero()) ||
      style.BorderInlineEndWidth() ||
      style.VerticalAlign() != EVerticalAlign::kBaseline) {
    return true;
  }

  return false;
}

inline bool NeedsShaping(const InlineItem& item) {
  if (item.Type() != InlineItem::kText) {
    return false;
  }
  // Text item with length==0 exists to maintain LayoutObject states such as
  // ClearNeedsLayout, but not needed to shape.
  if (!item.Length()) {
    return false;
  }
  if (item.IsUnsafeToReuseShapeResult()) {
    return true;
  }
  const ShapeResult* shape_result = item.TextShapeResult();
  if (!shape_result)
    return true;
  // |StartOffset| is usually safe-to-break, but it is not when we shape across
  // elements and split the |ShapeResult|. Such |ShapeResult| is not safe to
  // reuse.
  DCHECK_EQ(item.StartOffset(), shape_result->StartIndex());
  if (!shape_result->IsStartSafeToBreak())
    return true;
  return false;
}

// Determine if reshape is needed for ::first-line style.
bool FirstLineNeedsReshape(const ComputedStyle& first_line_style,
                           const ComputedStyle& base_style) {
  const Font& base_font = base_style.GetFont();
  const Font& first_line_font = first_line_style.GetFont();
  return &base_font != &first_line_font && base_font != first_line_font;
}

// Make a string to the specified length, either by truncating if longer, or
// appending space characters if shorter.
void TruncateOrPadText(String* text, unsigned length) {
  if (text->length() > length) {
    *text = text->Substring(0, length);
  } else if (text->length() < length) {
    StringBuilder builder;
    builder.ReserveCapacity(length);
    builder.Append(*text);
    while (builder.length() < length)
      builder.Append(kSpaceCharacter);
    *text = builder.ToString();
  }
}

bool SetParagraphTo(const String& text,
                    const ComputedStyle& block_style,
                    BidiParagraph& bidi) {
  if (block_style.GetUnicodeBidi() == UnicodeBidi::kPlaintext) [[unlikely]] {
    return bidi.SetParagraph(text, std::nullopt);
  }
  return bidi.SetParagraph(text, block_style.Direction());
}

}  // namespace

InlineNode::InlineNode(LayoutBlockFlow* block)
    : LayoutInputNode(block, kInline) {
  DCHECK(block);
  DCHECK(block->IsLayoutNGObject());
  if (!block->GetInlineNodeData()) {
    block->ResetInlineNodeData();
  }
}

bool InlineNode::IsPrepareLayoutFinished() const {
  const InlineNodeData* data =
      To<LayoutBlockFlow>(box_.Get())->GetInlineNodeData();
  return data && !data->text_content.IsNull();
}

void InlineNode::PrepareLayoutIfNeeded() const {
  InlineNodeData* previous_data = nullptr;
  LayoutBlockFlow* block_flow = GetLayoutBlockFlow();
  if (IsPrepareLayoutFinished()) {
    if (!block_flow->NeedsCollectInlines())
      return;

    // Note: For "text-combine-upright:all", we use a font calculated from
    // text width, so we can't reuse previous data.
    if (!IsTextCombine()) [[likely]] {
      previous_data = block_flow->TakeInlineNodeData();
    }
    block_flow->ResetInlineNodeData();
  }

  PrepareLayout(previous_data);

  if (previous_data) {
    // previous_data is not used from now on but exists until GC happens, so it
    // is better to eagerly clear HeapVector to improve memory utilization.
    previous_data->items.clear();
  }
}

void InlineNode::PrepareLayout(InlineNodeData* previous_data) const {
  // Scan list of siblings collecting all in-flow non-atomic inlines. A single
  // InlineNode represent a collection of adjacent non-atomic inlines.
  InlineNodeData* data = MutableData();
  DCHECK(data);
  CollectInlines(data, previous_data);
  SegmentText(data, previous_data);
  ShapeTextIncludingFirstLine(
      data, previous_data ? &previous_data->text_content : nullptr, nullptr);

  AssociateItemsWithInlines(data);
  DCHECK_EQ(data, MutableData());

  LayoutBlockFlow* block_flow = GetLayoutBlockFlow();
  block_flow->ClearNeedsCollectInlines();

  if (IsTextCombine()) [[unlikely]] {
    // To use |LayoutTextCombine::UsersScaleX()| in |FragmentItemsBuilder|,
    // we adjust font here instead in |Layout()|,
    AdjustFontForTextCombineUprightAll();
  }

#if EXPENSIVE_DCHECKS_ARE_ON()
  // ComputeOffsetMappingIfNeeded() runs some integrity checks as part of
  // creating offset mapping. Run the check, and discard the result.
  DCHECK(!data->offset_mapping);
  ComputeOffsetMappingIfNeeded();
  DCHECK(data->offset_mapping);
  data->offset_mapping.Clear();
#endif
}

// Building |InlineNodeData| for |LayoutText::SetTextWithOffset()| with
// reusing data.
class InlineNodeDataEditor final {
  STACK_ALLOCATED();

 public:
  explicit InlineNodeDataEditor(const LayoutText& layout_text)
      : block_flow_(layout_text.FragmentItemsContainer()),
        layout_text_(layout_text) {
    DCHECK(layout_text_.HasValidInlineItems());
  }
  InlineNodeDataEditor(const InlineNodeDataEditor&) = delete;
  InlineNodeDataEditor& operator=(const InlineNodeDataEditor&) = delete;

  LayoutBlockFlow* GetLayoutBlockFlow() const { return block_flow_; }

  // Note: We can't use |Position| for |layout_text_.GetNode()| because |Text|
  // node is already changed.
  InlineNodeData* Prepare() {
    if (!block_flow_ || block_flow_->NeedsCollectInlines() ||
        block_flow_->NeedsLayout() ||
        block_flow_->GetDocument().NeedsLayoutTreeUpdate() ||
        !block_flow_->GetInlineNodeData() ||
        block_flow_->GetInlineNodeData()->text_content.IsNull() ||
        block_flow_->GetInlineNodeData()->items.empty()) {
      return nullptr;
    }

    // For "text-combine-upright:all", we choose font to fit layout result in
    // 1em, so font can be different than original font.
    if (IsA<LayoutTextCombine>(block_flow_)) [[unlikely]] {
      return nullptr;
    }

    // Because of current text content has secured text, e.g. whole text is
    // "***", all characters including collapsed white spaces are marker, and
    // new text is original text, we can't reuse shape result.
    if (layout_text_.StyleRef().TextSecurity() != ETextSecurity::kNone)
      return nullptr;

    // It is hard to figure differences of bidi control codes before/after
    // editing. See http://crbug.com/1039143
    if (layout_text_.HasBidiControlInlineItems())
      return nullptr;

    // Note: We should compute offset mapping before calling
    // |LayoutBlockFlow::TakeInlineNodeData()|
    const OffsetMapping* const offset_mapping =
        InlineNode::GetOffsetMapping(block_flow_);
    DCHECK(offset_mapping);
    if (data_) {
      // data_ is not used from now on but exists until GC happens, so it is
      // better to eagerly clear HeapVector to improve memory utilization.
      data_->items.clear();
    }
    data_ = block_flow_->TakeInlineNodeData();
    return data_;
  }

  void Run() {
    const InlineNodeData& new_data = *block_flow_->GetInlineNodeData();
    const String& old_text = data_->text_content;
    const String& new_text = new_data.text_content;
    const auto [start_offset, end_match_length] =
        MatchedLengths(old_text, new_text);
    const unsigned old_length = old_text.length();
    const unsigned new_length = new_text.length();
    DCHECK_LE(start_offset, old_length - end_match_length);
    DCHECK_LE(start_offset, new_length - end_match_length);
    const unsigned end_offset = old_length - end_match_length;
    DCHECK_LE(start_offset, end_offset);
    HeapVector<InlineItem> items;
    ClearCollectionScope clear_scope(&items);

    // +3 for before and after replaced text.
    items.ReserveInitialCapacity(data_->items.size() + 3);

    // Copy items before replaced range
    auto end = data_->items.end();
    auto it = data_->items.begin();
    for (; it != end && it->end_offset_ < start_offset; ++it) {
      CHECK(it != data_->items.end(), base::NotFatalUntil::M130);
      items.push_back(*it);
    }

    while (it != end) {
      // Copy part of item before replaced range.
      if (it->start_offset_ < start_offset) {
        const InlineItem& new_item = CopyItemBefore(*it, start_offset);
        items.push_back(new_item);
        if (new_item.EndOffset() < start_offset) {
          items.push_back(
              InlineItem(*it, new_item.EndOffset(), start_offset, nullptr));
        }
      }

      // Skip items in replaced range.
      while (it != end && it->end_offset_ < end_offset)
        ++it;

      if (it == end)
        break;

      // Inserted text
      const int diff = new_length - old_length;
      const unsigned inserted_end = AdjustOffset(end_offset, diff);
      if (start_offset < inserted_end)
        items.push_back(InlineItem(*it, start_offset, inserted_end, nullptr));

      // Copy part of item after replaced range.
      if (end_offset < it->end_offset_) {
        const InlineItem& new_item = CopyItemAfter(*it, end_offset);
        if (end_offset < new_item.StartOffset()) {
          items.push_back(
              InlineItem(*it, end_offset, new_item.StartOffset(), nullptr));
          ShiftItem(&items.back(), diff);
        }
        items.push_back(new_item);
        ShiftItem(&items.back(), diff);
      }

      // Copy items after replaced range
      ++it;
      while (it != end) {
        DCHECK_LE(end_offset, it->start_offset_);
        items.push_back(*it);
        ShiftItem(&items.back(), diff);
        ++it;
      }
      break;
    }

    if (items.empty()) {
      items.push_back(InlineItem(data_->items.front(), 0,
                                 new_data.text_content.length(), nullptr));
    } else if (items.back().end_offset_ < new_data.text_content.length()) {
      items.push_back(InlineItem(data_->items.back(), items.back().end_offset_,
                                 new_data.text_content.length(), nullptr));
    }

    VerifyItems(items);
    // eagerly clear HeapVector to improve memory utilization.
    data_->items.clear();
    data_->items = std::move(items);
    data_->text_content = new_data.text_content;
  }

 private:
  // Find the number of characters that match in the two strings, from the start
  // and from the end.
  std::pair<unsigned, unsigned> MatchedLengths(const String& old_text,
                                               const String& new_text) const {
    // Find how many characters match from the start.
    const unsigned start_match_length = Mismatch(old_text, new_text);

    // Find from the end, excluding the `start_match_length` characters.
    const unsigned old_length = old_text.length();
    const unsigned new_length = new_text.length();
    const unsigned max_end_length = std::min(old_length - start_match_length,
                                             new_length - start_match_length);
    const unsigned end_match_length =
        MismatchFromEnd(StringView(old_text, old_length - max_end_length),
                        StringView(new_text, new_length - max_end_length));
    DCHECK_LE(start_match_length, old_length - end_match_length);
    DCHECK_LE(start_match_length, new_length - end_match_length);
    return {start_match_length, end_match_length};
  }

  static unsigned AdjustOffset(unsigned offset, int delta) {
    if (delta > 0)
      return offset + delta;
    return offset - (-delta);
  }

  // Returns copy of |item| after |start_offset| (inclusive).
  InlineItem CopyItemAfter(const InlineItem& item,
                           unsigned start_offset) const {
    DCHECK_LE(item.start_offset_, start_offset);
    DCHECK_LT(start_offset, item.end_offset_);
    const unsigned safe_start_offset = GetFirstSafeToReuse(item, start_offset);
    const unsigned end_offset = item.end_offset_;
    if (end_offset == safe_start_offset)
      return InlineItem(item, start_offset, end_offset, nullptr);
    // To handle kerning, e.g. inserting "A" before "V", and joining in Arabic,
    // we should not reuse first glyph.
    // See http://crbug.com/1199331
    DCHECK_LT(safe_start_offset, item.end_offset_);
    return InlineItem(
        item, safe_start_offset, end_offset,
        item.shape_result_->SubRange(safe_start_offset, end_offset));
  }

  // Returns copy of |item| before |end_offset| (exclusive).
  InlineItem CopyItemBefore(const InlineItem& item, unsigned end_offset) const {
    DCHECK_LT(item.start_offset_, end_offset);
    DCHECK_LE(end_offset, item.end_offset_);
    const unsigned safe_end_offset = GetLastSafeToReuse(item, end_offset);
    const unsigned start_offset = item.start_offset_;
    // Nothing to reuse if no characters are safe to reuse.
    if (safe_end_offset <= start_offset)
      return InlineItem(item, start_offset, end_offset, nullptr);
    // To handle kerning, e.g. "AV", we should not reuse last glyph.
    // See http://crbug.com/1129710
    DCHECK_LT(safe_end_offset, item.end_offset_);
    return InlineItem(
        item, start_offset, safe_end_offset,
        item.shape_result_->SubRange(start_offset, safe_end_offset));
  }

  // See also |GetLastSafeToReuse()|.
  unsigned GetFirstSafeToReuse(const InlineItem& item,
                               unsigned start_offset) const {
    DCHECK_LE(item.start_offset_, start_offset);
    DCHECK_LE(start_offset, item.end_offset_);
    const unsigned end_offset = item.end_offset_;
    // TODO(yosin): It is better to utilize OpenType |usMaxContext|.
    // For font having "fi", |usMaxContext = 2".
    const unsigned max_context = 2;
    const unsigned skip = max_context - 1;
    if (!item.shape_result_ || item.shape_result_->IsAppliedSpacing() ||
        start_offset + skip >= end_offset)
      return end_offset;
    item.shape_result_->EnsurePositionData();
    // Note: Because |CachedNextSafeToBreakOffset()| assumes |start_offset|
    // is always safe to break offset, we try to search after |start_offset|.
    return item.shape_result_->CachedNextSafeToBreakOffset(start_offset + skip);
  }

  // See also |GetFirstSafeToReuse()|.
  unsigned GetLastSafeToReuse(const InlineItem& item,
                              unsigned end_offset) const {
    DCHECK_LT(item.start_offset_, end_offset);
    DCHECK_LE(end_offset, item.end_offset_);
    const unsigned start_offset = item.start_offset_;
    // TODO(yosin): It is better to utilize OpenType |usMaxContext|.
    // For font having "fi", usMaxContext = 2.
    // For Emoji with ZWJ, usMaxContext = 10. (http://crbug.com/1213235)
    const unsigned max_context = data_->text_content.Is8Bit() ? 2 : 10;
    const unsigned skip = max_context - 1;
    if (!item.shape_result_ || item.shape_result_->IsAppliedSpacing() ||
        end_offset <= start_offset + skip)
      return start_offset;
    item.shape_result_->EnsurePositionData();
    // TODO(yosin): It is better to utilize OpenType |usMaxContext|.
    // Note: Because |CachedPreviousSafeToBreakOffset()| assumes |end_offset|
    // is always safe to break offset, we try to search before |end_offset|.
    return item.shape_result_->CachedPreviousSafeToBreakOffset(end_offset -
                                                               skip);
  }

  static void ShiftItem(InlineItem* item, int delta) {
    if (delta == 0)
      return;
    item->start_offset_ = AdjustOffset(item->start_offset_, delta);
    item->end_offset_ = AdjustOffset(item->end_offset_, delta);
    if (!item->shape_result_)
      return;
    item->shape_result_ =
        item->shape_result_->CopyAdjustedOffset(item->start_offset_);
  }

  void VerifyItems(const HeapVector<InlineItem>& items) const {
#if DCHECK_IS_ON()
    if (items.empty())
      return;
    unsigned last_offset = items.front().start_offset_;
    for (const InlineItem& item : items) {
      DCHECK_LE(item.start_offset_, item.end_offset_);
      DCHECK_EQ(last_offset, item.start_offset_);
      last_offset = item.end_offset_;
      if (!item.shape_result_)
        continue;
      DCHECK_LT(item.start_offset_, item.end_offset_);
      DCHECK_EQ(item.shape_result_->StartIndex(), item.start_offset_);
      DCHECK_EQ(item.shape_result_->EndIndex(), item.end_offset_);
    }
    DCHECK_EQ(last_offset,
              block_flow_->GetInlineNodeData()->text_content.length());
#endif
  }

  InlineNodeData* data_ = nullptr;
  LayoutBlockFlow* const block_flow_;
  const LayoutText& layout_text_;
};

// static
bool InlineNode::SetTextWithOffset(LayoutText* layout_text,
                                   String new_text,
                                   const TextDiffRange& diff) {
  if (!layout_text->HasValidInlineItems() ||
      !layout_text->IsInLayoutNGInlineFormattingContext())
    return false;
  const String old_text = layout_text->TransformedText();
  if (diff.offset == 0 && diff.old_size == old_text.length()) {
    // We'll run collect inline items since whole text of |layout_text| is
    // changed.
    return false;
  }

  InlineNodeDataEditor editor(*layout_text);
  InlineNodeData* const previous_data = editor.Prepare();
  if (!previous_data)
    return false;

  // This function runs outside of the layout phase. Prevent purging font cache
  // while shaping.
  FontCachePurgePreventer font_cache_purge_preventer;

  TextOffsetMap offset_map;
  new_text = layout_text->TransformAndSecureText(new_text, offset_map);
  if (!offset_map.IsEmpty()) {
    return false;
  }
  layout_text->SetTextInternal(new_text);
  layout_text->ClearHasNoControlItems();
  layout_text->ClearHasVariableLengthTransform();

  InlineNode node(editor.GetLayoutBlockFlow());
  InlineNodeData* data = node.MutableData();
  data->items.reserve(previous_data->items.size());
  InlineItemsBuilder builder(
      editor.GetLayoutBlockFlow(), &data->items,
      previous_data ? previous_data->text_content : String());
  // TODO(yosin): We should reuse before/after |layout_text| during collecting
  // inline items.
  layout_text->ClearInlineItems();
  CollectInlinesInternal(&builder, previous_data);
  builder.DidFinishCollectInlines(data);
  // Relocates |ShapeResult| in |previous_data| after |offset|+|length|
  editor.Run();
  node.SegmentText(data, nullptr);
  node.ShapeTextIncludingFirstLine(data, &previous_data->text_content,
                                   &previous_data->items);
  node.AssociateItemsWithInlines(data);
  return true;
}

const InlineNodeData& InlineNode::EnsureData() const {
  PrepareLayoutIfNeeded();
  return Data();
}

const OffsetMapping* InlineNode::ComputeOffsetMappingIfNeeded() const {
#if DCHECK_IS_ON()
  DCHECK(!GetLayoutBlockFlow()->GetDocument().NeedsLayoutTreeUpdate() ||
         GetLayoutBlockFlow()->IsInDetachedNonDomTree());
#endif

  InlineNodeData* data = MutableData();
  if (!data->offset_mapping) {
    DCHECK(!data->text_content.IsNull());
    ComputeOffsetMapping(GetLayoutBlockFlow(), data);
  }

  return data->offset_mapping.Get();
}

void InlineNode::ComputeOffsetMapping(LayoutBlockFlow* layout_block_flow,
                                      InlineNodeData* data) {
#if DCHECK_IS_ON()
  DCHECK(!data->offset_mapping);
  DCHECK(!layout_block_flow->GetDocument().NeedsLayoutTreeUpdate() ||
         layout_block_flow->IsInDetachedNonDomTree());
#endif

  const SvgTextChunkOffsets* chunk_offsets = nullptr;
  if (data->svg_node_data_ && data->svg_node_data_->chunk_offsets.size() > 0)
    chunk_offsets = &data->svg_node_data_->chunk_offsets;

  // TODO(xiaochengh): ComputeOffsetMappingIfNeeded() discards the
  // InlineItems and text content built by |builder|, because they are
  // already there in InlineNodeData. For efficiency, we should make
  // |builder| not construct items and text content.
  HeapVector<InlineItem> items;
  ClearCollectionScope<HeapVector<InlineItem>> clear_scope(&items);
  items.reserve(EstimateInlineItemsCount(*layout_block_flow));
  InlineItemsBuilderForOffsetMapping builder(layout_block_flow, &items,
                                             data->text_content, chunk_offsets);
  builder.GetOffsetMappingBuilder().ReserveCapacity(
      EstimateOffsetMappingItemsCount(*layout_block_flow));
  CollectInlinesInternal(&builder, nullptr);

  // For non-NG object, we need the text, and also the inline items to resolve
  // bidi levels. Otherwise |data| already has the text from the pre-layout
  // phase, check they match.
  if (data->text_content.IsNull()) {
    DCHECK(!layout_block_flow->IsLayoutNGObject());
    data->text_content = builder.ToString();
  } else {
    DCHECK(layout_block_flow->IsLayoutNGObject());
  }

  // TODO(xiaochengh): This doesn't compute offset mapping correctly when
  // text-transform CSS property changes text length.
  OffsetMappingBuilder& mapping_builder = builder.GetOffsetMappingBuilder();
  data->offset_mapping = nullptr;
  if (mapping_builder.SetDestinationString(data->text_content)) {
    data->offset_mapping = mapping_builder.Build();
    DCHECK(data->offset_mapping);
  }
}

const OffsetMapping* InlineNode::GetOffsetMapping(
    LayoutBlockFlow* layout_block_flow) {
  DCHECK(!layout_block_flow->GetDocument().NeedsLayoutTreeUpdate());

  if (layout_block_flow->NeedsLayout()) [[unlikely]] {
    // TODO(kojii): This shouldn't happen, but is not easy to fix all cases.
    // Return nullptr so that callers can chose to fail gracefully, or
    // null-deref. crbug.com/946004
    return nullptr;
  }

  InlineNode node(layout_block_flow);
  CHECK(node.IsPrepareLayoutFinished());
  return node.ComputeOffsetMappingIfNeeded();
}

// Depth-first-scan of all LayoutInline and LayoutText nodes that make up this
// InlineNode object. Collects LayoutText items, merging them up into the
// parent LayoutInline where possible, and joining all text content in a single
// string to allow bidi resolution and shaping of the entire block.
void InlineNode::CollectInlines(InlineNodeData* data,
                                InlineNodeData* previous_data) const {
  DCHECK(data->text_content.IsNull());
  DCHECK(data->items.empty());
  LayoutBlockFlow* block = GetLayoutBlockFlow();
  block->WillCollectInlines();

  const SvgTextChunkOffsets* chunk_offsets = nullptr;
  if (block->IsSVGText()) {
    // SVG <text> doesn't support reusing the previous result now.
    previous_data = nullptr;
    data->svg_node_data_ = nullptr;
    // We don't need to find text chunks if the IFC has only 0-1 character
    // because of no Bidi reordering and no ligatures.
    // This is an optimization for perf_tests/svg/France.html.
    const auto* layout_text = DynamicTo<LayoutText>(block->FirstChild());
    bool empty_or_one_char =
        !block->FirstChild() || (layout_text && !layout_text->NextSibling() &&
                                 layout_text->TransformedTextLength() <= 1);
    if (!empty_or_one_char)
      chunk_offsets = FindSvgTextChunks(*block, *data);
  }

  data->items.reserve(EstimateInlineItemsCount(*block));
  InlineItemsBuilder builder(
      block, &data->items,
      previous_data ? previous_data->text_content : String(), chunk_offsets);
  CollectInlinesInternal(&builder, previous_data);
  if (block->IsSVGText() && !data->svg_node_data_) {
    SvgTextLayoutAttributesBuilder svg_attr_builder(*this);
    svg_attr_builder.Build(builder.ToString(), data->items);
    data->svg_node_data_ = svg_attr_builder.CreateSvgInlineNodeData();
  }
  builder.DidFinishCollectInlines(data);

  if (builder.HasUnicodeBidiPlainText()) [[unlikely]] {
    UseCounter::Count(GetDocument(), WebFeature::kUnicodeBidiPlainText);
  }
}

const SvgTextChunkOffsets* InlineNode::FindSvgTextChunks(
    LayoutBlockFlow& block,
    InlineNodeData& data) const {
  TRACE_EVENT0("blink", "InlineNode::FindSvgTextChunks");
  // Build InlineItems and OffsetMapping first.  They are used only by
  // SVGTextLayoutAttributesBuilder, and are discarded because they might
  // be different from final ones.
  HeapVector<InlineItem> items;
  ClearCollectionScope<HeapVector<InlineItem>> clear_scope(&items);
  items.reserve(EstimateInlineItemsCount(block));
  InlineItemsBuilderForOffsetMapping items_builder(&block, &items);
  OffsetMappingBuilder& mapping_builder =
      items_builder.GetOffsetMappingBuilder();
  mapping_builder.ReserveCapacity(EstimateOffsetMappingItemsCount(block));
  CollectInlinesInternal(&items_builder, nullptr);
  String ifc_text_content = items_builder.ToString();

  SvgTextLayoutAttributesBuilder svg_attr_builder(*this);
  svg_attr_builder.Build(ifc_text_content, items);
  data.svg_node_data_ = svg_attr_builder.CreateSvgInlineNodeData();

  // Compute DOM offsets of text chunks.
  CHECK(mapping_builder.SetDestinationString(ifc_text_content));
  OffsetMapping* mapping = mapping_builder.Build();
  StringView ifc_text_view(ifc_text_content);
  for (wtf_size_t i = 0; i < data.svg_node_data_->character_data_list.size();
       ++i) {
    const std::pair<unsigned, SvgCharacterData>& char_data =
        data.svg_node_data_->character_data_list[i];
    if (!char_data.second.anchored_chunk)
      continue;
    unsigned addressable_offset = char_data.first;
    if (addressable_offset == 0u)
      continue;
    unsigned text_content_offset = svg_attr_builder.IfcTextContentOffsetAt(i);
    const auto* unit = mapping->GetLastMappingUnit(text_content_offset);
    DCHECK(unit);
    auto result = data.svg_node_data_->chunk_offsets.insert(
        To<LayoutText>(&unit->GetLayoutObject()), Vector<unsigned>());
    result.stored_value->value.push_back(
        unit->ConvertTextContentToFirstDOMOffset(text_content_offset));
  }
  return data.svg_node_data_->chunk_offsets.size() > 0
             ? &data.svg_node_data_->chunk_offsets
             : nullptr;
}

void InlineNode::SegmentText(InlineNodeData* data,
                             InlineNodeData* previous_data) const {
  SegmentBidiRuns(data);
  SegmentScriptRuns(data, previous_data);
  SegmentFontOrientation(data);
  if (data->segments)
    data->segments->ComputeItemIndex(data->items);
}

// Segment InlineItem by script, Emoji, and orientation using RunSegmenter.
void InlineNode::SegmentScriptRuns(InlineNodeData* data,
                                   InlineNodeData* previous_data) const {
  String& text_content = data->text_content;
  if (text_content.empty()) {
    data->segments = nullptr;
    return;
  }

  if (previous_data && text_content == previous_data->text_content) {
    if (!previous_data->segments) {
      const auto it = base::ranges::find_if(
          previous_data->items,
          [](const auto& item) { return item.Type() == InlineItem::kText; });
      if (it != previous_data->items.end()) {
        unsigned previous_packed_segment = it->segment_data_;
        for (auto& item : data->items) {
          if (item.Type() == InlineItem::kText) {
            item.segment_data_ = previous_packed_segment;
          }
        }
        data->segments = nullptr;
        return;
      }
    } else if (IsHorizontalTypographicMode()) {
      // We can reuse InlineNodeData::segments only in horizontal writing modes
      // because we might update it by SegmentFontOrientation() in vertical
      // writing modes.
      data->segments = std::move(previous_data->segments);
      return;
    }
  }

  if ((text_content.Is8Bit() || !data->HasNonOrc16BitCharacters()) &&
      !data->is_bidi_enabled_) {
    if (data->items.size()) {
      RunSegmenter::RunSegmenterRange range = {
          0u, data->text_content.length(), USCRIPT_LATIN,
          OrientationIterator::kOrientationKeep, FontFallbackPriority::kText};
      InlineItem::SetSegmentData(range, &data->items);
    }
    data->segments = nullptr;
    return;
  }

  // Segment by script and Emoji.
  // Orientation is segmented separately, because it may vary by items.
  text_content.Ensure16Bit();
  RunSegmenter segmenter(text_content.Characters16(), text_content.length(),
                         FontOrientation::kHorizontal);

  RunSegmenter::RunSegmenterRange range;
  bool consumed = segmenter.Consume(&range);
  DCHECK(consumed);
  if (range.end == text_content.length()) {
    InlineItem::SetSegmentData(range, &data->items);
    data->segments = nullptr;
    return;
  }

  // This node has multiple segments.
  if (!data->segments)
    data->segments = std::make_unique<InlineItemSegments>();
  data->segments->ComputeSegments(&segmenter, &range);
  DCHECK_EQ(range.end, text_content.length());
}

void InlineNode::SegmentFontOrientation(InlineNodeData* data) const {
  // Segment by orientation, only if vertical writing mode and items with
  // 'text-orientation: mixed'.
  if (IsHorizontalTypographicMode()) {
    return;
  }

  HeapVector<InlineItem>& items = data->items;
  if (items.empty())
    return;
  String& text_content = data->text_content;
  text_content.Ensure16Bit();

  // If we don't have |InlineItemSegments| yet, create a segment for the
  // entire content.
  const unsigned capacity = items.size() + text_content.length() / 10;
  InlineItemSegments* segments = data->segments.get();
  if (segments) {
    DCHECK(!data->segments->IsEmpty());
    data->segments->ReserveCapacity(capacity);
    DCHECK_EQ(text_content.length(), data->segments->EndOffset());
  }
  unsigned segment_index = 0;

  for (const InlineItem& item : items) {
    if (item.Type() == InlineItem::kText && item.Length() &&
        item.Style()->GetFont().GetFontDescription().Orientation() ==
            FontOrientation::kVerticalMixed) {
      if (!segments) {
        data->segments = std::make_unique<InlineItemSegments>();
        segments = data->segments.get();
        segments->ReserveCapacity(capacity);
        segments->Append(text_content.length(), item);
        DCHECK_EQ(text_content.length(), data->segments->EndOffset());
      }
      segment_index = segments->AppendMixedFontOrientation(
          text_content, item.StartOffset(), item.EndOffset(), segment_index);
    }
  }
}

// Segment bidi runs by resolving bidi embedding levels.
// http://unicode.org/reports/tr9/#Resolving_Embedding_Levels
void InlineNode::SegmentBidiRuns(InlineNodeData* data) const {
  if (!data->is_bidi_enabled_) {
    data->SetBaseDirection(TextDirection::kLtr);
    return;
  }

  BidiParagraph bidi;
  data->text_content.Ensure16Bit();
  if (!SetParagraphTo(data->text_content, Style(), bidi)) {
    // On failure, give up bidi resolving and reordering.
    data->is_bidi_enabled_ = false;
    data->SetBaseDirection(TextDirection::kLtr);
    return;
  }

  data->SetBaseDirection(bidi.BaseDirection());

  if (bidi.IsUnidirectional() && IsLtr(bidi.BaseDirection())) {
    // All runs are LTR, no need to reorder.
    data->is_bidi_enabled_ = false;
    return;
  }

  HeapVector<InlineItem>& items = data->items;
  unsigned item_index = 0;
  for (unsigned start = 0; start < data->text_content.length();) {
    UBiDiLevel level;
    unsigned end = bidi.GetLogicalRun(start, &level);
    DCHECK_EQ(items[item_index].start_offset_, start);
    item_index = InlineItem::SetBidiLevel(items, item_index, end, level);
    start = end;
  }
#if DCHECK_IS_ON()
  // Check all items have bidi levels, except trailing non-length items.
  // Items that do not create break opportunities such as kOutOfFlowPositioned
  // do not have corresponding characters, and that they do not have bidi level
  // assigned.
  while (item_index < items.size() && !items[item_index].Length())
    item_index++;
  DCHECK_EQ(item_index, items.size());
#endif
}

bool InlineNode::IsNGShapeCacheAllowed(
    const String& text_content,
    const Font* override_font,
    const HeapVector<InlineItem>& items,
    ShapeResultSpacing<String>& spacing) const {
  if (!RuntimeEnabledFeatures::LayoutNGShapeCacheEnabled()) {
    return false;
  }
  // For consistency with similar usages of ShapeCache (e.g. canvas) and in
  // order to avoid caching bugs (e.g. with scripts having Arabic joining)
  // NGShapeCache is only enabled when the IFC is made of a single text item. To
  // be efficient, NGShapeCache only stores entries for short strings and
  // without memory copy, so don't allow it if the text item is too long or if
  // the start/end offsets match a substring. Don't allow it either if a call to
  // ApplySpacing is needed to avoid a costly copy of the ShapeResult in the
  // loop below. Finally, check that the font meet requirements on the font
  // family list to avoid expensive hash key calculations.
  if (items.size() != 1) {
    return false;
  }
  if (text_content.length() > NGShapeCache::kMaxTextLengthOfEntries) {
    return false;
  }
  const InlineItem& single_item = items[0];
  if (!(single_item.Type() == InlineItem::kText &&
        single_item.StartOffset() == 0 &&
        single_item.EndOffset() == text_content.length())) {
    return false;
  }
  const Font& font =
      override_font ? *override_font : single_item.FontWithSvgScaling();
  return !spacing.SetSpacing(font.GetFontDescription());
}

void InlineNode::ShapeText(InlineItemsData* data,
                           const String* previous_text,
                           const HeapVector<InlineItem>* previous_items,
                           const Font* override_font) const {
  TRACE_EVENT0("fonts", "InlineNode::ShapeText");
  base::ScopedClosureRunner scoped_closure_runner(WTF::BindOnce(
      [](base::ElapsedTimer timer, Document* document) {
        if (document) {
          document->MaybeRecordShapeTextElapsedTime(timer.Elapsed());
        }
      },
      base::ElapsedTimer(),
      WrapWeakPersistent(GetLayoutBox() ? &GetLayoutBox()->GetDocument()
                                        : nullptr)));

  const String& text_content = data->text_content;
  HeapVector<InlineItem>* items = &data->items;

  ShapeResultSpacing<String> spacing(text_content, IsSvgText());
  InlineTextAutoSpace auto_space(*data);

  const bool allow_shape_cache =
      IsNGShapeCacheAllowed(text_content, override_font, *items, spacing) &&
      !auto_space.MayApply();

  // Provide full context of the entire node to the shaper.
  ReusingTextShaper shaper(data, previous_items, allow_shape_cache);
  bool is_next_start_of_paragraph = true;

  DCHECK(!data->segments ||
         data->segments->EndOffset() == text_content.length());

  for (unsigned index = 0; index < items->size();) {
    InlineItem& start_item = (*items)[index];
    if (start_item.Type() != InlineItem::kText || !start_item.Length()) {
      index++;
      is_next_start_of_paragraph = start_item.IsForcedLineBreak();
      continue;
    }

    const ComputedStyle& start_style = *start_item.Style();
    const Font& font =
        override_font ? *override_font : start_item.FontWithSvgScaling();
#if DCHECK_IS_ON()
    if (!IsTextCombine()) {
      DCHECK(!override_font);
    } else {
      DCHECK_EQ(font.GetFontDescription().Orientation(),
                FontOrientation::kHorizontal);
      LayoutTextCombine::AssertStyleIsValid(start_style);
      DCHECK(!override_font ||
             font.GetFontDescription().WidthVariant() != kRegularWidth);
    }
#endif
    shaper.SetOptions({
        .is_line_start = is_next_start_of_paragraph,
        .han_kerning_start =
            is_next_start_of_paragraph &&
            ShouldTrimStartOfParagraph(
                font.GetFontDescription().GetTextSpacingTrim()) &&
            Character::MaybeHanKerningOpen(
                text_content[start_item.StartOffset()]),
    });
    is_next_start_of_paragraph = false;
    TextDirection direction = start_item.Direction();
    unsigned end_index = index + 1;
    unsigned end_offset = start_item.EndOffset();

    // Symbol marker is painted as graphics. Create a ShapeResult of space
    // glyphs with the desired size to make it less special for line breaker.
    if (start_item.IsSymbolMarker()) [[unlikely]] {
      LayoutUnit symbol_width = ListMarker::WidthOfSymbol(
          start_style,
          LayoutCounter::ListStyle(start_item.GetLayoutObject(), start_style));
      DCHECK_GE(symbol_width, 0);
      start_item.shape_result_ = ShapeResult::CreateForSpaces(
          &font, direction, start_item.StartOffset(), start_item.Length(),
          symbol_width);
      index++;
      continue;
    }

    // Scan forward until an item is encountered that should trigger a shaping
    // break. This ensures that adjacent text items are shaped together whenever
    // possible as this is required for accurate cross-element shaping.
    unsigned num_text_items = 1;
    for (; end_index < items->size(); end_index++) {
      const InlineItem& item = (*items)[end_index];

      if (item.Type() == InlineItem::kControl) {
        // Do not shape across control characters (line breaks, zero width
        // spaces, etc).
        break;
      }
      if (item.Type() == InlineItem::kText) {
        if (!item.Length())
          continue;
        if (item.TextType() == TextItemType::kSymbolMarker) {
          break;
        }
        if (ShouldBreakShapingBeforeText(item, start_item, start_style, font,
                                         direction)) {
          break;
        }
        // Break shaping at ZWNJ so that it prevents kerning. ZWNJ is always at
        // the beginning of an item for this purpose; see InlineItemsBuilder.
        if (text_content[item.StartOffset()] == kZeroWidthNonJoinerCharacter)
          break;
        end_offset = item.EndOffset();
        num_text_items++;
      } else if (item.Type() == InlineItem::kOpenTag) {
        if (ShouldBreakShapingBeforeBox(item))
          break;
        // Should not have any characters to be opaque to shaping.
        DCHECK_EQ(0u, item.Length());
      } else if (item.Type() == InlineItem::kCloseTag) {
        if (ShouldBreakShapingAfterBox(item))
          break;
        // Should not have any characters to be opaque to shaping.
        DCHECK_EQ(0u, item.Length());
      } else {
        break;
      }
    }

    // Shaping a single item. Skip if the existing results remain valid.
    if (previous_text && end_offset == start_item.EndOffset() &&
        !NeedsShaping(start_item)) {
      if (!IsTextCombine()) [[likely]] {
        DCHECK_EQ(start_item.StartOffset(),
                  start_item.TextShapeResult()->StartIndex());
        DCHECK_EQ(start_item.EndOffset(),
                  start_item.TextShapeResult()->EndIndex());
        index++;
        continue;
      }
    }

    // Results may only be reused if all items in the range remain valid.
    if (previous_text) {
      bool has_valid_shape_results = true;
      for (unsigned item_index = index; item_index < end_index; item_index++) {
        if (NeedsShaping((*items)[item_index])) {
          has_valid_shape_results = false;
          break;
        }
      }

      // When shaping across multiple items checking whether the individual
      // items has valid shape results isn't sufficient as items may have been
      // re-ordered or removed.
      // TODO(layout-dev): It would probably be faster to check for removed or
      // moved items but for now comparing the string itself will do.
      unsigned text_start = start_item.StartOffset();
      DCHECK_GE(end_offset, text_start);
      unsigned text_length = end_offset - text_start;
      if (has_valid_shape_results && previous_text &&
          end_offset <= previous_text->length() &&
          StringView(text_content, text_start, text_length) ==
              StringView(*previous_text, text_start, text_length)) {
        index = end_index;
        continue;
      }
    }

    // Shape each item with the full context of the entire node.
    const ShapeResult* shape_result =
        shaper.Shape(start_item, font, end_offset);

    if (spacing.SetSpacing(font.GetFontDescription())) [[unlikely]] {
      DCHECK(!IsTextCombine()) << GetLayoutBlockFlow();
      DCHECK(!allow_shape_cache);
      // The ShapeResult is actually not a reusable entry of NGShapeCache,
      // so it is safe to mutate it.
      const_cast<ShapeResult*>(shape_result)->ApplySpacing(spacing);
    }

    // If the text is from one item, use the ShapeResult as is.
    if (end_offset == start_item.EndOffset()) {
      start_item.shape_result_ = shape_result;
      DCHECK_EQ(start_item.TextShapeResult()->StartIndex(),
                start_item.StartOffset());
      DCHECK_EQ(start_item.TextShapeResult()->EndIndex(),
                start_item.EndOffset());
      index++;
      continue;
    }

    // If the text is from multiple items, split the ShapeResult to
    // corresponding items.
    DCHECK_GT(num_text_items, 0u);
    // "32" is heuristic, most major sites are up to 8 or so, wikipedia is 21.
    HeapVector<ShapeResult::ShapeRange, 32> text_item_ranges;
    text_item_ranges.ReserveInitialCapacity(num_text_items);
    ClearCollectionScope clear_scope(&text_item_ranges);

    const bool has_ligatures =
        shape_result->NumGlyphs() < shape_result->NumCharacters();
    if (has_ligatures) {
      shape_result->EnsurePositionData();
    }
    for (; index < end_index; index++) {
      InlineItem& item = (*items)[index];
      if (item.Type() != InlineItem::kText || !item.Length()) {
        continue;
      }

      // We don't use SafeToBreak API here because this is not a line break.
      // The ShapeResult is broken into multiple results, but they must look
      // like they were not broken.
      //
      // When multiple code units shape to one glyph, such as ligatures, the
      // item that has its first code unit keeps the glyph.
      ShapeResult* item_result = ShapeResult::CreateEmpty(*shape_result);
      text_item_ranges.emplace_back(item.StartOffset(), item.EndOffset(),
                                    item_result);
      if (has_ligatures && item.EndOffset() < shape_result->EndIndex() &&
          shape_result->CachedNextSafeToBreakOffset(item.EndOffset()) !=
              item.EndOffset()) {
        // Note: We should not reuse `ShapeResult` ends with ligature glyph.
        // e.g. <div>f<span>i</div> to <div>f</div> with ligature "fi".
        // See http://crbug.com/1409702
        item.SetUnsafeToReuseShapeResult();
      }
      item.shape_result_ = item_result;
    }
    DCHECK_EQ(text_item_ranges.size(), num_text_items);
    shape_result->CopyRanges(text_item_ranges.data(), text_item_ranges.size());
  }

  auto_space.ApplyIfNeeded(*data);

#if DCHECK_IS_ON()
  for (const InlineItem& item : *items) {
    if (item.Type() == InlineItem::kText && item.Length()) {
      DCHECK(item.TextShapeResult());
      DCHECK_EQ(item.TextShapeResult()->StartIndex(), item.StartOffset());
      DCHECK_EQ(item.TextShapeResult()->EndIndex(), item.EndOffset());
    }
  }
#endif
}

// Create HeapVector<InlineItem> with :first-line rules applied if needed.
void InlineNode::ShapeTextForFirstLineIfNeeded(InlineNodeData* data) const {
  // First check if the document has any :first-line rules.
  DCHECK(!data->first_line_items_);
  LayoutObject* layout_object = GetLayoutBox();
  if (!layout_object->GetDocument().GetStyleEngine().UsesFirstLineRules())
    return;

  // Check if :first-line rules make any differences in the style.
  const ComputedStyle* block_style = layout_object->Style();
  const ComputedStyle* first_line_style = layout_object->FirstLineStyle();
  if (block_style == first_line_style)
    return;

  auto* first_line_items = MakeGarbageCollected<InlineItemsData>();
  String text_content = data->text_content;
  bool needs_reshape = false;
  if (first_line_style->TextTransform() != block_style->TextTransform()) {
    // TODO(kojii): This logic assumes that text-transform is applied only to
    // ::first-line, and does not work when the base style has text-transform
    // and ::first-line has different text-transform.
    text_content = first_line_style->ApplyTextTransform(text_content);
    if (text_content != data->text_content) {
      // TODO(kojii): When text-transform changes the length, we need to adjust
      // offset in InlineItem, or re-collect inlines. Other classes such as
      // line breaker need to support the scenario too. For now, we force the
      // string to be the same length to prevent them from crashing. This may
      // result in a missing or a duplicate character if the length changes.
      TruncateOrPadText(&text_content, data->text_content.length());
      needs_reshape = true;
    }
  }
  first_line_items->text_content = text_content;

  first_line_items->items.AppendVector(data->items);
  for (auto& item : first_line_items->items) {
    item.SetStyleVariant(StyleVariant::kFirstLine);
  }
  if (data->segments) {
    first_line_items->segments = data->segments->Clone();
  }

  // Re-shape if the font is different.
  if (needs_reshape || FirstLineNeedsReshape(*first_line_style, *block_style))
    ShapeText(first_line_items);

  data->first_line_items_ = first_line_items;
  // The score line breaker can't apply different styles by different line
  // breaking.
  data->is_score_line_break_disabled_ = true;
}

void InlineNode::ShapeTextIncludingFirstLine(
    InlineNodeData* data,
    const String* previous_text,
    const HeapVector<InlineItem>* previous_items) const {
  ShapeText(data, previous_text, previous_items);
  ShapeTextForFirstLineIfNeeded(data);
}

void InlineNode::AssociateItemsWithInlines(InlineNodeData* data) const {
#if DCHECK_IS_ON()
  HeapHashSet<Member<LayoutObject>> associated_objects;
#endif
  HeapVector<InlineItem>& items = data->items;
  WTF::wtf_size_t size = items.size();
  for (WTF::wtf_size_t i = 0; i != size;) {
    LayoutObject* object = items[i].GetLayoutObject();
    auto* layout_text = DynamicTo<LayoutText>(object);
    if (layout_text && !layout_text->IsBR()) {
#if DCHECK_IS_ON()
      // Items split from a LayoutObject should be consecutive.
      DCHECK(associated_objects.insert(object).is_new_entry);
#endif
      layout_text->ClearHasBidiControlInlineItems();
      bool has_bidi_control = false;
      WTF::wtf_size_t begin = i;
      for (++i; i != size; ++i) {
        auto& item = items[i];
        if (item.GetLayoutObject() != object)
          break;
        if (item.Type() == InlineItem::kBidiControl) {
          has_bidi_control = true;
        }
      }
      layout_text->SetInlineItems(data, begin, i - begin);
      if (has_bidi_control)
        layout_text->SetHasBidiControlInlineItems();
      continue;
    }
    ++i;
  }
}

const LayoutResult* InlineNode::Layout(
    const ConstraintSpace& constraint_space,
    const BreakToken* break_token,
    const ColumnSpannerPath* column_spanner_path,
    InlineChildLayoutContext* context) const {
  PrepareLayoutIfNeeded();

  const auto* inline_break_token = To<InlineBreakToken>(break_token);
  InlineLayoutAlgorithm algorithm(*this, constraint_space, inline_break_token,
                                  column_spanner_path, context);
  return algorithm.Layout();
}

namespace {

template <typename CharType>
String CreateTextContentForStickyImagesQuirk(
    const CharType* text,
    unsigned length,
    base::span<const InlineItem> items) {
  StringBuffer<CharType> buffer(length);
  CharType* characters = buffer.Characters();
  memcpy(characters, text, length * sizeof(CharType));
  for (const InlineItem& item : items) {
    if (item.Type() == InlineItem::kAtomicInline && item.IsImage()) {
      DCHECK_EQ(characters[item.StartOffset()], kObjectReplacementCharacter);
      characters[item.StartOffset()] = kNoBreakSpaceCharacter;
    }
  }
  return buffer.Release();
}

}  // namespace

// The stick images quirk changes the line breaking behavior around images. This
// function returns a text content that has non-breaking spaces for images, so
// that no changes are needed in the line breaking logic.
// https://quirks.spec.whatwg.org/#the-table-cell-width-calculation-quirk
// static
String InlineNode::TextContentForStickyImagesQuirk(
    const InlineItemsData& items_data) {
  const String& text_content = items_data.text_content;
  for (unsigned i = 0; i < items_data.items.size(); ++i) {
    const InlineItem& item = items_data.items[i];
    if (item.Type() == InlineItem::kAtomicInline && item.IsImage()) {
      auto item_span = base::span(items_data.items).subspan(i);
      if (text_content.Is8Bit()) {
        return CreateTextContentForStickyImagesQuirk(
            text_content.Characters8(), text_content.length(), item_span);
      }
      return CreateTextContentForStickyImagesQuirk(
          text_content.Characters16(), text_content.length(), item_span);
    }
  }
  return text_content;
}

static LayoutUnit ComputeContentSize(InlineNode node,
                                     WritingMode container_writing_mode,
                                     const ConstraintSpace& space,
                                     const MinMaxSizesFloatInput& float_input,
                                     LineBreakerMode mode,
                                     LineBreaker::MaxSizeCache* max_size_cache,
                                     std::optional<LayoutUnit>* max_size_out,
                                     bool* depends_on_block_constraints_out) {
  const ComputedStyle& style = node.Style();
  LayoutUnit available_inline_size =
      mode == LineBreakerMode::kMaxContent ? LayoutUnit::Max() : LayoutUnit();

  ExclusionSpace empty_exclusion_space;
  LeadingFloats empty_leading_floats;
  LineLayoutOpportunity line_opportunity(available_inline_size);
  LayoutUnit result;
  LineBreaker line_breaker(
      node, mode, space, line_opportunity, empty_leading_floats,
      /* break_token */ nullptr,
      /* column_spanner_path */ nullptr, &empty_exclusion_space);
  line_breaker.SetIntrinsicSizeOutputs(max_size_cache,
                                       depends_on_block_constraints_out);
  const InlineItemsData& items_data = line_breaker.ItemsData();

  // Computes max-size for floats in inline formatting context.
  class FloatsMaxSize {
    STACK_ALLOCATED();

   public:
    explicit FloatsMaxSize(const MinMaxSizesFloatInput& float_input)
        : floats_inline_size_(float_input.float_left_inline_size +
                              float_input.float_right_inline_size) {
      DCHECK_GE(floats_inline_size_, 0);
    }

    void AddFloat(const ComputedStyle& float_style,
                  const ComputedStyle& style,
                  LayoutUnit float_inline_max_size_with_margin) {
      floating_objects_.push_back(InlineNode::FloatingObject{
          float_style, style, float_inline_max_size_with_margin});
    }

    LayoutUnit ComputeMaxSizeForLine(LayoutUnit line_inline_size,
                                     LayoutUnit max_inline_size) {
      if (floating_objects_.empty())
        return std::max(max_inline_size, line_inline_size);

      EFloat previous_float_type = EFloat::kNone;
      for (const auto& floating_object : floating_objects_) {
        const EClear float_clear =
            floating_object.float_style->Clear(*floating_object.style);

        // If this float clears the previous float we start a new "line".
        // This is subtly different to block layout which will only reset either
        // the left or the right float size trackers.
        if ((previous_float_type == EFloat::kLeft &&
             (float_clear == EClear::kBoth || float_clear == EClear::kLeft)) ||
            (previous_float_type == EFloat::kRight &&
             (float_clear == EClear::kBoth || float_clear == EClear::kRight))) {
          max_inline_size =
              std::max(max_inline_size, line_inline_size + floats_inline_size_);
          floats_inline_size_ = LayoutUnit();
        }

        // When negative margins move the float outside the content area,
        // such float should not affect the content size.
        floats_inline_size_ += floating_object.float_inline_max_size_with_margin
                                   .ClampNegativeToZero();
        previous_float_type =
            floating_object.float_style->Floating(*floating_object.style);
      }
      max_inline_size =
          std::max(max_inline_size, line_inline_size + floats_inline_size_);
      floats_inline_size_ = LayoutUnit();
      floating_objects_.Shrink(0);
      return max_inline_size;
    }

   private:
    LayoutUnit floats_inline_size_;
    HeapVector<InlineNode::FloatingObject, 4> floating_objects_;
  };

  // This struct computes the max size from the line break results for the min
  // size.
  struct MaxSizeFromMinSize {
    STACK_ALLOCATED();

   public:
    using ItemIterator = HeapVector<InlineItem>::const_iterator;

    LayoutUnit position;
    LayoutUnit max_size;
    const InlineItemsData& items_data;
    ItemIterator next_item;
    const LineBreaker::MaxSizeCache& max_size_cache;
    FloatsMaxSize* floats;
    bool is_after_break = true;
    wtf_size_t annotation_nesting_level = 0;

    explicit MaxSizeFromMinSize(const InlineItemsData& items_data,
                                const LineBreaker::MaxSizeCache& max_size_cache,
                                FloatsMaxSize* floats)
        : items_data(items_data),
          next_item(items_data.items.begin()),
          max_size_cache(max_size_cache),
          floats(floats) {}

    // Add all text items up to |end|. The line break results for min size
    // may break text into multiple lines, and may remove trailing spaces. For
    // max size, use the original text widths from InlineItem instead.
    void AddTextUntil(ItemIterator end) {
      for (; next_item != end; ++next_item) {
        if (next_item->Type() == InlineItem::kOpenTag &&
            next_item->GetLayoutObject()->IsInlineRubyText()) {
          ++annotation_nesting_level;
        } else if (next_item->Type() == InlineItem::kCloseTag &&
                   next_item->GetLayoutObject()->IsInlineRubyText()) {
          --annotation_nesting_level;
        } else if (next_item->Type() == InlineItem::kText &&
                   next_item->Length() && annotation_nesting_level == 0) {
          DCHECK(next_item->TextShapeResult());
          const ShapeResult& shape_result = *next_item->TextShapeResult();
          position += shape_result.SnappedWidth().ClampNegativeToZero();
        }
      }
    }

    void ForceLineBreak(const LineInfo& line_info) {
      // Add all text up to the end of the line. There may be spaces that were
      // removed during the line breaking.
      CHECK_LE(line_info.EndItemIndex(), items_data.items.size());
      AddTextUntil(items_data.items.begin() + line_info.EndItemIndex());
      max_size = floats->ComputeMaxSizeForLine(position.ClampNegativeToZero(),
                                               max_size);
      position = LayoutUnit();
      is_after_break = true;
    }

    void AddTabulationCharacters(const InlineItem& item, unsigned length) {
      DCHECK_GE(length, 1u);
      AddTextUntil(items_data.ToItemIterator(item));
      DCHECK(item.Style());
      const ComputedStyle& style = *item.Style();
      const Font& font = style.GetFont();
      const SimpleFontData* font_data = font.PrimaryFont();
      const TabSize& tab_size = style.GetTabSize();
      // Sync with `ShapeResult::CreateForTabulationCharacters()`.
      TextRunLayoutUnit glyph_advance = TextRunLayoutUnit::FromFloatRound(
          font.TabWidth(font_data, tab_size, position));
      InlineLayoutUnit run_advance = glyph_advance;
      DCHECK_GE(length, 1u);
      if (length > 1u) {
        glyph_advance = TextRunLayoutUnit::FromFloatRound(
            font.TabWidth(font_data, tab_size));
        run_advance += glyph_advance.To<InlineLayoutUnit>() * (length - 1);
      }
      position += run_advance.ToCeil<LayoutUnit>().ClampNegativeToZero();
    }

    LayoutUnit Finish(ItemIterator end) {
      AddTextUntil(end);
      return floats->ComputeMaxSizeForLine(position.ClampNegativeToZero(),
                                           max_size);
    }

    void ComputeFromMinSize(const LineInfo& line_info) {
      if (is_after_break) {
        position += line_info.TextIndent();
        is_after_break = false;
      }

      ComputeFromMinSizeInternal(line_info);

      // Compute the forced break after all results were handled, because
      // when close tags appear after a forced break, they are included in
      // the line, and they may have inline sizes. crbug.com/991320.
      if (line_info.HasForcedBreak()) {
        ForceLineBreak(line_info);
      }
    }

    void ComputeFromMinSizeInternal(const LineInfo& line_info) {
      for (const InlineItemResult& result : line_info.Results()) {
        const InlineItem& item = *result.item;
        if (item.Type() == InlineItem::kText) {
          // Text in InlineItemResult may be wrapped and trailing spaces
          // may be removed. Ignore them, but add text later from
          // InlineItem.
          continue;
        }
#if DCHECK_IS_ON()
        if (item.Type() == InlineItem::kBlockInInline) {
          DCHECK(line_info.HasForcedBreak());
        }
#endif
        if (item.Type() == InlineItem::kAtomicInline ||
            item.Type() == InlineItem::kBlockInInline) {
          // The max-size for atomic inlines are cached in |max_size_cache|.
          unsigned item_index = items_data.ToItemIndex(item);
          position += max_size_cache[item_index];
          continue;
        }
        if (item.Type() == InlineItem::kControl) {
          UChar c = items_data.text_content[item.StartOffset()];
#if DCHECK_IS_ON()
          if (c == kNewlineCharacter)
            DCHECK(line_info.HasForcedBreak());
#endif
          // Tabulation characters change the widths by their positions, so
          // their widths for the max size may be different from the widths for
          // the min size. Fall back to 2 pass for now.
          if (c == kTabulationCharacter) {
            AddTabulationCharacters(item, result.Length());
            continue;
          }
        }
        if (result.IsRubyColumn()) {
          ComputeFromMinSizeInternal(result.ruby_column->base_line);
          continue;
        }
        position += result.inline_size;
      }
    }
  };

  if (node.IsInitialLetterBox()) [[unlikely]] {
    LayoutUnit inline_size = LayoutUnit();
    LineInfo line_info;
    do {
      line_breaker.NextLine(&line_info);
      if (line_info.Results().empty())
        break;
      inline_size =
          std::max(CalculateInitialLetterBoxInlineSize(line_info), inline_size);
    } while (!line_breaker.IsFinished());
    return inline_size;
  }

  FloatsMaxSize floats_max_size(float_input);
  bool can_compute_max_size_from_min_size = true;
  MaxSizeFromMinSize max_size_from_min_size(items_data, *max_size_cache,
                                            &floats_max_size);

  LineInfo line_info;
  do {
    line_breaker.NextLine(&line_info);
    if (line_info.Results().empty())
      break;

    LayoutUnit inline_size = line_info.Width();
    for (const InlineItemResult& item_result : line_info.Results()) {
      DCHECK(item_result.item);
      const InlineItem& item = *item_result.item;
      if (item.Type() != InlineItem::kFloating) {
        continue;
      }
      LayoutObject* floating_object = item.GetLayoutObject();
      DCHECK(floating_object && floating_object->IsFloating());

      BlockNode float_node(To<LayoutBox>(floating_object));

      MinMaxConstraintSpaceBuilder builder(space, style, float_node,
                                           /* is_new_fc */ true);
      builder.SetAvailableBlockSize(space.AvailableSize().block_size);
      builder.SetPercentageResolutionBlockSize(
          space.PercentageResolutionBlockSize());
      builder.SetReplacedPercentageResolutionBlockSize(
          space.ReplacedPercentageResolutionBlockSize());
      const auto float_space = builder.ToConstraintSpace();

      const MinMaxSizesResult child_result =
          ComputeMinAndMaxContentContribution(style, float_node, float_space);
      LayoutUnit child_inline_margins =
          ComputeMarginsFor(float_space, float_node.Style(), space).InlineSum();

      if (depends_on_block_constraints_out) {
        *depends_on_block_constraints_out |=
            child_result.depends_on_block_constraints;
      }

      if (mode == LineBreakerMode::kMinContent) {
        result = std::max(result,
                          child_result.sizes.min_size + child_inline_margins);
      }
      floats_max_size.AddFloat(
          float_node.Style(), style,
          child_result.sizes.max_size + child_inline_margins);
    }

    if (mode == LineBreakerMode::kMinContent) {
      result = std::max(result, inline_size);
      can_compute_max_size_from_min_size =
          can_compute_max_size_from_min_size &&
          // `box-decoration-break: clone` clones box decorations to each
          // fragment (line) that we cannot compute max-content from
          // min-content.
          !line_breaker.HasClonedBoxDecorations() &&
          !line_info.MayHaveRubyOverhang();
      if (can_compute_max_size_from_min_size)
        max_size_from_min_size.ComputeFromMinSize(line_info);
    } else {
      result = floats_max_size.ComputeMaxSizeForLine(inline_size, result);
    }
  } while (!line_breaker.IsFinished());

  if (mode == LineBreakerMode::kMinContent &&
      can_compute_max_size_from_min_size) {
    if (node.IsSvgText()) {
      *max_size_out = result;
      return result;
      // The following DCHECK_EQ() doesn't work well for SVG <text> because
      // it has glyph-split InlineItemResults. The sum of InlineItem
      // widths and the sum of InlineItemResult widths can be different.
    }
    *max_size_out = max_size_from_min_size.Finish(items_data.items.end());

#if EXPENSIVE_DCHECKS_ARE_ON()
    // Check the max size matches to the value computed from 2 pass.
    LayoutUnit content_size = ComputeContentSize(
        node, container_writing_mode, space, float_input,
        LineBreakerMode::kMaxContent, max_size_cache, nullptr, nullptr);
    bool values_might_be_saturated =
        (*max_size_out)->MightBeSaturated() || content_size.MightBeSaturated();
    if (!values_might_be_saturated) {
      DCHECK_EQ((*max_size_out)->Round(), content_size.Round())
          << node.GetLayoutBox();
    }
#endif
  }

  return result;
}

MinMaxSizesResult InlineNode::ComputeMinMaxSizes(
    WritingMode container_writing_mode,
    const ConstraintSpace& space,
    const MinMaxSizesFloatInput& float_input) const {
  PrepareLayoutIfNeeded();

  // Compute the max of inline sizes of all line boxes with 0 available inline
  // size. This gives the min-content, the width where lines wrap at every
  // break opportunity.
  LineBreaker::MaxSizeCache max_size_cache;
  MinMaxSizes sizes;
  std::optional<LayoutUnit> max_size;
  bool depends_on_block_constraints = false;
  sizes.min_size =
      ComputeContentSize(*this, container_writing_mode, space, float_input,
                         LineBreakerMode::kMinContent, &max_size_cache,
                         &max_size, &depends_on_block_constraints);
  if (max_size) {
    sizes.max_size = *max_size;
  } else {
    sizes.max_size = ComputeContentSize(
        *this, container_writing_mode, space, float_input,
        LineBreakerMode::kMaxContent, &max_size_cache, nullptr, nullptr);
  }

  // Negative text-indent can make min > max. Ensure min is the minimum size.
  sizes.min_size = std::min(sizes.min_size, sizes.max_size);

  return MinMaxSizesResult(sizes, depends_on_block_constraints);
}

bool InlineNode::UseFirstLineStyle() const {
  return GetLayoutBox() &&
         GetLayoutBox()->GetDocument().GetStyleEngine().UsesFirstLineRules();
}

void InlineNode::CheckConsistency() const {
#if DCHECK_IS_ON()
  const HeapVector<InlineItem>& items = Data().items;
  for (const InlineItem& item : items) {
    DCHECK(!item.GetLayoutObject() || !item.Style() ||
           item.Style() == item.GetLayoutObject()->Style());
  }
#endif
}

const Vector<std::pair<unsigned, SvgCharacterData>>&
InlineNode::SvgCharacterDataList() const {
  DCHECK(IsSvgText());
  return Data().svg_node_data_->character_data_list;
}

const HeapVector<SvgTextContentRange>& InlineNode::SvgTextLengthRangeList()
    const {
  DCHECK(IsSvgText());
  return Data().svg_node_data_->text_length_range_list;
}

const HeapVector<SvgTextContentRange>& InlineNode::SvgTextPathRangeList()
    const {
  DCHECK(IsSvgText());
  return Data().svg_node_data_->text_path_range_list;
}

void InlineNode::AdjustFontForTextCombineUprightAll() const {
  DCHECK(IsTextCombine()) << GetLayoutBlockFlow();
  DCHECK(IsPrepareLayoutFinished()) << GetLayoutBlockFlow();

  const float content_width = CalculateWidthForTextCombine(ItemsData(false));
  if (content_width == 0.0f) [[unlikely]] {
    return;  // See "fast/css/zero-font-size-crash.html".
  }
  auto& text_combine = *To<LayoutTextCombine>(GetLayoutBlockFlow());
  const float desired_width = text_combine.DesiredWidth();
  text_combine.ResetLayout();
  if (desired_width == 0.0f) [[unlikely]] {
    // See http://crbug.com/1342520
    return;
  }
  if (content_width <= desired_width)
    return;

  const Font& font = Style().GetFont();
  FontSelector* const font_selector = font.GetFontSelector();
  FontDescription description = font.GetFontDescription();

  // Try compressed fonts.
  static const std::array<FontWidthVariant, 3> kWidthVariants = {
      kHalfWidth, kThirdWidth, kQuarterWidth};
  for (const auto width_variant : kWidthVariants) {
    description.SetWidthVariant(width_variant);
    Font compressed_font(description, font_selector);
    // TODO(crbug.com/561873): PrimaryFont should not be nullptr.
    if (!compressed_font.PrimaryFont()) {
      continue;
    }
    ShapeText(MutableData(), nullptr, nullptr, &compressed_font);
    if (CalculateWidthForTextCombine(ItemsData(false)) <= desired_width) {
      text_combine.SetCompressedFont(compressed_font);
      return;
    }
  }

  // There is no compressed font to fit within 1em. We use original font with
  // scaling.
  ShapeText(MutableData());
  DCHECK_EQ(content_width, CalculateWidthForTextCombine(ItemsData(false)));
  DCHECK_NE(content_width, 0.0f);
  text_combine.SetScaleX(desired_width / content_width);
}

bool InlineNode::NeedsShapingForTesting(const InlineItem& item) {
  return NeedsShaping(item);
}

String InlineNode::ToString() const {
  return "InlineNode";
}

}  // namespace blink
