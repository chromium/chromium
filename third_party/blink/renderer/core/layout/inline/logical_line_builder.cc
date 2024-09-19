// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/logical_line_builder.h"

#include <algorithm>

#include "base/containers/adapters.h"
#include "third_party/blink/renderer/core/layout/disable_layout_side_effects_scope.h"
#include "third_party/blink/renderer/core/layout/inline/inline_box_state.h"
#include "third_party/blink/renderer/core/layout/inline/inline_child_layout_context.h"
#include "third_party/blink/renderer/core/layout/inline/inline_item_result_ruby_column.h"
#include "third_party/blink/renderer/core/layout/inline/inline_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/inline/inline_node.h"
#include "third_party/blink/renderer/core/layout/inline/justification_utils.h"
#include "third_party/blink/renderer/core/layout/inline/line_breaker.h"
#include "third_party/blink/renderer/core/layout/inline/line_info.h"
#include "third_party/blink/renderer/core/layout/inline/logical_line_item.h"
#include "third_party/blink/renderer/core/layout/inline/ruby_utils.h"
#include "third_party/blink/renderer/core/layout/layout_text_combine.h"
#include "third_party/blink/renderer/core/layout/logical_box_fragment.h"
#include "third_party/blink/renderer/platform/text/bidi_paragraph.h"

