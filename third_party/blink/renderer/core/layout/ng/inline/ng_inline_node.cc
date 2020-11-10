// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"

#include <algorithm>
#include <memory>

#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/inline/layout_ng_text.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_bidi_paragraph.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_items_builder.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_breaker.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"
#include "third_party/blink/renderer/core/layout/ng/legacy_layout_tree_walking.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_item.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_positioned_float.h"
#include "third_party/blink/renderer/core/layout/ng/ng_space_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_unpositioned_float.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_shaper.h"
#include "third_party/blink/renderer/platform/fonts/shaping/run_segmenter.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_spacing.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"

namespace blink {

namespace {

bool IsLeftAligned(const ComputedStyle& style) {
  switch (style.GetTextAlign()) {
    case ETextAlign::kStart:
      return IsLtr(style.Direction());
    case ETextAlign::kEnd:
      return IsRtl(style.Direction());
    case ETextAlign::kLeft:
    case ETextAlign::kWebkitLeft:
      return true;
    case ETextAlign::kCenter:
    case ETextAlign::kWebkitCenter:
    case ETextAlign::kJustify:
    case ETextAlign::kRight:
    case ETextAlign::kWebkitRight:
      return false;
  }
  NOTREACHED();
  return false;
}

bool HasLetterSpacingWorkAround(const LayoutObject* layout_object,
                                bool first_line,
                                const LayoutBlockFlow* block_flow) {
  // All we need to know is whether it computes to 0 or not, so any
  // |maxmimum_value| can work.
  const LayoutUnit maximum_value(100);
  if (MinimumValueForLength(block_flow->StyleRef(first_line).TextIndent(),
                            maximum_value))
    return true;

  // Margin/padding maybe applied to <span> or to the containing block. Sum up
  // to the containing block. ex.:
  //   <div style="letter-spacing: 1em">
  //     <span style="margin-left: 1em>text</span>
  //   </div>
  LayoutUnit margin_padding_start;
  LayoutUnit margin_padding_end;
  DCHECK(!layout_object->IsText());
  for (;; layout_object = layout_object->Parent()) {
    const ComputedStyle& style = layout_object->StyleRef(first_line);
    if (style.MayHavePadding() || style.MayHaveMargin()) {
      margin_padding_start +=
          MinimumValueForLength(style.MarginStart(), maximum_value) +
          MinimumValueForLength(style.PaddingStart(), maximum_value);
      margin_padding_end +=
          MinimumValueForLength(style.MarginEnd(), maximum_value) +
          MinimumValueForLength(style.PaddingEnd(), maximum_value);
    }
    if (layout_object == block_flow)
      break;
  }
  return margin_padding_start != margin_padding_end;
}

bool ShouldReportLetterSpacingUseCounter(const LayoutObject* layout_object,
                                         bool first_line,
                                         const LayoutBlockFlow* block_flow) {
  DCHECK(layout_object->IsText());
  layout_object = layout_object->Parent();
  const ComputedStyle& style = layout_object->StyleRef(first_line);
  DCHECK(style.GetFont().GetFontDescription().LetterSpacing());

  // Count only when the containing block has `letter-spacing`. For now, we
  // don't count cases like:
  //   <div><span style="letter-spacing: 1em">text</span></div>
  const ComputedStyle& block_style = block_flow->StyleRef(first_line);
  if (layout_object != block_flow &&
      !block_style.GetFont().GetFontDescription().LetterSpacing())
    return false;

  if (((layout_object->HasBoxDecorationBackground() ||
        block_flow->HasBoxDecorationBackground() ||
        !IsLeftAligned(block_style)) &&
       HasLetterSpacingWorkAround(layout_object, first_line, block_flow)) ||
      // Workaround for `text-decoration` is complicated, just include all.
      !style.AppliedTextDecorations().IsEmpty())
    return true;

  return false;
}

// Estimate the number of NGInlineItem to minimize the vector expansions.
unsigned EstimateInlineItemsCount(const LayoutBlockFlow& block) {
  unsigned count = 0;
  for (LayoutObject* child = block.FirstChild(); child;
       child = child->NextSibling()) {
    ++count;
  }
  return count * 4;
}

// Estimate the number of units and ranges in NGOffsetMapping to minimize vector
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
 public:
  ReusingTextShaper(NGInlineItemsData* data,
                    const Vector<NGInlineItem>* reusable_items)
      : data_(*data),
        reusable_items_(reusable_items),
        shaper_(data->text_content) {}

  scoped_refptr<ShapeResult> Shape(const NGInlineItem& start_item,
                                   unsigned end_offset) {
    const unsigned start_offset = start_item.StartOffset();
    DCHECK_LT(start_offset, end_offset);

    if (!reusable_items_)
      return Reshape(start_item, start_offset, end_offset);

    // TODO(yosin): We should support segment text
    if (data_.segments)
      return Reshape(start_item, start_offset, end_offset);

    const Vector<const ShapeResult*> reusable_shape_results =
        CollectReusableShapeResults(start_offset, end_offset,
                                    start_item.Direction());
    if (reusable_shape_results.IsEmpty())
      return Reshape(start_item, start_offset, end_offset);

    const scoped_refptr<ShapeResult> shape_result =
        ShapeResult::CreateEmpty(*reusable_shape_results.front());
    unsigned offset = start_offset;
    for (const ShapeResult* reusable_shape_result : reusable_shape_results) {
      // In case of pre-wrap having break opportunity after leading space,
      // |offset| can be greater than |reusable_shape_result->StartIndex()|.
      // e.g. <div style="white-space:pre">&nbsp; abc</div>, deleteChar(0, 1)
      // See xternal/wpt/editing/run/delete.html?993-993
      if (offset < reusable_shape_result->StartIndex()) {
        AppendShapeResult(
            *Reshape(start_item, offset, reusable_shape_result->StartIndex()),
            shape_result.get());
        offset = shape_result->EndIndex();
      }
      DCHECK_LT(offset, reusable_shape_result->EndIndex());
      DCHECK(shape_result->NumCharacters() == 0 ||
             shape_result->EndIndex() == offset);
      reusable_shape_result->CopyRange(
          offset, std::min(reusable_shape_result->EndIndex(), end_offset),
          shape_result.get());
      offset = shape_result->EndIndex();
      if (offset == end_offset)
        return shape_result;
    }
    DCHECK_LT(offset, end_offset);
    AppendShapeResult(*Reshape(start_item, offset, end_offset),
                      shape_result.get());
    return shape_result;
  }

 private:
  void AppendShapeResult(const ShapeResult& shape_result, ShapeResult* target) {
    DCHECK(target->NumCharacters() == 0 ||
           target->EndIndex() == shape_result.StartIndex());
    shape_result.CopyRange(shape_result.StartIndex(), shape_result.EndIndex(),
                           target);
  }

  Vector<const ShapeResult*> CollectReusableShapeResults(
      unsigned start_offset,
      unsigned end_offset,
      TextDirection direction) {
    DCHECK_LT(start_offset, end_offset);
    Vector<const ShapeResult*> shape_results;
    if (!reusable_items_)
      return shape_results;
    for (const NGInlineItem *item = std::lower_bound(
             reusable_items_->begin(), reusable_items_->end(), start_offset,
             [](const NGInlineItem&item, unsigned offset) {
               return item.EndOffset() <= offset;
             });
         item != reusable_items_->end(); ++item) {
      if (end_offset <= item->StartOffset())
        break;
      if (item->EndOffset() < start_offset)
        continue;
      if (!item->TextShapeResult() || item->Direction() != direction)
        continue;
      if (item->TextShapeResult()->IsAppliedSpacing())
        continue;
      shape_results.push_back(item->TextShapeResult());
    }
    return shape_results;
  }