namespace blink {

LogicalLineBuilder::LogicalLineBuilder(InlineNode node,
                                       const ConstraintSpace& constraint_space,
                                       const InlineBreakToken* break_token,
                                       InlineLayoutStateStack* state_stack,
                                       InlineChildLayoutContext* context)
    : node_(node),
      constraint_space_(constraint_space),
      break_token_(break_token),
      box_states_(state_stack),
      context_(context),
      baseline_type_(node.Style().GetFontBaseline()),
      quirks_mode_(node.GetDocument().InLineHeightQuirksMode()) {}

void LogicalLineBuilder::CreateLine(LineInfo* line_info,
                                    LogicalLineItems* line_box,
                                    InlineLayoutAlgorithm* main_line_helper) {
  // Needs MutableResults to move ShapeResult out of the LineInfo.
  InlineItemResults* line_items = line_info->MutableResults();

  // Compute heights of all inline items by placing the dominant baseline at 0.
  // The baseline is adjusted after the height of the line box is computed.
  const ComputedStyle& line_style = line_info->LineStyle();
  box_states_->SetIsEmptyLine(line_info->IsEmptyLine());
  InlineBoxState* box = box_states_->OnBeginPlaceItems(
      node_, line_style, baseline_type_, quirks_mode_, line_box);
#if EXPENSIVE_DCHECKS_ARE_ON()
  if (main_line_helper) {
    main_line_helper->CheckBoxStates(*line_info);
  }
#endif

  // List items trigger strict line height, i.e. we make room for the line box
  // strut, for *every* line. This matches other browsers. The intention may
  // have been to make sure that there's always room for the list item marker,
  // but that doesn't explain why it's done for every line...
  if (quirks_mode_ && ComputedStyle::IsDisplayListItem(line_style.Display())) {
    box->ComputeTextMetrics(line_style, *box->font, baseline_type_);
  }

#if DCHECK_IS_ON()
  if (line_info->IsBlockInInline()) {
    DCHECK_EQ(line_items->size(), 1u);
    DCHECK_EQ((*line_items)[0].item->Type(), InlineItem::kBlockInInline);
  }
#endif
  box = HandleItemResults(*line_info, *line_items, line_box, main_line_helper,
                          box);

  box_states_->OnEndPlaceItems(constraint_space_, line_box, baseline_type_);

  if (node_.IsBidiEnabled()) [[unlikely]] {
    box_states_->PrepareForReorder(line_box);
    BidiReorder(line_info->BaseDirection(), line_box,
                box_states_->RubyColumnList());
    box_states_->UpdateAfterReorder(line_box);
  } else {
    DCHECK(IsLtr(line_info->BaseDirection()));
  }

  for (auto& logical_column : box_states_->RubyColumnList()) {
    std::pair<LayoutUnit, LayoutUnit>& insets = logical_column->base_insets;
    ApplyLeftAndRightExpansion(
        insets.first, insets.second,
        base::span(*line_box).subspan(logical_column->start_index,
                                      logical_column->size));
  }
}

InlineBoxState* LogicalLineBuilder::HandleItemResults(
    const LineInfo& line_info,
    InlineItemResults& line_items,
    LogicalLineItems* line_box,
    InlineLayoutAlgorithm* main_line_helper,
    InlineBoxState* box) {
  for (InlineItemResult& item_result : line_items) {
    DCHECK(item_result.item);
    const InlineItem& item = *item_result.item;
    if (item.Type() == InlineItem::kText) {
      DCHECK(item.GetLayoutObject());
      DCHECK(item.GetLayoutObject()->IsText() ||
             item.GetLayoutObject()->IsLayoutListItem());

      if (!item_result.Length()) [[unlikely]] {
        // Empty or fully collapsed text isn't needed for layout, but needs
        // `ClearNeedsLayout`. See `LineBreaker::HandleEmptyText`.
        LayoutObject* layout_object = item.GetLayoutObject();
        if (layout_object->NeedsLayout()) {
          layout_object->ClearNeedsLayout();
        }
        continue;
      }
      DCHECK(item_result.shape_result);

      if (quirks_mode_) [[unlikely]] {
        box->EnsureTextMetrics(*item.Style(), *box->font, baseline_type_);
      }

      // Take all used fonts into account if 'line-height: normal'.
      if (box->include_used_fonts) {
        box->AccumulateUsedFonts(item_result.shape_result.Get());
      }

      DCHECK(item.TextType() == TextItemType::kNormal ||
             item.TextType() == TextItemType::kSymbolMarker);
      if (item_result.is_hyphenated) [[unlikely]] {
        DCHECK(item_result.hyphen);
        LayoutUnit hyphen_inline_size = item_result.hyphen.InlineSize();
        line_box->AddChild(item, item_result, item_result.TextOffset(),
                           box->text_top,
                           item_result.inline_size - hyphen_inline_size,
                           box->text_height, item.BidiLevel());
        PlaceHyphen(item_result, hyphen_inline_size, line_box, box);
      } else if (node_.IsTextCombine()) [[unlikely]] {
        // We make combined text at block offset 0 with 1em height.
        // Painter paints text at block offset + |font.internal_leading / 2|.
        const auto one_em = item.Style()->ComputedFontSizeAsFixed();
        const auto text_height = one_em;
        const auto text_top = LayoutUnit();
        line_box->AddChild(item, item_result, item_result.TextOffset(),
                           text_top, item_result.inline_size, text_height,
                           item.BidiLevel());
      } else {
        line_box->AddChild(item, item_result, item_result.TextOffset(),
                           box->text_top, item_result.inline_size,
                           box->text_height, item.BidiLevel());
      }

      // Text boxes always need full paint invalidations.
      item.GetLayoutObject()->ClearNeedsLayoutWithFullPaintInvalidation();

    } else if (item.Type() == InlineItem::kControl) {
      PlaceControlItem(item, line_info.ItemsData().text_content, &item_result,
                       line_box, box);
    } else if (item.Type() == InlineItem::kOpenTag) {
      box = HandleOpenTag(item, item_result, line_box);
    } else if (item.Type() == InlineItem::kCloseTag) {
      box = HandleCloseTag(item, item_result, line_box, box);
    } else if (item.Type() == InlineItem::kAtomicInline) {
      box = PlaceAtomicInline(item, &item_result, line_box);
      has_relative_positioned_items_ |=
          item.Style()->GetPosition() == EPosition::kRelative;
    } else if (item.Type() == InlineItem::kBlockInInline) {
      DCHECK(line_info.IsBlockInInline());
      DCHECK(main_line_helper);
      main_line_helper->PlaceBlockInInline(item, &item_result, line_box);
    } else if (item.Type() == InlineItem::kOpenRubyColumn) {
      DCHECK(RuntimeEnabledFeatures::RubyLineBreakableEnabled());
      if (item_result.ruby_column) {
        box = PlaceRubyColumn(line_info, item_result, *line_box, box);
      } else {
        line_box->AddChild(item.BidiLevel());
      }
    } else if (item.Type() == InlineItem::kCloseRubyColumn) {
      DCHECK(RuntimeEnabledFeatures::RubyLineBreakableEnabled());
      line_box->AddChild(item.BidiLevel());
    } else if (item.Type() == InlineItem::kRubyLinePlaceholder) {
      DCHECK(RuntimeEnabledFeatures::RubyLineBreakableEnabled());
      // Overhang values are zero or negative.
      LayoutUnit start_overhang = item_result.margins.inline_start;
      LayoutUnit end_overhang = item_result.margins.inline_end;
      // Adds a LogicalLineItem with an InlineItem to check its
      // InlineItemType later.
      line_box->AddChild(
          item, item_result, item_result.TextOffset(),
          /* block_offset */ LayoutUnit(),
          item_result.inline_size + start_overhang + end_overhang,
          /* text_height */ LayoutUnit(), item.BidiLevel());
      (*line_box)[line_box->size() - 1].rect.offset.inline_offset =
          start_overhang;
    } else if (item.Type() == InlineItem::kListMarker) {
      PlaceListMarker(item, &item_result);
    } else if (item.Type() == InlineItem::kOutOfFlowPositioned) {
      // An inline-level OOF child positions itself based on its direction, a
      // block-level OOF child positions itself based on the direction of its
      // block-level container.
      TextDirection direction =
          item.GetLayoutObject()->StyleRef().IsOriginalDisplayInlineType()
              ? item.Direction()
              : constraint_space_.Direction();

      line_box->AddChild(item.GetLayoutObject(), item.BidiLevel(), direction);
      has_out_of_flow_positioned_items_ = true;
    } else if (item.Type() == InlineItem::kFloating) {
      if (item_result.positioned_float) {
        if (!item_result.positioned_float->break_before_token) {
          DCHECK(item_result.positioned_float->layout_result);
          line_box->AddChild(item_result.positioned_float->layout_result,
                             item_result.positioned_float->bfc_offset,
                             item.BidiLevel());
        }
      } else {
        line_box->AddChild(item.GetLayoutObject(), item.BidiLevel(),
                           item_result.Start());
      }
      has_floating_items_ = true;
      has_relative_positioned_items_ |=
          item.Style()->GetPosition() == EPosition::kRelative;
    } else if (item.Type() == InlineItem::kBidiControl) {
      line_box->AddChild(item.BidiLevel());
    } else if (item.Type() == InlineItem::kInitialLetterBox) [[unlikely]] {
      // The initial letter does not increase the logical height of the line
      // box in which it participates[1]. So, we should not changes
      // `InlineBoxState::metrics`, or not call ` ComputeTextMetrics()` to
      // incorporate from `ComputedStyle::GetFont()` of the initial letter box.
      // See also `LineInfo::ComputeTotalBlockSize()` for calculation of
      // layout opportunities.
      // [1] https://drafts.csswg.org/css-inline/#initial-letter-block-position
      DCHECK(!initial_letter_item_result_);
      initial_letter_item_result_ = &item_result;
      PlaceInitialLetterBox(item, &item_result, line_box);
    }
  }
  return box;
}

InlineBoxState* LogicalLineBuilder::HandleOpenTag(
    const InlineItem& item,
    const InlineItemResult& item_result,
    LogicalLineItems* line_box) {
  InlineBoxState* box = box_states_->OnOpenTag(
      constraint_space_, item, item_result, baseline_type_, line_box);
  // Compute text metrics for all inline boxes since even empty inlines
  // influence the line height, except when quirks mode and the box is empty
  // for the purpose of empty block calculation.
  // https://drafts.csswg.org/css2/visudet.html#line-height
  if (!quirks_mode_ || !item.IsEmptyItem()) {
    box->ComputeTextMetrics(*item.Style(), *box->font, baseline_type_);
  }

  if (item.Style()->HasMask()) {
    // Layout may change the bounding box, which affects MaskClip.
    if (LayoutObject* object = item.GetLayoutObject()) {
      object->SetNeedsPaintPropertyUpdate();
    }
  }

  return box;
}

InlineBoxState* LogicalLineBuilder::HandleCloseTag(
    const InlineItem& item,
    const InlineItemResult& item_result,
    LogicalLineItems* line_box,
    InlineBoxState* box) {
  if (quirks_mode_ && !item.IsEmptyItem()) [[unlikely]] {
    box->EnsureTextMetrics(*item.Style(), *box->font, baseline_type_);
  }
  box =
      box_states_->OnCloseTag(constraint_space_, line_box, box, baseline_type_);
  // Just clear |NeedsLayout| flags. Culled inline boxes do not need paint
  // invalidations. If this object produces box fragments,
  // |InlineBoxStateStack| takes care of invalidations.
  if (!DisableLayoutSideEffectsScope::IsDisabled()) {
    item.GetLayoutObject()->ClearNeedsLayoutWithoutPaintInvalidation();
  }
  return box;
}

void LogicalLineBuilder::PlaceControlItem(const InlineItem& item,
                                          const String& text_content,
                                          InlineItemResult* item_result,
                                          LogicalLineItems* line_box,
                                          InlineBoxState* box) {
  DCHECK_EQ(item.Type(), InlineItem::kControl);
  DCHECK_GE(item.Length(), 1u);
  DCHECK(!item.TextShapeResult());
  DCHECK_NE(item.TextType(), TextItemType::kNormal);
#if DCHECK_IS_ON()
  item.CheckTextType(text_content);
#endif

  // Don't generate fragments if this is a generated (not in DOM) break
  // opportunity during the white space collapsing in InlineItemBuilder.
  if (item.IsGeneratedForLineBreak()) [[unlikely]] {
    return;
  }

  DCHECK(item.GetLayoutObject());
  DCHECK(item.GetLayoutObject()->IsText());
  if (!DisableLayoutSideEffectsScope::IsDisabled()) {
    item.GetLayoutObject()->ClearNeedsLayoutWithFullPaintInvalidation();
  }

  if (!item_result->Length()) [[unlikely]] {
    // Empty or fully collapsed text isn't needed for layout, but needs
    // `ClearNeedsLayout`. See `LineBreaker::HandleEmptyText`.
    return;
  }

  if (quirks_mode_ && !box->HasMetrics()) [[unlikely]] {
    box->EnsureTextMetrics(*item.Style(), *box->font, baseline_type_);
  }

  line_box->AddChild(item, std::move(item_result->shape_result),
                     item_result->TextOffset(), box->text_top,
                     item_result->inline_size, box->text_height,
                     item.BidiLevel());
}

void LogicalLineBuilder::PlaceHyphen(const InlineItemResult& item_result,
                                     LayoutUnit hyphen_inline_size,
                                     LogicalLineItems* line_box,
                                     InlineBoxState* box) {
  DCHECK(item_result.item);
  DCHECK(item_result.is_hyphenated);
  DCHECK(item_result.hyphen);
  DCHECK_EQ(hyphen_inline_size, item_result.hyphen.InlineSize());
  const InlineItem& item = *item_result.item;
  line_box->AddChild(
      item, ShapeResultView::Create(&item_result.hyphen.GetShapeResult()),
      item_result.hyphen.Text(), box->text_top, hyphen_inline_size,
      box->text_height, item.BidiLevel());
}

InlineBoxState* LogicalLineBuilder::PlaceAtomicInline(
    const InlineItem& item,
    InlineItemResult* item_result,
    LogicalLineItems* line_box) {
  DCHECK(item_result->layout_result);

  // Reset the ellipsizing state. Atomic inline is monolithic.
  LayoutObject* layout_object = item.GetLayoutObject();
  DCHECK(layout_object);
  DCHECK(layout_object->IsAtomicInlineLevel());
  DCHECK(To<LayoutBox>(layout_object)->IsMonolithic());
  layout_object->SetIsTruncated(false);

  InlineBoxState* box = box_states_->OnOpenTag(
      constraint_space_, item, *item_result, baseline_type_, *line_box);

  if (!IsA<LayoutTextCombine>(layout_object)) [[likely]] {
    PlaceLayoutResult(item_result, line_box, box,
                      box->margins.inline_start + item_result->spacing_before);
  } else {
    // The metrics should be as text instead of atomic inline box.
    const auto& style = layout_object->Parent()->StyleRef();
    box->ComputeTextMetrics(style, style.GetFont(), baseline_type_);
    // Note: |item_result->spacing_before| is non-zero if this |item_result|
    // is |LayoutTextCombine| and after CJK character.
    // See "text-combine-justify.html".
    const LayoutUnit inline_offset =
        box->margins.inline_start + item_result->spacing_before;
    line_box->AddChild(std::move(item_result->layout_result),
                       LogicalOffset{inline_offset, box->text_top},
                       item_result->inline_size, /* children_count */ 0,
                       item.BidiLevel());
  }
  return box_states_->OnCloseTag(constraint_space_, line_box, box,
                                 baseline_type_);
}

// Place a LayoutResult into the line box.
void LogicalLineBuilder::PlaceLayoutResult(InlineItemResult* item_result,
                                           LogicalLineItems* line_box,
                                           InlineBoxState* box,
                                           LayoutUnit inline_offset) {
  DCHECK(item_result->layout_result);
  DCHECK(item_result->item);
  const InlineItem& item = *item_result->item;
  DCHECK(item.Style());
  FontHeight metrics =
      LogicalBoxFragment(constraint_space_.GetWritingDirection(),
                         To<PhysicalBoxFragment>(
                             item_result->layout_result->GetPhysicalFragment()))
          .BaselineMetrics(item_result->margins, baseline_type_);
  if (box) {
    box->metrics.Unite(metrics);
  }

  LayoutUnit line_top = item_result->margins.line_over - metrics.ascent;
  line_box->AddChild(std::move(item_result->layout_result),
                     LogicalOffset{inline_offset, line_top},
                     item_result->inline_size, /* children_count */ 0,
                     item.BidiLevel());
}

void LogicalLineBuilder::PlaceInitialLetterBox(const InlineItem& item,
                                               InlineItemResult* item_result,
                                               LogicalLineItems* line_box) {
  DCHECK(item_result->layout_result);
  DCHECK(!IsA<LayoutTextCombine>(item.GetLayoutObject()));
  DCHECK(!item_result->spacing_before);

  // Because of the initial letter box should not contribute baseline position
  // to surrounding text, we should not update `InlineBoxState` for avoiding
  // to affect `line_box_metrics`.
  //
  // Note: `item.Style()` which holds style of `<::first-letter>` should not be
  // include in `InlineBoxState::font_metrics` and `metrics`, because they
  // don't affect baseline of surrounding text.
  line_box->AddChild(
      std::move(item_result->layout_result),
      LogicalOffset{item_result->margins.inline_start, LayoutUnit()},
      item_result->inline_size, /* children_count */ 0, item.BidiLevel());
}

InlineBoxState* LogicalLineBuilder::PlaceRubyColumn(
    const LineInfo& line_info,
    InlineItemResult& item_result,
    LogicalLineItems& line_box,
    InlineBoxState* box) {
  InlineItemResultRubyColumn& ruby_column = *item_result.ruby_column;
  bool on_start_edge = false;
  bool on_end_edge = false;
  std::optional<LayoutUnit> line_available_size;
  if (!node_.IsBidiEnabled() && !line_info.IsRubyBase() &&
      !line_info.IsRubyText() &&
      (line_info.TextAlign() == ETextAlign::kJustify ||
       (line_info.IsLastLine() &&
        line_info.LineStyle().GetTextAlign(/* is_last_line */ false) ==
            ETextAlign::kJustify))) {
    on_start_edge = ruby_column.base_line.InflowStartOffset() ==
                    line_info.InflowStartOffset();
    if (line_info.TextAlign() == ETextAlign::kJustify) {
      wtf_size_t end_text_offset = ruby_column.base_line.EndTextOffset();
      wtf_size_t inflow_end = line_info.InflowEndOffsetWithoutForcedBreak();
      on_end_edge = end_text_offset == inflow_end;

      // If this is the only item in the line and is a base-shorter ruby and
      // the line has text-align:justify, ApplyJustification() did nothing
      // because this item is represented as an OBJECT REPLACEMENT CHARACTER.
      // We expand the item by ruby-align processing.
      if (on_start_edge && on_end_edge &&
          item_result.inline_size > ruby_column.base_line.Width()) {
        line_available_size = line_info.AvailableWidth();
      }
    }
  }
  std::pair<LayoutUnit, LayoutUnit> base_insets =
      ApplyRubyAlign(line_available_size.value_or(item_result.inline_size),
                     on_start_edge, on_end_edge, ruby_column.base_line);

  // Set up LogicalRubyColumns. This should be done before consuming the base
  // InlineItemResults because it might contain ruby columns, and annotation
  // level detection depends on the LogicalRubyColumn creation order.
  wtf_size_t start_index = line_box.size();
  wtf_size_t ruby_column_start_index = box_states_->RubyColumnList().size();
  for (const RubyPosition position : ruby_column.position_list) {
    LogicalRubyColumn& logical_column = box_states_->CreateRubyColumn();
    logical_column.start_index = start_index;
    logical_column.ruby_position = position;
  }

  box = HandleItemResults(line_info, *ruby_column.base_line.MutableResults(),
                          &line_box,
                          /* main_line_helper */ nullptr, box);
  wtf_size_t column_base_size = line_box.size() - start_index;

  for (wtf_size_t i = 0; i < ruby_column.annotation_line_list.size(); ++i) {
    LogicalRubyColumn& logical_column =
        box_states_->RubyColumnAt(ruby_column_start_index + i);
    if (!ruby_column.annotation_line_list[i].IsEmptyLine()) {
      if (!line_box[start_index].has_over_annotation &&
          logical_column.ruby_position == RubyPosition::kOver) {
        for (wtf_size_t j = start_index; j < line_box.size(); ++j) {
          line_box[j].has_over_annotation = true;
        }
      }
      if (!line_box[start_index].has_under_annotation &&
          logical_column.ruby_position == RubyPosition::kUnder) {
        for (wtf_size_t j = start_index; j < line_box.size(); ++j) {
          line_box[j].has_under_annotation = true;
        }
      }
    }
    if (i == 0) {
      logical_column.base_insets = base_insets;
    }
    logical_column.size = column_base_size;
    PlaceRubyAnnotation(item_result, line_available_size, i,
                        ruby_column.annotation_line_list[i], logical_column);
  }

  return box;
}

void LogicalLineBuilder::PlaceRubyAnnotation(
    InlineItemResult& item_result,
    std::optional<LayoutUnit> line_available_size,
    wtf_size_t index,
    LineInfo& annotation_line,
    LogicalRubyColumn& logical_column) {
  std::pair<LayoutUnit, LayoutUnit> insets =
      ApplyRubyAlign(line_available_size.value_or(
                         item_result.inline_size -
                         item_result.ruby_column->last_base_glyph_spacing),
                     /* on_start_edge */ false,
                     /* on_end_edge */ false, annotation_line);

  auto* line_items = MakeGarbageCollected<LogicalLineItems>();
  LogicalLineBuilder annotation_builder(node_, constraint_space_, nullptr,
                                        &logical_column.state_stack, context_);
  if (item_result.ruby_column->is_continuation &&
      !annotation_line.Results().empty()) {
    CHECK(break_token_->RubyData());
    annotation_builder.RebuildBoxStates(
        annotation_line,
        break_token_->RubyData()->annotation_data[index].start_item_index,
        annotation_line.Results()[0].item_index);
  }
  annotation_builder.CreateLine(&annotation_line, line_items,
                                /* main_line_helper */ nullptr);
  ApplyLeftAndRightExpansion(insets.first, insets.second,
                             base::span(*line_items));

  logical_column.state_stack.ComputeInlinePositions(
      line_items, LayoutUnit(), /* ignore_box_margin_border_padding */ false);

  logical_column.annotation_items = line_items;
}

// Place a list marker.
void LogicalLineBuilder::PlaceListMarker(const InlineItem& item,
                                         InlineItemResult* item_result) {
  if (quirks_mode_) [[unlikely]] {
    box_states_->LineBoxState().EnsureTextMetrics(
        *item.Style(), item.Style()->GetFont(), baseline_type_);
  }
}

void LogicalLineBuilder::BidiReorder(
    TextDirection base_direction,
    LogicalLineItems* line_box,
    HeapVector<Member<LogicalRubyColumn>>& column_list) {
  if (line_box->IsEmpty()) {
    return;
  }

  // TODO(kojii): UAX#9 L1 is not supported yet. Supporting L1 may change
  // embedding levels of parts of runs, which requires to split items.
  // http://unicode.org/reports/tr9/#L1
  // BidiResolver does not support L1 crbug.com/316409.

  // A sentinel value for items that are opaque to bidi reordering. Should be
  // larger than the maximum resolved level.
  constexpr UBiDiLevel kOpaqueBidiLevel = 0xff;
  DCHECK_GT(kOpaqueBidiLevel, UBIDI_MAX_EXPLICIT_LEVEL + 1);

  // The base direction level is used for the items that should ignore its
  // original level and just use the paragraph level, as trailing opaque
  // items and items with only trailing whitespaces.
  UBiDiLevel base_direction_level = IsLtr(base_direction) ? 0 : 1;

  // Create a list of chunk indices in the visual order.
  // ICU |ubidi_getVisualMap()| works for a run of characters. Since we can
  // handle the direction of each run, we use |ubidi_reorderVisual()| to reorder
  // runs instead of characters.
  Vector<UBiDiLevel, 32> levels;
  levels.ReserveInitialCapacity(line_box->size());
  bool has_opaque_items = false;
  for (LogicalLineItem& item : *line_box) {
    if (item.IsOpaqueToBidiReordering()) {
      levels.push_back(kOpaqueBidiLevel);
      has_opaque_items = true;
      continue;
    }
    DCHECK_NE(item.bidi_level, kOpaqueBidiLevel);
    // UAX#9 L1: trailing whitespaces should use paragraph direction.
    if (item.has_only_bidi_trailing_spaces) {
      levels.push_back(base_direction_level);
      continue;
    }
    levels.push_back(item.bidi_level);
  }

  // For opaque items, copy bidi levels from adjacent items.
  if (has_opaque_items) {
    // Use the paragraph level for trailing opaque items.
    UBiDiLevel last_level = base_direction_level;
    for (UBiDiLevel& level : base::Reversed(levels)) {
      if (level == kOpaqueBidiLevel) {
        level = last_level;
      } else {
        last_level = level;
      }
    }
  }

  // Compute visual indices from resolved levels.
  Vector<int32_t, 32> indices_in_visual_order(levels.size());
  BidiParagraph::IndicesInVisualOrder(levels, &indices_in_visual_order);

  // Reorder to the visual order.
  LogicalLineItems& visual_items = context_->AcquireTempLogicalLineItems();
  visual_items.ReserveInitialCapacity(line_box->size());
  for (unsigned logical_index : indices_in_visual_order) {
    visual_items.AddChild(std::move((*line_box)[logical_index]));
  }
  DCHECK_EQ(line_box->size(), visual_items.size());
  line_box->swap(visual_items);
  context_->ReleaseTempLogicalLineItems(visual_items);

  // Adjust LogicalRubyColumn::start_index.
  if (column_list.size() > 0) {
    Vector<unsigned, 32> logical_to_visual(line_box->size());
    for (unsigned i = 0; i < indices_in_visual_order.size(); ++i) {
      logical_to_visual[indices_in_visual_order[i]] = i;
    }
    for (auto& column : column_list) {
      // Find the minimum visual index in the logical range
      // [column->start_index, column->EndIndex()).
      //
      // Base items in a ruby column are placed consecutively even after the
      // reorder because they are isolated.
      //
      // min_element() below doesn't return the end iterator because we
      // ensure there is at least one item in the range.
      column->start_index = *base::ranges::min_element(
          base::span(logical_to_visual)
              .subspan(column->start_index, column->size));
    }
    // The order is important for RubyBlockPositionCalculator::HandleRubyLine().
    std::stable_sort(
        column_list.begin(), column_list.end(),
        [](const auto& column1, const auto& column2) {
          int32_t diff = column2->start_index - column1->start_index;
          return diff != 0 ? (diff > 0) : (column1->size > column2->size);
        });
  }
}

void LogicalLineBuilder::RebuildBoxStates(const LineInfo& line_info,
                                          wtf_size_t start_item_index,
                                          wtf_size_t end_item_index) {
  // Compute which tags are not closed at the beginning of this line.
  InlineItemsData::OpenTagItems open_items;
  line_info.ItemsData().GetOpenTagItems(
      start_item_index, end_item_index - start_item_index, &open_items);

  // Create box states for tags that are not closed yet.
  LogicalLineItems& line_box = context_->AcquireTempLogicalLineItems();
  box_states_->OnBeginPlaceItems(node_, line_info.LineStyle(), baseline_type_,
                                 quirks_mode_, &line_box);
  for (const InlineItem* item : open_items) {
    InlineItemResult item_result;
    LineBreaker::ComputeOpenTagResult(*item, constraint_space_,
                                      node_.IsSvgText(), &item_result);
    HandleOpenTag(*item, item_result, &line_box);
  }
  context_->ReleaseTempLogicalLineItems(line_box);
}

}  // namespace blink