  scoped_refptr<ShapeResult> Reshape(const NGInlineItem& start_item,
                                     unsigned start_offset,
                                     unsigned end_offset) {
    DCHECK_LT(start_offset, end_offset);
    const TextDirection direction = start_item.Direction();
    const Font& font = start_item.Style()->GetFont();
    if (data_.segments) {
      return data_.segments->ShapeText(&shaper_, &font, direction, start_offset,
                                       end_offset,
                                       &start_item - data_.items.begin());
    }
    RunSegmenter::RunSegmenterRange range =
        start_item.CreateRunSegmenterRange();
    range.end = end_offset;
    return shaper_.Shape(&font, direction, start_offset, end_offset, range);
  }

  NGInlineItemsData& data_;
  const Vector<NGInlineItem>* const reusable_items_;
  HarfBuzzShaper shaper_;
};

// The function is templated to indicate the purpose of collected inlines:
// - With EmptyOffsetMappingBuilder: updating layout;
// - With NGOffsetMappingBuilder: building offset mapping on clean layout.
//
// This allows code sharing between the two purposes with slightly different
// behaviors. For example, we clear a LayoutObject's need layout flags when
// updating layout, but don't do that when building offset mapping.
//
// There are also performance considerations, since template saves the overhead
// for condition checking and branching.
template <typename ItemsBuilder>
void CollectInlinesInternal(ItemsBuilder* builder,
                            const NGInlineNodeData* previous_data) {
  LayoutBlockFlow* const block = builder->GetLayoutBlockFlow();
  builder->EnterBlock(block->Style());
  LayoutObject* node = GetLayoutObjectForFirstChildNode(block);

  const LayoutObject* symbol =
      LayoutNGListItem::FindSymbolMarkerLayoutText(block);
  while (node) {
    if (auto* layout_text = DynamicTo<LayoutText>(node)) {
      builder->AppendText(layout_text, previous_data);

      if (symbol == layout_text)
        builder->SetIsSymbolMarker();

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
      if (node->IsBoxListMarkerIncludingNG()) {
        // LayoutNGListItem produces the 'outside' list marker as an inline
        // block. This is an out-of-flow item whose position is computed
        // automatically.
        builder->AppendOpaque(NGInlineItem::kListMarker, node);
      } else {
        // For atomic inlines add a unicode "object replacement character" to
        // signal the presence of a non-text object to the unicode bidi
        // algorithm.
        builder->AppendAtomicInline(node);
      }
      builder->ClearInlineFragment(node);

    } else {
      // Because we're collecting from LayoutObject tree, block-level children
      // should not appear. LayoutObject tree should have created an anonymous
      // box to prevent having inline/block-mixed children.
      DCHECK(node->IsInline());
      LayoutInline* layout_inline = ToLayoutInline(node);
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
inline bool ShouldBreakShapingBeforeText(const NGInlineItem& item,
                                         const NGInlineItem& start_item,
                                         const ComputedStyle& start_style,
                                         const Font& start_font,
                                         TextDirection start_direction) {
  DCHECK_EQ(item.Type(), NGInlineItem::kText);
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
inline bool ShouldBreakShapingBeforeBox(const NGInlineItem& item,
                                        const Font& start_font) {
  DCHECK_EQ(item.Type(), NGInlineItem::kOpenTag);
  DCHECK(item.Style());
  const ComputedStyle& style = *item.Style();

  // These properties values must break shaping.
  // https://drafts.csswg.org/css-text-3/#boundary-shaping
  if ((style.MayHavePadding() && !style.PaddingStart().IsZero()) ||
      (style.MayHaveMargin() && !style.MarginStart().IsZero()) ||
      style.BorderStartWidth() ||
      style.VerticalAlign() != EVerticalAlign::kBaseline)
    return true;

  return false;
}

// Returns whether the end of this box should break shaping.
inline bool ShouldBreakShapingAfterBox(const NGInlineItem& item,
                                       const Font& start_font) {
  DCHECK_EQ(item.Type(), NGInlineItem::kCloseTag);
  DCHECK(item.Style());
  const ComputedStyle& style = *item.Style();

  // These properties values must break shaping.
  // https://drafts.csswg.org/css-text-3/#boundary-shaping
  if ((style.MayHavePadding() && !style.PaddingEnd().IsZero()) ||
      (style.MayHaveMargin() && !style.MarginEnd().IsZero()) ||
      style.BorderEndWidth() ||
      style.VerticalAlign() != EVerticalAlign::kBaseline)
    return true;

  return false;
}

inline bool NeedsShaping(const NGInlineItem& item) {
  return item.Type() == NGInlineItem::kText && !item.TextShapeResult() &&
         // Text item with length==0 exists to maintain LayoutObject states such
         // as ClearNeedsLayout, but not needed to shape.
         item.Length();
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

}  // namespace

NGInlineNode::NGInlineNode(LayoutBlockFlow* block)
    : NGLayoutInputNode(block, kInline) {
  DCHECK(block);
  DCHECK(block->IsLayoutNGMixin());
  if (!block->HasNGInlineNodeData())
    block->ResetNGInlineNodeData();
}

bool NGInlineNode::IsPrepareLayoutFinished() const {
  const NGInlineNodeData* data =
      To<LayoutBlockFlow>(box_)->GetNGInlineNodeData();
  return data && !data->text_content.IsNull();
}

void NGInlineNode::PrepareLayoutIfNeeded() const {
  std::unique_ptr<NGInlineNodeData> previous_data;
  LayoutBlockFlow* block_flow = GetLayoutBlockFlow();
  if (IsPrepareLayoutFinished()) {
    if (!block_flow->NeedsCollectInlines())
      return;

    previous_data.reset(block_flow->TakeNGInlineNodeData());
    block_flow->ResetNGInlineNodeData();
  }

  PrepareLayout(std::move(previous_data));
}

void NGInlineNode::PrepareLayout(
    std::unique_ptr<NGInlineNodeData> previous_data) const {
  // Scan list of siblings collecting all in-flow non-atomic inlines. A single
  // NGInlineNode represent a collection of adjacent non-atomic inlines.
  NGInlineNodeData* data = MutableData();
  DCHECK(data);
  CollectInlines(data, previous_data.get());
  SegmentText(data);
  ShapeText(data, previous_data ? &previous_data->text_content : nullptr);
  ShapeTextForFirstLineIfNeeded(data);
  AssociateItemsWithInlines(data);
  DCHECK_EQ(data, MutableData());

  LayoutBlockFlow* block_flow = GetLayoutBlockFlow();
  block_flow->ClearNeedsCollectInlines();

#if DCHECK_IS_ON()
  // ComputeOffsetMappingIfNeeded() runs some integrity checks as part of
  // creating offset mapping. Run the check, and discard the result.
  DCHECK(!data->offset_mapping);
  ComputeOffsetMappingIfNeeded();
  DCHECK(data->offset_mapping);
  data->offset_mapping.reset();
#endif
}

// Building |NGInlineNodeData| for |LayoutText::SetTextWithOffset()| with
// reusing data.
class NGInlineNodeDataEditor final {
  STACK_ALLOCATED();

 public:
  explicit NGInlineNodeDataEditor(const LayoutText& layout_text)
      : block_flow_(layout_text.ContainingNGBlockFlow()),
        layout_text_(layout_text) {
    DCHECK(layout_text_.HasValidInlineItems());
  }
  NGInlineNodeDataEditor(const NGInlineNodeDataEditor&) = delete;
  NGInlineNodeDataEditor& operator=(const NGInlineNodeDataEditor&) = delete;

  LayoutBlockFlow* GetLayoutBlockFlow() const { return block_flow_; }

  // Note: We can't use |Position| for |layout_text_.GetNode()| because |Text|
  // node is already changed.
  NGInlineNodeData* Prepare(unsigned offset, unsigned length) {
    if (!block_flow_ || block_flow_->NeedsCollectInlines() ||
        block_flow_->NeedsLayout() ||
        block_flow_->GetDocument().NeedsLayoutTreeUpdate() ||
        !block_flow_->GetNGInlineNodeData() ||
        block_flow_->GetNGInlineNodeData()->text_content.IsNull() ||
        block_flow_->GetNGInlineNodeData()->items.IsEmpty())
      return nullptr;

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
    // |LayoutBlockFlow::TakeNGInlineNodeData()|
    const NGOffsetMapping* const offset_mapping =
        NGInlineNode::GetOffsetMapping(block_flow_);
    DCHECK(offset_mapping);
    data_.reset(block_flow_->TakeNGInlineNodeData());
    return data_.get();
  }

  void Run() {
    const NGInlineNodeData& new_data = *block_flow_->GetNGInlineNodeData();
    const unsigned old_length = data_->text_content.length();
    const unsigned new_length = new_data.text_content.length();
    const unsigned start_offset = Mismatch(*data_, new_data);
    //  * "ab cd ef" => delete "cd" => "ab ef"
    //    We should not reuse " " before "ef"
    //  * "a bc" => delete "bc" => "a"
    //    There are no spaces after "a".
    const unsigned matched_length = MismatchFromEnd(
        *data_, new_data,
        std::min(old_length - start_offset, new_length - start_offset));
    DCHECK_LE(start_offset, old_length - matched_length);
    DCHECK_LE(start_offset, new_length - matched_length);
    const unsigned end_offset = old_length - matched_length;
    DCHECK_LE(start_offset, end_offset);

    Vector<NGInlineItem> items;
    // +3 for before and after replaced text.
    items.ReserveInitialCapacity(data_->items.size() + 3);

    // Copy items before replaced range
    auto const* end = data_->items.end();
    auto* it = data_->items.begin();
    while (it != end && it->end_offset_ < start_offset) {
      DCHECK(it != data_->items.end());
      items.push_back(*it);
      ++it;
    }

    for (;;) {
      if (it == end)
        break;

      // Copy part of item before replaced range.
      if (it->start_offset_ < start_offset) {
        const NGInlineItem& new_item = CopyItemBefore(*it, start_offset);
        items.push_back(new_item);
        if (new_item.EndOffset() < start_offset) {
          items.push_back(
              NGInlineItem(*it, new_item.EndOffset(), start_offset, nullptr));
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
        items.push_back(NGInlineItem(*it, start_offset, inserted_end, nullptr));

      // Copy part of item after replaced range.
      if (end_offset < it->end_offset_) {
        const NGInlineItem& new_item = CopyItemAfter(*it, end_offset);
        if (end_offset < new_item.StartOffset()) {
          items.push_back(
              NGInlineItem(*it, end_offset, new_item.StartOffset(), nullptr));
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

    if (items.IsEmpty()) {
      items.push_back(NGInlineItem(data_->items.front(), 0,
                                   new_data.text_content.length(), nullptr));
    } else if (items.back().end_offset_ < new_data.text_content.length()) {
      items.push_back(NGInlineItem(data_->items.back(),
                                   items.back().end_offset_,
                                   new_data.text_content.length(), nullptr));
    }

    VerifyItems(items);
    data_->items = std::move(items);
    data_->text_content = new_data.text_content;
  }

 private:
  static unsigned AdjustOffset(unsigned offset, int delta) {
    if (delta > 0)
      return offset + delta;
    return offset - (-delta);
  }

  static unsigned ConvertDOMOffsetToTextContent(
      base::span<const NGOffsetMappingUnit> units,
      unsigned offset) {
    auto it = std::find_if(
        units.begin(), units.end(), [offset](const NGOffsetMappingUnit& unit) {
          return unit.DOMStart() <= offset && offset <= unit.DOMEnd();
        });
    DCHECK(it != units.end());
    return it->ConvertDOMOffsetToTextContent(offset);
  }

  // Returns copy of |item| after |start_offset| (inclusive).
  NGInlineItem CopyItemAfter(const NGInlineItem& item,
                             unsigned start_offset) const {
    DCHECK_LE(item.start_offset_, start_offset);
    DCHECK_LT(start_offset, item.end_offset_);
    const unsigned end_offset = item.end_offset_;
    if (!item.shape_result_ || item.shape_result_->IsAppliedSpacing())
      return NGInlineItem(item, start_offset, end_offset, nullptr);
    if (item.start_offset_ == start_offset)
      return item;
    // TODO(yosin): We should handle |shape_result| doesn't have safe-to-break
    // at start and end, because of |ShapeText()| splits |ShapeResult| ignoring
    // safe-to-break offset.
    item.shape_result_->EnsurePositionData();
    const unsigned safe_start_offset =
        item.shape_result_->CachedNextSafeToBreakOffset(start_offset);
    if (end_offset == safe_start_offset)
      return NGInlineItem(item, start_offset, end_offset, nullptr);
    return NGInlineItem(
        item, safe_start_offset, end_offset,
        item.shape_result_->SubRange(safe_start_offset, end_offset));
  }

  // Returns copy of |item| before |end_offset| (exclusive).
  NGInlineItem CopyItemBefore(const NGInlineItem& item,
                              unsigned end_offset) const {
    DCHECK_LT(item.start_offset_, end_offset);
    DCHECK_LE(end_offset, item.end_offset_);
    const unsigned safe_end_offset = GetLastSafeToReuse(item, end_offset);
    const unsigned start_offset = item.start_offset_;
    if (start_offset == safe_end_offset)
      return NGInlineItem(item, start_offset, end_offset, nullptr);
    // To handle kerning, e.g. "AV", we should not reuse last glyph.
    // See http://crbug.com/1129710
    DCHECK_LT(safe_end_offset, item.end_offset_);
    return NGInlineItem(
        item, start_offset, safe_end_offset,
        item.shape_result_->SubRange(start_offset, safe_end_offset));
  }

  unsigned GetLastSafeToReuse(const NGInlineItem& item,
                              unsigned end_offset) const {
    DCHECK_LT(item.start_offset_, end_offset);
    DCHECK_LE(end_offset, item.end_offset_);
    const unsigned start_offset = item.start_offset_;
    if (!item.shape_result_ || item.shape_result_->IsAppliedSpacing() ||
        end_offset - start_offset <= 1)
      return start_offset;
    item.shape_result_->EnsurePositionData();
    // Note: Because |CachedPreviousSafeToBreakOffset()| assumes |end_offset|
    // is always safe to break offset, we try to search before |end_offset|.
    return item.shape_result_->CachedPreviousSafeToBreakOffset(end_offset - 1);
  }

  template <typename Span1, typename Span2>
  static unsigned MismatchInternal(const Span1& span1, const Span2& span2) {
    const auto old_new =
        std::mismatch(span1.begin(), span1.end(), span2.begin(), span2.end());
    return static_cast<unsigned>(old_new.first - span1.begin());
  }

  static unsigned Mismatch(const NGInlineItemsData& old_data,
                           const NGInlineItemsData& new_data) {
    const StringImpl& old_text = *old_data.text_content.Impl();
    const StringImpl& new_text = *new_data.text_content.Impl();
    if (old_text.Is8Bit()) {
      const auto old_span8 = old_text.Span8();
      if (new_text.Is8Bit())
        return MismatchInternal(old_span8, new_text.Span8());
      return MismatchInternal(old_span8, new_text.Span16());
    }
    const auto old_span16 = old_text.Span16();
    if (new_text.Is8Bit())
      return MismatchInternal(old_span16, new_text.Span8());
    return MismatchInternal(old_span16, new_text.Span16());
  }

  template <typename Span1, typename Span2>
  static unsigned MismatchFromEnd(const Span1& span1, const Span2& span2) {
    const auto old_new = std::mismatch(span1.rbegin(), span1.rend(),
                                       span2.rbegin(), span2.rend());
    return static_cast<unsigned>(old_new.first - span1.rbegin());
  }

  static unsigned MismatchFromEnd(const NGInlineItemsData& old_data,
                                  const NGInlineItemsData& new_data,
                                  unsigned max_length) {
    const StringImpl& old_text = *old_data.text_content.Impl();
    const StringImpl& new_text = *new_data.text_content.Impl();
    const unsigned old_length = old_text.length();
    const unsigned new_length = new_text.length();
    DCHECK_LE(max_length, old_length);
    DCHECK_LE(max_length, new_length);
    const unsigned old_start = old_length - max_length;
    const unsigned new_start = new_length - max_length;
    if (old_text.Is8Bit()) {
      const auto old_span8 = old_text.Span8().subspan(old_start, max_length);
      if (new_text.Is8Bit()) {
        return MismatchFromEnd(old_span8,
                               new_text.Span8().subspan(new_start, max_length));
      }
      return MismatchFromEnd(old_span8,
                             new_text.Span16().subspan(new_start, max_length));
    }
    const auto old_span16 = old_text.Span16().subspan(old_start, max_length);
    if (new_text.Is8Bit()) {
      return MismatchFromEnd(old_span16,
                             new_text.Span8().subspan(new_start, max_length));
    }
    return MismatchFromEnd(old_span16,
                           new_text.Span16().subspan(new_start, max_length));
  }

  static void ShiftItem(NGInlineItem* item, int delta) {
    if (delta == 0)
      return;
    item->start_offset_ = AdjustOffset(item->start_offset_, delta);
    item->end_offset_ = AdjustOffset(item->end_offset_, delta);
    if (!item->shape_result_)
      return;
    item->shape_result_ =
        item->shape_result_->CopyAdjustedOffset(item->start_offset_);
  }

  void VerifyItems(const Vector<NGInlineItem>& items) const {
#if DCHECK_IS_ON()
    if (items.IsEmpty())
      return;
    unsigned last_offset = items.front().start_offset_;
    for (const NGInlineItem& item : items) {
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
              block_flow_->GetNGInlineNodeData()->text_content.length());
#endif
  }

  std::unique_ptr<NGInlineNodeData> data_;
  LayoutBlockFlow* const block_flow_;
  const LayoutText& layout_text_;
};

// static
bool NGInlineNode::SetTextWithOffset(LayoutText* layout_text,
                                     scoped_refptr<StringImpl> new_text_in,
                                     unsigned offset,
                                     unsigned length) {
  if (!layout_text->HasValidInlineItems() ||
      !layout_text->IsInLayoutNGInlineFormattingContext())
    return false;
  const String old_text = layout_text->GetText();
  if (offset == 0 && length == old_text.length()) {
    // We'll run collect inline items since whole text of |layout_text| is
    // changed.
    return false;
  }

  NGInlineNodeDataEditor editor(*layout_text);
  NGInlineNodeData* const previous_data = editor.Prepare(offset, length);
  if (!previous_data)
    return false;

  // This function runs outside of the layout phase. Prevent purging font cache
  // while shaping.
  FontCachePurgePreventer fontCachePurgePreventer;

  String new_text(std::move(new_text_in));
  layout_text->StyleRef().ApplyTextTransform(&new_text,
                                             layout_text->PreviousCharacter());
  layout_text->SetTextInternal(new_text.Impl());

  NGInlineNode node(editor.GetLayoutBlockFlow());
  NGInlineNodeData* data = node.MutableData();
  data->items.ReserveCapacity(previous_data->items.size());
  NGInlineItemsBuilder builder(editor.GetLayoutBlockFlow(), &data->items);
  // TODO(yosin): We should reuse before/after |layout_text| during collecting
  // inline items.
  layout_text->ClearInlineItems();
  CollectInlinesInternal(&builder, previous_data);
  builder.DidFinishCollectInlines(data);
  // Relocates |ShapeResult| in |previous_data| after |offset|+|length|
  editor.Run();
  node.SegmentText(data);
  node.ShapeText(data, &previous_data->text_content, &previous_data->items);
  node.ShapeTextForFirstLineIfNeeded(data);
  node.AssociateItemsWithInlines(data);
  return true;
}

const NGInlineNodeData& NGInlineNode::EnsureData() const {
  PrepareLayoutIfNeeded();
  return Data();
}

const NGOffsetMapping* NGInlineNode::ComputeOffsetMappingIfNeeded() const {
  DCHECK(!GetLayoutBlockFlow()->GetDocument().NeedsLayoutTreeUpdate());

  NGInlineNodeData* data = MutableData();
  if (!data->offset_mapping) {
    DCHECK(!data->text_content.IsNull());
    ComputeOffsetMapping(GetLayoutBlockFlow(), data);
    DCHECK(data->offset_mapping);
  }

  return data->offset_mapping.get();
}

void NGInlineNode::ComputeOffsetMapping(LayoutBlockFlow* layout_block_flow,
                                        NGInlineNodeData* data) {
  DCHECK(!data->offset_mapping);
  DCHECK(!layout_block_flow->GetDocument().NeedsLayoutTreeUpdate());

  // TODO(xiaochengh): ComputeOffsetMappingIfNeeded() discards the
  // NGInlineItems and text content built by |builder|, because they are
  // already there in NGInlineNodeData. For efficiency, we should make
  // |builder| not construct items and text content.
  Vector<NGInlineItem> items;
  items.ReserveCapacity(EstimateInlineItemsCount(*layout_block_flow));
  NGInlineItemsBuilderForOffsetMapping builder(layout_block_flow, &items);
  builder.GetOffsetMappingBuilder().ReserveCapacity(
      EstimateOffsetMappingItemsCount(*layout_block_flow));
  CollectInlinesInternal(&builder, nullptr);

  // For non-NG object, we need the text, and also the inline items to resolve
  // bidi levels. Otherwise |data| already has the text from the pre-layout
  // phase, check they match.
  if (data->text_content.IsNull()) {
    DCHECK(!layout_block_flow->IsLayoutNGMixin());
    data->text_content = builder.ToString();
  } else {
    DCHECK(layout_block_flow->IsLayoutNGMixin());
    DCHECK_EQ(data->text_content, builder.ToString());
  }

  // TODO(xiaochengh): This doesn't compute offset mapping correctly when
  // text-transform CSS property changes text length.
  NGOffsetMappingBuilder& mapping_builder = builder.GetOffsetMappingBuilder();
  mapping_builder.SetDestinationString(data->text_content);
  data->offset_mapping = mapping_builder.Build();
  DCHECK(data->offset_mapping);
}

const NGOffsetMapping* NGInlineNode::GetOffsetMapping(
    LayoutBlockFlow* layout_block_flow) {
  DCHECK(!layout_block_flow->GetDocument().NeedsLayoutTreeUpdate());

  if (UNLIKELY(layout_block_flow->NeedsLayout())) {
    // TODO(kojii): This shouldn't happen, but is not easy to fix all cases.
    // Return nullptr so that callers can chose to fail gracefully, or
    // null-deref. crbug.com/946004
    NOTREACHED();
    return nullptr;
  }

  // If |layout_block_flow| is LayoutNG, compute from |NGInlineNode|.
  if (layout_block_flow->IsLayoutNGMixin()) {
    NGInlineNode node(layout_block_flow);
    CHECK(node.IsPrepareLayoutFinished());
    return node.ComputeOffsetMappingIfNeeded();
  }

  // If this is not LayoutNG, compute the offset mapping and store into
  // |LayoutBlockFlowRareData|.
  if (const NGOffsetMapping* mapping = layout_block_flow->GetOffsetMapping())
    return mapping;
  NGInlineNodeData data;
  ComputeOffsetMapping(layout_block_flow, &data);
  NGOffsetMapping* const mapping = data.offset_mapping.get();
  layout_block_flow->SetOffsetMapping(std::move(data.offset_mapping));
  return mapping;
}

// Depth-first-scan of all LayoutInline and LayoutText nodes that make up this
// NGInlineNode object. Collects LayoutText items, merging them up into the
// parent LayoutInline where possible, and joining all text content in a single
// string to allow bidi resolution and shaping of the entire block.
void NGInlineNode::CollectInlines(NGInlineNodeData* data,
                                  NGInlineNodeData* previous_data) const {
  DCHECK(data->text_content.IsNull());
  DCHECK(data->items.IsEmpty());
  LayoutBlockFlow* block = GetLayoutBlockFlow();
  block->WillCollectInlines();

  data->items.ReserveCapacity(EstimateInlineItemsCount(*block));
  NGInlineItemsBuilder builder(block, &data->items);
  CollectInlinesInternal(&builder, previous_data);
  builder.DidFinishCollectInlines(data);

  if (UNLIKELY(builder.HasUnicodeBidiPlainText()))
    UseCounter::Count(GetDocument(), WebFeature::kUnicodeBidiPlainText);
}

void NGInlineNode::SegmentText(NGInlineNodeData* data) const {
  SegmentBidiRuns(data);
  SegmentScriptRuns(data);
  SegmentFontOrientation(data);
  if (data->segments)
    data->segments->ComputeItemIndex(data->items);
}

// Segment NGInlineItem by script, Emoji, and orientation using RunSegmenter.
void NGInlineNode::SegmentScriptRuns(NGInlineNodeData* data) const {
  DCHECK_EQ(data->segments.get(), nullptr);

  String& text_content = data->text_content;
  if (text_content.IsEmpty()) {
    return;
  }

  Vector<NGInlineItem>& items = data->items;
  if (items.IsEmpty()) {
    return;
  }

  if (text_content.Is8Bit() && !data->is_bidi_enabled_) {
    RunSegmenter::RunSegmenterRange range = {
        0u, data->text_content.length(), USCRIPT_LATIN,
        OrientationIterator::kOrientationKeep, FontFallbackPriority::kText};
    NGInlineItem::SetSegmentData(range, &items);
    return;
  }

  // Segment by script and Emoji.
  // Orientation is segmented separately, because it may vary by items.
  text_content.Ensure16Bit();

  NGInlineItem* current_item = &items.front();
  unsigned range_length = current_item->Length();

  RunSegmenter::RunSegmenterRange range = RunSegmenter::NullRange();
  if (data->is_bidi_enabled_) {
    // run RunSegmenter for each bidi run
    for (wtf_size_t idx = 1; idx < items.size(); idx++) {
      NGInlineItem& item = items[idx];
      if (item.BidiLevel() == current_item->BidiLevel()) {
        // same bidi level as the previous item. We can merge
        range_length += item.Length();
        continue;
      }

      // We have reached the boundary of a bidi run. We need to run the script
      // segmenter.
      if (!data->segments) {
        data->segments = std::make_unique<NGInlineItemSegments>();
      }
      RunSegmenter segmenter(text_content.Characters16(), range_length,
                             FontOrientation::kHorizontal,
                             current_item->StartOffset());
      while (segmenter.Consume(&range)) {
        data->segments->Append(range);
      }
      range_length = item.Length();
      current_item = &item;
    }
  } else {
    range_length = text_content.length();
  }

  // We will now handle the last item. If the text is not bidirectional, it
  // will be the only one.

  if (range_length == 0) {
    return;
  }

  RunSegmenter segmenter(text_content.Characters16(), range_length,
                         FontOrientation::kHorizontal,
                         current_item->StartOffset());
  bool consumed = segmenter.Consume(&range);
  DCHECK(consumed);
  if (range.start == 0 && range.end == text_content.length()) {
    NGInlineItem::SetSegmentData(range, &items);
    return;
  }

  if (!data->segments) {
    data->segments = std::make_unique<NGInlineItemSegments>();
  }
  do {
    data->segments->Append(range);
  } while (segmenter.Consume(&range));
  DCHECK_EQ(range.end, text_content.length());
}

void NGInlineNode::SegmentFontOrientation(NGInlineNodeData* data) const {
  // Segment by orientation, only if vertical writing mode and items with
  // 'text-orientation: mixed'.
  if (GetLayoutBlockFlow()->IsHorizontalWritingMode())
    return;

  Vector<NGInlineItem>& items = data->items;
  if (items.IsEmpty())
    return;
  String& text_content = data->text_content;
  text_content.Ensure16Bit();

  // If we don't have |NGInlineItemSegments| yet, create a segment for the
  // entire content.
  const unsigned capacity = items.size() + text_content.length() / 10;
  NGInlineItemSegments* segments = data->segments.get();
  if (segments) {
    DCHECK(!data->segments->IsEmpty());
    data->segments->ReserveCapacity(capacity);
    DCHECK_EQ(text_content.length(), data->segments->EndOffset());
  }
  unsigned segment_index = 0;

  for (const NGInlineItem& item : items) {
    if (item.Type() == NGInlineItem::kText && item.Length() &&
        item.Style()->GetFont().GetFontDescription().Orientation() ==
            FontOrientation::kVerticalMixed) {
      if (!segments) {
        data->segments = std::make_unique<NGInlineItemSegments>();
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
void NGInlineNode::SegmentBidiRuns(NGInlineNodeData* data) const {
  if (!data->is_bidi_enabled_) {
    data->SetBaseDirection(TextDirection::kLtr);
    return;
  }

  NGBidiParagraph bidi;
  data->text_content.Ensure16Bit();
  if (!bidi.SetParagraph(data->text_content, Style())) {
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

  Vector<NGInlineItem>& items = data->items;
  unsigned item_index = 0;
  for (unsigned start = 0; start < data->text_content.length();) {
    UBiDiLevel level;
    unsigned end = bidi.GetLogicalRun(start, &level);
    DCHECK_EQ(items[item_index].start_offset_, start);
    item_index = NGInlineItem::SetBidiLevel(items, item_index, end, level);
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

void NGInlineNode::ShapeText(NGInlineItemsData* data,
                             const String* previous_text,
                             const Vector<NGInlineItem>* previous_items) const {
  TRACE_EVENT0("fonts", "NGInlineNode::ShapeText");
  const String& text_content = data->text_content;
  Vector<NGInlineItem>* items = &data->items;

  // Provide full context of the entire node to the shaper.
  ReusingTextShaper shaper(data, previous_items);
  ShapeResultSpacing<String> spacing(text_content);

  DCHECK(!data->segments ||
         data->segments->EndOffset() == text_content.length());

  for (unsigned index = 0; index < items->size();) {
    NGInlineItem& start_item = (*items)[index];
    if (start_item.Type() != NGInlineItem::kText || !start_item.Length()) {
      index++;
      continue;
    }

    const ComputedStyle& start_style = *start_item.Style();
    const Font& font = start_style.GetFont();
    TextDirection direction = start_item.Direction();
    unsigned end_index = index + 1;
    unsigned end_offset = start_item.EndOffset();

    // Symbol marker is painted as graphics. Create a ShapeResult of space
    // glyphs with the desired size to make it less special for line breaker.
    if (UNLIKELY(start_item.IsSymbolMarker())) {
      LayoutUnit symbol_width = ListMarker::WidthOfSymbol(start_style);
      DCHECK_GT(symbol_width, 0);
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
      const NGInlineItem& item = (*items)[end_index];

      if (item.Type() == NGInlineItem::kControl) {
        // Do not shape across control characters (line breaks, zero width
        // spaces, etc).
        break;
      }
      if (item.Type() == NGInlineItem::kText) {
        if (!item.Length())
          continue;
        if (ShouldBreakShapingBeforeText(item, start_item, start_style, font,
                                         direction)) {
          break;
        }
        // Break shaping at ZWNJ so that it prevents kerning. ZWNJ is always at
        // the beginning of an item for this purpose; see NGInlineItemsBuilder.
        if (text_content[item.StartOffset()] == kZeroWidthNonJoinerCharacter)
          break;
        end_offset = item.EndOffset();
        num_text_items++;
      } else if (item.Type() == NGInlineItem::kOpenTag) {
        if (ShouldBreakShapingBeforeBox(item, font)) {
          break;
        }
        // Should not have any characters to be opaque to shaping.
        DCHECK_EQ(0u, item.Length());
      } else if (item.Type() == NGInlineItem::kCloseTag) {
        if (ShouldBreakShapingAfterBox(item, font)) {
          break;
        }
        // Should not have any characters to be opaque to shaping.
        DCHECK_EQ(0u, item.Length());
      } else {
        break;
      }
    }

    // Shaping a single item. Skip if the existing results remain valid.
    if (previous_text && end_offset == start_item.EndOffset() &&
        !NeedsShaping(start_item)) {
      DCHECK_EQ(start_item.StartOffset(),
                start_item.TextShapeResult()->StartIndex());
      DCHECK_EQ(start_item.EndOffset(),
                start_item.TextShapeResult()->EndIndex());
      index++;
      continue;
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
    scoped_refptr<ShapeResult> shape_result =
        shaper.Shape(start_item, end_offset);

    if (UNLIKELY(spacing.SetSpacing(font))) {
      shape_result->ApplySpacing(spacing);
      if (spacing.LetterSpacing() &&
          ShouldReportLetterSpacingUseCounter(
              start_item.GetLayoutObject(),
              start_item.StyleVariant() == NGStyleVariant::kFirstLine,
              GetLayoutBlockFlow())) {
        UseCounter::Count(GetDocument(),
                          WebFeature::kLastLetterSpacingAffectsRendering);
      }
    }

    // If the text is from one item, use the ShapeResult as is.
    if (end_offset == start_item.EndOffset()) {
      start_item.shape_result_ = std::move(shape_result);
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
    Vector<ShapeResult::ShapeRange, 32> text_item_ranges;
    text_item_ranges.ReserveInitialCapacity(num_text_items);
    for (; index < end_index; index++) {
      NGInlineItem& item = (*items)[index];
      if (item.Type() != NGInlineItem::kText || !item.Length())
        continue;

      // We don't use SafeToBreak API here because this is not a line break.
      // The ShapeResult is broken into multiple results, but they must look
      // like they were not broken.
      //
      // When multiple code units shape to one glyph, such as ligatures, the
      // item that has its first code unit keeps the glyph.
      scoped_refptr<ShapeResult> item_result =
          ShapeResult::CreateEmpty(*shape_result.get());
      text_item_ranges.emplace_back(item.StartOffset(), item.EndOffset(),
                                    item_result.get());
      item.shape_result_ = std::move(item_result);
    }
    DCHECK_EQ(text_item_ranges.size(), num_text_items);
    shape_result->CopyRanges(text_item_ranges.data(), text_item_ranges.size());
  }

#if DCHECK_IS_ON()
  for (const NGInlineItem& item : *items) {
    if (item.Type() == NGInlineItem::kText && item.Length()) {
      DCHECK(item.TextShapeResult());
      DCHECK_EQ(item.TextShapeResult()->StartIndex(), item.StartOffset());
      DCHECK_EQ(item.TextShapeResult()->EndIndex(), item.EndOffset());
    }
  }
#endif
}

// Create Vector<NGInlineItem> with :first-line rules applied if needed.
void NGInlineNode::ShapeTextForFirstLineIfNeeded(NGInlineNodeData* data) const {
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

  auto first_line_items = std::make_unique<NGInlineItemsData>();
  first_line_items->text_content = data->text_content;
  bool needs_reshape = false;
  if (first_line_style->TextTransform() != block_style->TextTransform()) {
    // TODO(kojii): This logic assumes that text-transform is applied only to
    // ::first-line, and does not work when the base style has text-transform
    // and ::first-line has different text-transform.
    first_line_style->ApplyTextTransform(&first_line_items->text_content);
    if (first_line_items->text_content != data->text_content) {
      // TODO(kojii): When text-transform changes the length, we need to adjust
      // offset in NGInlineItem, or re-collect inlines. Other classes such as
      // line breaker need to support the scenario too. For now, we force the
      // string to be the same length to prevent them from crashing. This may
      // result in a missing or a duplicate character if the length changes.
      TruncateOrPadText(&first_line_items->text_content,
                        data->text_content.length());
      needs_reshape = true;
    }
  }

  first_line_items->items.AppendVector(data->items);
  for (auto& item : first_line_items->items) {
    item.SetStyleVariant(NGStyleVariant::kFirstLine);
  }

  // Re-shape if the font is different.
  if (needs_reshape || FirstLineNeedsReshape(*first_line_style, *block_style))
    ShapeText(first_line_items.get());

  data->first_line_items_ = std::move(first_line_items);
}

void NGInlineNode::AssociateItemsWithInlines(NGInlineNodeData* data) const {
#if DCHECK_IS_ON()
  HashSet<LayoutObject*> associated_objects;
#endif
  Vector<NGInlineItem>& items = data->items;
  for (NGInlineItem* item = items.begin(); item != items.end();) {
    LayoutObject* object = item->GetLayoutObject();
    if (auto* layout_text = DynamicTo<LayoutNGText>(object)) {
#if DCHECK_IS_ON()
      // Items split from a LayoutObject should be consecutive.
      DCHECK(associated_objects.insert(object).is_new_entry);
#endif
      layout_text->ClearHasBidiControlInlineItems();
      bool has_bidi_control = false;
      NGInlineItem* begin = item;
      for (++item; item != items.end(); ++item) {
        if (item->GetLayoutObject() != object)
          break;
        if (item->Type() == NGInlineItem::kBidiControl)
          has_bidi_control = true;
      }
      layout_text->SetInlineItems(begin, item);
      if (has_bidi_control)
        layout_text->SetHasBidiControlInlineItems();
      continue;
    }
    ++item;
  }
}

void NGInlineNode::ClearAssociatedFragments(
    const NGPhysicalFragment& fragment,
    const NGBlockBreakToken* block_break_token) {
  auto* block_flow = To<LayoutBlockFlow>(fragment.GetMutableLayoutObject());
  if (!block_flow->ChildrenInline())
    return;
  DCHECK(AreNGBlockFlowChildrenInline(block_flow));
  NGInlineNode node = NGInlineNode(block_flow);

  DCHECK(node.IsPrepareLayoutFinished());
  const Vector<NGInlineItem>& items = node.MaybeDirtyData().items;

  unsigned start_index;
  if (!block_break_token) {
    start_index = 0;
  } else {
    // TODO(kojii): Not fully supported, need more logic when the block is
    // fragmented, because one inline LayoutObject may span across
    // fragmentainers.
    // TODO(kojii): Not sure if using |block_break_token->InputNode()| is
    // correct for multicol. Should verify and somehow get NGInlineNode from it.
    // Also change |InlineBreakTokenFor| to receive NGInlineNode instead of
    // NGLayoutInputNode once this is done.
    const NGInlineBreakToken* inline_break_token =
        block_break_token->InlineBreakTokenFor(block_break_token->InputNode());
    // TODO(kojii): This needs to investigate in what case this happens. It's
    // probably wrong to create NGPaintFragment when there's no inline break
    // token.
    if (!inline_break_token)
      return;
    start_index = inline_break_token->ItemIndex();
  }

  LayoutObject* last_object = nullptr;
  for (unsigned i = start_index; i < items.size(); i++) {
    const NGInlineItem& item = items[i];
    if (item.Type() == NGInlineItem::kFloating ||
        item.Type() == NGInlineItem::kOutOfFlowPositioned) {
      // These items are not associated and that no need to clear.
      DCHECK(!item.GetLayoutObject()->FirstInlineFragment());
      continue;
    }
    LayoutObject* object = item.GetLayoutObject();
    if (!object || object == last_object)
      continue;
    object->SetFirstInlineFragment(nullptr);
    last_object = object;
  }
}

scoped_refptr<const NGLayoutResult> NGInlineNode::Layout(
    const NGConstraintSpace& constraint_space,
    const NGBreakToken* break_token,
    NGInlineChildLayoutContext* context) const {
  PrepareLayoutIfNeeded();

  const auto* inline_break_token = To<NGInlineBreakToken>(break_token);
  NGInlineLayoutAlgorithm algorithm(*this, constraint_space, inline_break_token,
                                    context);
  auto layout_result = algorithm.Layout();

#if defined(OS_ANDROID)
  if (!RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled()) {
    // Cached position data is crucial for line breaking performance and is
    // preserved across layouts to speed up subsequent layout passes due to
    // reflow, page zoom, window resize, etc. On Android though reflows are less
    // common, page zoom isn't used (instead uses pinch-zoom), and the window
    // typically can't be resized (apart from rotation). To reduce memory usage
    // discard the cached position data after layout.
    // TODO(crbug.com/1042604): FragmentItem should save memory enough to re-
    // enable the position cache.
    NGInlineNodeData* data = MutableData();
    for (auto& item : data->items) {
      if (item.shape_result_)
        item.shape_result_->DiscardPositionData();
    }
  }
#endif  // defined(OS_ANDROID)

  return layout_result;
}

namespace {

template <typename CharType>
scoped_refptr<StringImpl> CreateTextContentForStickyImagesQuirk(
    const CharType* text,
    unsigned length,
    base::span<const NGInlineItem> items) {
  StringBuffer<CharType> buffer(length);
  CharType* characters = buffer.Characters();
  memcpy(characters, text, length * sizeof(CharType));
  for (const NGInlineItem& item : items) {
    if (item.Type() == NGInlineItem::kAtomicInline && item.IsImage()) {
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
String NGInlineNode::TextContentForStickyImagesQuirk(
    const NGInlineItemsData& items_data) {
  const String& text_content = items_data.text_content;
  for (const NGInlineItem& item : items_data.items) {
    if (item.Type() == NGInlineItem::kAtomicInline && item.IsImage()) {
      if (text_content.Is8Bit()) {
        return CreateTextContentForStickyImagesQuirk(
            text_content.Characters8(), text_content.length(),
            base::span<const NGInlineItem>(&item, items_data.items.end()));
      }
      return CreateTextContentForStickyImagesQuirk(
          text_content.Characters16(), text_content.length(),
          base::span<const NGInlineItem>(&item, items_data.items.end()));
    }
  }
  return text_content;
}

static LayoutUnit ComputeContentSize(
    NGInlineNode node,
    WritingMode container_writing_mode,
    const MinMaxSizesInput& input,
    NGLineBreakerMode mode,
    NGLineBreaker::MaxSizeCache* max_size_cache,
    base::Optional<LayoutUnit>* max_size_out,
    bool* depends_on_percentage_block_size_out) {
  const ComputedStyle& style = node.Style();
  LayoutUnit available_inline_size =
      mode == NGLineBreakerMode::kMaxContent ? LayoutUnit::Max() : LayoutUnit();

  NGConstraintSpaceBuilder builder(
      /* parent_writing_mode */ style.GetWritingMode(),
      style.GetWritingDirection(),
      /* is_new_fc */ false);
  builder.SetAvailableSize({available_inline_size, kIndefiniteSize});
  builder.SetPercentageResolutionSize({LayoutUnit(), LayoutUnit()});
  builder.SetReplacedPercentageResolutionSize({LayoutUnit(), LayoutUnit()});
  NGConstraintSpace space = builder.ToConstraintSpace();

  NGExclusionSpace empty_exclusion_space;
  NGPositionedFloatVector empty_leading_floats;
  NGLineLayoutOpportunity line_opportunity(available_inline_size);
  LayoutUnit result;
  NGLineBreaker line_breaker(node, mode, space, line_opportunity,
                             empty_leading_floats,
                             /* handled_leading_floats_index */ 0u,
                             /* break_token */ nullptr, &empty_exclusion_space);
  line_breaker.SetIntrinsicSizeOutputs(max_size_cache,
                                       depends_on_percentage_block_size_out);
  const NGInlineItemsData& items_data = line_breaker.ItemsData();

  // Computes max-size for floats in inline formatting context.
  class FloatsMaxSize {
    STACK_ALLOCATED();

   public:
    explicit FloatsMaxSize(const MinMaxSizesInput& input)
        : floats_inline_size_(input.float_left_inline_size +
                              input.float_right_inline_size) {
      DCHECK_GE(floats_inline_size_, 0);
    }

    void AddFloat(const ComputedStyle& float_style,
                  const ComputedStyle& style,
                  LayoutUnit float_inline_max_size_with_margin) {
      floating_objects_.push_back(NGInlineNode::FloatingObject{
          float_style, style, float_inline_max_size_with_margin});
    }

    LayoutUnit ComputeMaxSizeForLine(LayoutUnit line_inline_size,
                                     LayoutUnit max_inline_size) {
      if (floating_objects_.IsEmpty())
        return std::max(max_inline_size, line_inline_size);

      EFloat previous_float_type = EFloat::kNone;
      for (const auto& floating_object : floating_objects_) {
        const EClear float_clear =
            floating_object.float_style.Clear(floating_object.style);

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
            floating_object.float_style.Floating(floating_object.style);
      }
      max_inline_size =
          std::max(max_inline_size, line_inline_size + floats_inline_size_);
      floats_inline_size_ = LayoutUnit();
      floating_objects_.Shrink(0);
      return max_inline_size;
    }

   private:
    LayoutUnit floats_inline_size_;
    HeapVector<NGInlineNode::FloatingObject, 4> floating_objects_;
  };

  // This struct computes the max size from the line break results for the min
  // size.
  struct MaxSizeFromMinSize {
    STACK_ALLOCATED();

   public:
    LayoutUnit position;
    LayoutUnit max_size;
    const NGInlineItemsData& items_data;
    const NGInlineItem* next_item;
    const NGLineBreaker::MaxSizeCache& max_size_cache;
    FloatsMaxSize* floats;
    bool is_after_break = true;

    explicit MaxSizeFromMinSize(
        const NGInlineItemsData& items_data,
        const NGLineBreaker::MaxSizeCache& max_size_cache,
        FloatsMaxSize* floats)
        : items_data(items_data),
          next_item(items_data.items.begin()),
          max_size_cache(max_size_cache),
          floats(floats) {}

    // Add all text items up to |end|. The line break results for min size
    // may break text into multiple lines, and may remove trailing spaces. For
    // max size, use the original text widths from NGInlineItem instead.
    void AddTextUntil(const NGInlineItem* end) {
      DCHECK(end);
      for (; next_item != end; ++next_item) {
        if (next_item->Type() == NGInlineItem::kText && next_item->Length()) {
          DCHECK(next_item->TextShapeResult());
          const ShapeResult& shape_result = *next_item->TextShapeResult();
          position += shape_result.SnappedWidth().ClampNegativeToZero();
        }
      }
    }

    void ForceLineBreak(const NGLineInfo& line_info) {
      // Add all text up to the end of the line. There may be spaces that were
      // removed during the line breaking.
      CHECK_LE(line_info.EndItemIndex(), items_data.items.size());
      AddTextUntil(items_data.items.begin() + line_info.EndItemIndex());
      max_size = floats->ComputeMaxSizeForLine(position.ClampNegativeToZero(),
                                               max_size);
      position = LayoutUnit();
      is_after_break = true;
    }

    void AddTabulationCharacters(const NGInlineItem& item, unsigned length) {
      DCHECK_GE(length, 1u);
      AddTextUntil(&item);
      DCHECK(item.Style());
      const ComputedStyle& style = *item.Style();
      const Font& font = style.GetFont();
      const SimpleFontData* font_data = font.PrimaryFont();
      const TabSize& tab_size = style.GetTabSize();
      float advance = font.TabWidth(font_data, tab_size, position);
      DCHECK_GE(length, 1u);
      if (length > 1u)
        advance += font.TabWidth(font_data, tab_size) * (length - 1);
      position += LayoutUnit::FromFloatCeil(advance).ClampNegativeToZero();
    }

    LayoutUnit Finish(const NGInlineItem* end) {
      AddTextUntil(end);
      return floats->ComputeMaxSizeForLine(position.ClampNegativeToZero(),
                                           max_size);
    }

    void ComputeFromMinSize(const NGLineInfo& line_info) {
      if (is_after_break) {
        position += line_info.TextIndent();
        is_after_break = false;
      }

      bool has_forced_break = false;
      for (const NGInlineItemResult& result : line_info.Results()) {
        const NGInlineItem& item = *result.item;
        if (item.Type() == NGInlineItem::kText) {
          // Text in NGInlineItemResult may be wrapped and trailing spaces
          // may be removed. Ignore them, but add text later from
          // NGInlineItem.
          continue;
        }
        if (item.Type() == NGInlineItem::kAtomicInline) {
          // The max-size for atomic inlines are cached in |max_size_cache|.
          unsigned item_index = &item - items_data.items.begin();
          position += max_size_cache[item_index];
          continue;
        }
        if (item.Type() == NGInlineItem::kControl) {
          UChar c = items_data.text_content[item.StartOffset()];
          if (c == kNewlineCharacter) {
            // Compute the forced break after all results were handled, because
            // when close tags appear after a forced break, they are included in
            // the line, and they may have inline sizes. crbug.com/991320.
            DCHECK(!has_forced_break);
            has_forced_break = true;
            continue;
          }
          // Tabulation characters change the widths by their positions, so
          // their widths for the max size may be different from the widths for
          // the min size. Fall back to 2 pass for now.
          if (c == kTabulationCharacter) {
            AddTabulationCharacters(item, result.Length());
            continue;
          }
        }
        position += result.inline_size;
      }
      if (has_forced_break)
        ForceLineBreak(line_info);
    }
  };
  FloatsMaxSize floats_max_size(input);
  bool can_compute_max_size_from_min_size = true;
  MaxSizeFromMinSize max_size_from_min_size(items_data, *max_size_cache,
                                            &floats_max_size);

  do {
    NGLineInfo line_info;
    line_breaker.NextLine(input.percentage_resolution_block_size, &line_info);
    if (line_info.Results().IsEmpty())
      break;

    LayoutUnit inline_size = line_info.Width();
    for (const NGInlineItemResult& item_result : line_info.Results()) {
      DCHECK(item_result.item);
      const NGInlineItem& item = *item_result.item;
      if (item.Type() != NGInlineItem::kFloating)
        continue;
      LayoutObject* floating_object = item.GetLayoutObject();
      DCHECK(floating_object && floating_object->IsFloating());

      NGBlockNode float_node(To<LayoutBox>(floating_object));
      const ComputedStyle& float_style = float_node.Style();

      // Floats don't intrude into floats.
      MinMaxSizesInput float_input(input.percentage_resolution_block_size,
                                   MinMaxSizesType::kContent);
      MinMaxSizesResult child_result =
          ComputeMinAndMaxContentContribution(style, float_node, float_input);
      LayoutUnit child_inline_margins =
          ComputeMinMaxMargins(style, float_node).InlineSum();

      if (depends_on_percentage_block_size_out) {
        *depends_on_percentage_block_size_out |=
            child_result.depends_on_percentage_block_size;
      }

      if (mode == NGLineBreakerMode::kMinContent) {
        result = std::max(result,
                          child_result.sizes.min_size + child_inline_margins);
      }
      floats_max_size.AddFloat(
          float_style, style,
          child_result.sizes.max_size + child_inline_margins);
    }

    if (mode == NGLineBreakerMode::kMinContent) {
      result = std::max(result, inline_size);
      can_compute_max_size_from_min_size =
          can_compute_max_size_from_min_size &&
          // `box-decoration-break: clone` clones box decorations to each
          // fragment (line) that we cannot compute max-content from
          // min-content.
          !line_breaker.HasClonedBoxDecorations();
      if (can_compute_max_size_from_min_size)
        max_size_from_min_size.ComputeFromMinSize(line_info);
    } else {
      result = floats_max_size.ComputeMaxSizeForLine(inline_size, result);
    }
  } while (!line_breaker.IsFinished());

  if (mode == NGLineBreakerMode::kMinContent &&
      can_compute_max_size_from_min_size) {
    *max_size_out = max_size_from_min_size.Finish(items_data.items.end());
    // Check the max size matches to the value computed from 2 pass.
#if DCHECK_IS_ON()
    LayoutUnit content_size = ComputeContentSize(
        node, container_writing_mode, input, NGLineBreakerMode::kMaxContent,
        max_size_cache, nullptr, nullptr);
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

MinMaxSizesResult NGInlineNode::ComputeMinMaxSizes(
    WritingMode container_writing_mode,
    const MinMaxSizesInput& input,
    const NGConstraintSpace* constraint_space) const {
  PrepareLayoutIfNeeded();

  // Compute the max of inline sizes of all line boxes with 0 available inline
  // size. This gives the min-content, the width where lines wrap at every
  // break opportunity.
  NGLineBreaker::MaxSizeCache max_size_cache;
  MinMaxSizes sizes;
  base::Optional<LayoutUnit> max_size;
  bool depends_on_percentage_block_size = false;
  sizes.min_size = ComputeContentSize(
      *this, container_writing_mode, input, NGLineBreakerMode::kMinContent,
      &max_size_cache, &max_size, &depends_on_percentage_block_size);
  if (max_size) {
    sizes.max_size = *max_size;
  } else {
    sizes.max_size = ComputeContentSize(*this, container_writing_mode, input,
                                        NGLineBreakerMode::kMaxContent,
                                        &max_size_cache, nullptr, nullptr);
  }

  // Negative text-indent can make min > max. Ensure min is the minimum size.
  sizes.min_size = std::min(sizes.min_size, sizes.max_size);

  return {sizes, depends_on_percentage_block_size};
}

bool NGInlineNode::UseFirstLineStyle() const {
  return GetLayoutBox() &&
         GetLayoutBox()->GetDocument().GetStyleEngine().UsesFirstLineRules();
}

void NGInlineNode::CheckConsistency() const {
#if DCHECK_IS_ON()
  const Vector<NGInlineItem>& items = Data().items;
  for (const NGInlineItem& item : items) {
    DCHECK(!item.GetLayoutObject() || !item.Style() ||
           item.Style() == item.GetLayoutObject()->Style());
  }
#endif
}

bool NGInlineNode::ShouldReportLetterSpacingUseCounterForTesting(
    const LayoutObject* layout_object,
    bool first_line,
    const LayoutBlockFlow* block_flow) {
  return ShouldReportLetterSpacingUseCounter(layout_object, first_line,
                                             block_flow);
}

String NGInlineNode::ToString() const {
  return "NGInlineNode";
}

}  // namespace blink
