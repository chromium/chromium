// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/inline_layout_algorithm.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/containers/adapters.h"
#include "base/metrics/histogram_macros.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-shared.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/core/layout/constraint_space.h"
#include "third_party/blink/renderer/core/layout/disable_layout_side_effects_scope.h"
#include "third_party/blink/renderer/core/layout/floats_utils.h"
#include "third_party/blink/renderer/core/layout/fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/inline/initial_letter_utils.h"
#include "third_party/blink/renderer/core/layout/inline/inline_box_state.h"
#include "third_party/blink/renderer/core/layout/inline/inline_break_token.h"
#include "third_party/blink/renderer/core/layout/inline/inline_child_layout_context.h"
#include "third_party/blink/renderer/core/layout/inline/inline_item_result.h"
#include "third_party/blink/renderer/core/layout/inline/inline_node.h"
#include "third_party/blink/renderer/core/layout/inline/line_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/inline/line_breaker.h"
#include "third_party/blink/renderer/core/layout/inline/line_info.h"
#include "third_party/blink/renderer/core/layout/inline/line_truncator.h"
#include "third_party/blink/renderer/core/layout/inline/line_widths.h"
#include "third_party/blink/renderer/core/layout/inline/paragraph_line_breaker.h"
#include "third_party/blink/renderer/core/layout/inline/ruby_utils.h"
#include "third_party/blink/renderer/core/layout/inline/score_line_breaker.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/core/layout/layout_text_combine.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/layout/list/layout_outside_list_marker.h"
#include "third_party/blink/renderer/core/layout/list/unpositioned_list_marker.h"
#include "third_party/blink/renderer/core/layout/logical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/positioned_float.h"
#include "third_party/blink/renderer/core/layout/relative_utils.h"
#include "third_party/blink/renderer/core/layout/space_utils.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/layout/unpositioned_float.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_spacing.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/text/bidi_paragraph.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

// This class provides hooks to switch between the default line breaker,
// `text-wrap: balance`, and `text-wrap: pretty`.
class LineBreakStrategy {
  STACK_ALLOCATED();

 public:
  LineBreakStrategy(InlineChildLayoutContext* context,
                    const InlineNode& node,
                    const ComputedStyle& block_style,
                    const InlineBreakToken* break_token,
                    const ColumnSpannerPath* column_spanner_path) {
    if (!column_spanner_path) {
      const TextWrap text_wrap = block_style.GetTextWrap();
      if (UNLIKELY(text_wrap == TextWrap::kBalance)) {
        score_line_break_context_ = context->GetScoreLineBreakContext();
        initiate_balancing_ = !break_token;
        if (initiate_balancing_) {
          DCHECK(!score_line_break_context_ ||
                 score_line_break_context_->IsActive());
          use_score_line_break_ = score_line_break_context_;
        }
      } else if (UNLIKELY(text_wrap == TextWrap::kPretty)) {
        score_line_break_context_ = context->GetScoreLineBreakContext();
        use_score_line_break_ =
            score_line_break_context_ && score_line_break_context_->IsActive();
      }
    }
#if EXPENSIVE_DCHECKS_ARE_ON()
    // `ScoreLineBreakContext()` must be null if `IsScoreLineBreakDisabled()`,
    // see `NeedsOptimalInlineChildLayoutContext()`, but the opposite may not be
    // true because some callsites such as MathML don't setup the context for
    // the score line breaker.
    DCHECK(!context->GetScoreLineBreakContext() ||
           !node.IsScoreLineBreakDisabled());
    DCHECK(!use_score_line_break_ || !node.IsScoreLineBreakDisabled());
#endif  // EXPENSIVE_DCHECKS_ARE_ON()
  }

  bool NeedsToPrepare() const {
    return initiate_balancing_ || use_score_line_break_;
  }

  void Prepare(InlineChildLayoutContext* context,
               const InlineNode& node,
               const ConstraintSpace& space,
               base::span<const LayoutOpportunity> opportunities,
               const LineLayoutOpportunity& line_opportunity,
               const LeadingFloats& leading_floats,
               const InlineBreakToken* break_token,
               ExclusionSpace* exclusion_space) {
    if (initiate_balancing_) {
      Balance(context, node, space, opportunities, line_opportunity,
              leading_floats, break_token, exclusion_space);
    } else if (use_score_line_break_) {
      Optimize(node, space, opportunities, leading_floats, break_token,
               exclusion_space);
    }
  }

  void SetupLineBreaker(InlineChildLayoutContext* context,
                        LineBreaker& line_breaker) {
    if (const absl::optional<LayoutUnit>& balanced_available_width =
            context->BalancedAvailableWidth();
        UNLIKELY(balanced_available_width)) {
      DCHECK(!score_line_break_context_ ||
             !score_line_break_context_->CurrentLineBreakPoint());
      line_breaker.OverrideAvailableWidth(*balanced_available_width);
    } else if (UNLIKELY(score_line_break_context_)) {
      if (const LineBreakPoint* break_point =
              score_line_break_context_->CurrentLineBreakPoint()) {
        line_breaker.SetBreakAt(*break_point);
      }
    }
  }

  void DidCreateLine(bool is_end_paragraph) {
    if (UNLIKELY(score_line_break_context_)) {
      score_line_break_context_->DidCreateLine(is_end_paragraph);
    }
  }

 private:
  void Balance(InlineChildLayoutContext* context,
               const InlineNode& node,
               const ConstraintSpace& space,
               const base::span<const LayoutOpportunity>& opportunities,
               const LineLayoutOpportunity& line_opportunity,
               const LeadingFloats& leading_floats,
               const InlineBreakToken* break_token,
               ExclusionSpace* exclusion_space) {
    // `initiate_balancing` should have checked these conditions.
    DCHECK(!context->BalancedAvailableWidth());
    DCHECK_GT(opportunities.size(), 0u);
    DCHECK(!opportunities.back().HasShapeExclusions());
    const base::ElapsedTimer timer;

    // Try `ScoreLineBreaker` first if it's applicable.
    if (use_score_line_break_ && score_line_break_context_->IsActive()) {
      DCHECK(RuntimeEnabledFeatures::CSSTextWrapBalanceByScoreEnabled());
      DCHECK(score_line_break_context_->GetLineBreakPoints().empty());
      DCHECK_EQ(score_line_break_context_->LineBreakPointsIndex(), 0u);
      LineWidths line_widths;
      if (line_widths.Set(node, opportunities)) {
        ScoreLineBreaker optimizer(node, space, line_widths, break_token,
                                   exclusion_space);
        optimizer.BalanceBreakPoints(leading_floats,
                                     *score_line_break_context_);
        if (!score_line_break_context_->GetLineBreakPoints().empty()) {
          UMA_HISTOGRAM_TIMES("Renderer.Layout.TextWrapBalance",
                              timer.Elapsed());
          return;
        }
      }
      // Fallback to the bisection if `ScoreLineBreaker` failed.
    }

    // Then try the bisection algorithm.
    // Exclusions and negative inline sizes are not supported.
    if (opportunities.size() == 1 &&
        line_opportunity.AvailableInlineSize() > LayoutUnit()) {
      if (const absl::optional<LayoutUnit> balanced_available_width =
              ParagraphLineBreaker::AttemptParagraphBalancing(
                  node, space, line_opportunity)) {
        context->SetBalancedAvailableWidth(balanced_available_width);
        if (score_line_break_context_) {
          score_line_break_context_->GetLineInfoList().Clear();
        }
        UMA_HISTOGRAM_TIMES("Renderer.Layout.TextWrapBalance", timer.Elapsed());
        return;
      }
    }

    UMA_HISTOGRAM_TIMES("Renderer.Layout.TextWrapBalance.Fail",
                        timer.Elapsed());
  }

  void Optimize(const InlineNode& node,
                const ConstraintSpace& space,
                const base::span<const LayoutOpportunity>& opportunities,
                const LeadingFloats& leading_floats,
                const InlineBreakToken* break_token,
                ExclusionSpace* exclusion_space) {
    DCHECK(score_line_break_context_->GetLineBreakPoints().empty());
    DCHECK_EQ(score_line_break_context_->LineBreakPointsIndex(), 0u);
    if (UNLIKELY(!score_line_break_context_->IsActive())) {
      return;
    }
    const base::ElapsedTimer timer;
    LineWidths line_widths;
    if (UNLIKELY(!line_widths.Set(node, opportunities, break_token))) {
      // The next line may have less opportunities that keep running, without
      // suspending the context.
      UMA_HISTOGRAM_TIMES("Renderer.Layout.TextWrapPretty.Fail",
                          timer.Elapsed());
      return;
    }
    ScoreLineBreaker optimizer(node, space, line_widths, break_token,
                               exclusion_space);
    optimizer.OptimalBreakPoints(leading_floats, *score_line_break_context_);
    if (score_line_break_context_->IsActive()) {
      // There are more lines until the end of a paragraph, keep looking.
      return;
    }
    if (!score_line_break_context_->GetLineBreakPoints().empty()) {
      UMA_HISTOGRAM_TIMES("Renderer.Layout.TextWrapPretty", timer.Elapsed());
    } else {
      UMA_HISTOGRAM_TIMES("Renderer.Layout.TextWrapPretty.Fail",
                          timer.Elapsed());
    }
  }

  bool initiate_balancing_ = false;
  bool use_score_line_break_ = false;
  ScoreLineBreakContext* score_line_break_context_ = nullptr;
};

}  // namespace

InlineLayoutAlgorithm::InlineLayoutAlgorithm(
    InlineNode inline_node,
    const ConstraintSpace& space,
    const InlineBreakToken* break_token,
    const ColumnSpannerPath* column_spanner_path,
    InlineChildLayoutContext* context)
    : LayoutAlgorithm(inline_node,
                      &inline_node.Style(),
                      space,
                      // Use LTR direction since inline layout handles bidi by
                      // itself and lays out in visual order.
                      TextDirection::kLtr,
                      break_token),
      box_states_(nullptr),
      context_(context),
      column_spanner_path_(column_spanner_path),
      baseline_type_(inline_node.Style().GetFontBaseline()),
      quirks_mode_(inline_node.GetDocument().InLineHeightQuirksMode()) {
  DCHECK(context);
}

// Define the destructor here, so that we can forward-declare more in the
// header.
InlineLayoutAlgorithm::~InlineLayoutAlgorithm() = default;

InlineBoxState* InlineLayoutAlgorithm::HandleOpenTag(
    const InlineItem& item,
    const InlineItemResult& item_result,
    LogicalLineItems* line_box,
    InlineLayoutStateStack* box_states) const {
  InlineBoxState* box = box_states->OnOpenTag(
      GetConstraintSpace(), item, item_result, baseline_type_, line_box);
  // Compute text metrics for all inline boxes since even empty inlines
  // influence the line height, except when quirks mode and the box is empty
  // for the purpose of empty block calculation.
  // https://drafts.csswg.org/css2/visudet.html#line-height
  if (!quirks_mode_ || !item.IsEmptyItem())
    box->ComputeTextMetrics(*item.Style(), *box->font, baseline_type_);

  if (item.Style()->HasMask()) {
    // Layout may change the bounding box, which affects MaskClip.
    if (LayoutObject* object = item.GetLayoutObject())
      object->SetNeedsPaintPropertyUpdate();
  }

  return box;
}

InlineBoxState* InlineLayoutAlgorithm::HandleCloseTag(
    const InlineItem& item,
    const InlineItemResult& item_result,
    LogicalLineItems* line_box,
    InlineBoxState* box) {
  if (UNLIKELY(quirks_mode_ && !item.IsEmptyItem()))
    box->EnsureTextMetrics(*item.Style(), *box->font, baseline_type_);
  box = box_states_->OnCloseTag(GetConstraintSpace(), line_box, box,
                                baseline_type_);
  // Just clear |NeedsLayout| flags. Culled inline boxes do not need paint
  // invalidations. If this object produces box fragments,
  // |InlineBoxStateStack| takes care of invalidations.
  if (!DisableLayoutSideEffectsScope::IsDisabled()) {
    item.GetLayoutObject()->ClearNeedsLayoutWithoutPaintInvalidation();
  }
  return box;
}

// Prepare InlineLayoutStateStack for a new line.
void InlineLayoutAlgorithm::PrepareBoxStates(
    const LineInfo& line_info,
    const InlineBreakToken* break_token) {
#if EXPENSIVE_DCHECKS_ARE_ON()
  is_box_states_from_context_ = false;
#endif

  // Use the initial box states if no break token; i.e., a line from the start.
  if (!break_token) {
    box_states_ = context_->ResetBoxStates();
    return;
  }

  // Check if the box states in InlineChildLayoutContext is valid for this line.
  // If the previous line was ::first-line, always rebuild because box states
  // have ::first-line styles.
  const HeapVector<InlineItem>& items = line_info.ItemsData().items;
  if (!break_token->UseFirstLineStyle()) {
    box_states_ = context_->BoxStatesIfValidForItemIndex(
        items, break_token->StartItemIndex());
    if (box_states_) {
#if EXPENSIVE_DCHECKS_ARE_ON()
      is_box_states_from_context_ = true;
#endif
      return;
    }
  }

  // If not, rebuild the box states for the break token.
  box_states_ = context_->ResetBoxStates();
  RebuildBoxStates(line_info, break_token, box_states_);
}

static LayoutUnit AdjustLineOffsetForHanging(LineInfo* line_info,
                                             LayoutUnit& line_offset) {
  if (IsLtr(line_info->BaseDirection()))
    return LayoutUnit();

  // If !line_info->ShouldHangTrailingSpaces(), the hang width is not considered
  // in ApplyTextAlign, and so line_offset points to where the left edge of the
  // hanging spaces should be. Since the line box rect has to start at the left
  // edge of the text instead (needed for caret positioning), we increase
  // line_offset.
  LayoutUnit hang_width = line_info->HangWidth();
  if (!line_info->ShouldHangTrailingSpaces()) {
    line_offset += hang_width;
  }

  // Now line_offset always points to where the left edge of the text should be.
  // If there are any hanging spaces, the starting position of the line must be
  // offset by the width of the hanging spaces so that the text starts at
  // line_offset.
  return -hang_width;
}

void InlineLayoutAlgorithm::RebuildBoxStates(
    const LineInfo& line_info,
    const InlineBreakToken* break_token,
    InlineLayoutStateStack* box_states) const {
  // Compute which tags are not closed at the beginning of this line.
  InlineItemsData::OpenTagItems open_items;
  line_info.ItemsData().GetOpenTagItems(break_token->StartItemIndex(),
                                        &open_items);

  // Create box states for tags that are not closed yet.
  LogicalLineItems& line_box = context_->AcquireTempLogicalLineItems();
  box_states->OnBeginPlaceItems(Node(), line_info.LineStyle(), baseline_type_,
                                quirks_mode_, &line_box);
  for (const InlineItem* item : open_items) {
    InlineItemResult item_result;
    LineBreaker::ComputeOpenTagResult(*item, GetConstraintSpace(),
                                      Node().IsSvgText(), &item_result);
    HandleOpenTag(*item, item_result, &line_box, box_states);
  }
  context_->ReleaseTempLogicalLineItems(line_box);
}

#if EXPENSIVE_DCHECKS_ARE_ON()
void InlineLayoutAlgorithm::CheckBoxStates(
    const LineInfo& line_info,
    const InlineBreakToken* break_token) const {
  InlineLayoutStateStack rebuilt;
  RebuildBoxStates(line_info, break_token, &rebuilt);
  LogicalLineItems& line_box = context_->AcquireTempLogicalLineItems();
  rebuilt.OnBeginPlaceItems(Node(), line_info.LineStyle(), baseline_type_,
                            quirks_mode_, &line_box);
  DCHECK(box_states_);
  box_states_->CheckSame(rebuilt);
  context_->ReleaseTempLogicalLineItems(line_box);
}
#endif

void InlineLayoutAlgorithm::CreateLine(const LineLayoutOpportunity& opportunity,
                                       LineInfo* line_info,
                                       LogicalLineItems* line_box) {
  // Needs MutableResults to move ShapeResult out of the LineInfo.
  InlineItemResults* line_items = line_info->MutableResults();
  // Clear the current line without releasing the buffer.
  line_box->Shrink(0);

  // Apply justification before placing items, because it affects size/position
  // of items, which are needed to compute inline static positions.
  LayoutUnit line_offset_for_text_align = ApplyTextAlign(line_info);

  // Compute heights of all inline items by placing the dominant baseline at 0.
  // The baseline is adjusted after the height of the line box is computed.
  const ComputedStyle& line_style = line_info->LineStyle();
  box_states_->SetIsEmptyLine(line_info->IsEmptyLine());
  InlineBoxState* box = box_states_->OnBeginPlaceItems(
      Node(), line_style, baseline_type_, quirks_mode_, line_box);
#if EXPENSIVE_DCHECKS_ARE_ON()
  if (is_box_states_from_context_)
    CheckBoxStates(*line_info, GetBreakToken());
#endif

  bool has_out_of_flow_positioned_items = false;
  bool has_floating_items = false;
  bool has_relative_positioned_items = false;
  const InlineItemResult* initial_letter_item_result = nullptr;

  // List items trigger strict line height, i.e. we make room for the line box
  // strut, for *every* line. This matches other browsers. The intention may
  // have been to make sure that there's always room for the list item marker,
  // but that doesn't explain why it's done for every line...
  if (quirks_mode_ && ComputedStyle::IsDisplayListItem(line_style.Display())) {
    box->ComputeTextMetrics(line_style, *box->font, baseline_type_);
  }

  for (InlineItemResult& item_result : *line_items) {
    DCHECK(item_result.item);
    const InlineItem& item = *item_result.item;
#if DCHECK_IS_ON()
    if (line_info->IsBlockInInline()) {
      DCHECK_EQ(line_items->size(), 1u);
      DCHECK_EQ(item.Type(), InlineItem::kBlockInInline);
    }
#endif
    if (item.Type() == InlineItem::kText) {
      DCHECK(item.GetLayoutObject());
      DCHECK(item.GetLayoutObject()->IsText() ||
             item.GetLayoutObject()->IsLayoutListItem());

      if (UNLIKELY(!item_result.Length())) {
        // Empty or fully collapsed text isn't needed for layout, but needs
        // `ClearNeedsLayout`. See `LineBreaker::HandleEmptyText`.
        LayoutObject* layout_object = item.GetLayoutObject();
        if (layout_object->NeedsLayout()) {
          layout_object->ClearNeedsLayout();
        }
        continue;
      }
      DCHECK(item_result.shape_result);

      if (UNLIKELY(quirks_mode_))
        box->EnsureTextMetrics(*item.Style(), *box->font, baseline_type_);

      // Take all used fonts into account if 'line-height: normal'.
      if (box->include_used_fonts)
        box->AccumulateUsedFonts(item_result.shape_result.get());

      DCHECK(item.TextType() == TextItemType::kNormal ||
             item.TextType() == TextItemType::kSymbolMarker);
      if (UNLIKELY(item_result.is_hyphenated)) {
        DCHECK(item_result.hyphen);
        LayoutUnit hyphen_inline_size = item_result.hyphen.InlineSize();
        line_box->AddChild(item, item_result, item_result.TextOffset(),
                           box->text_top,
                           item_result.inline_size - hyphen_inline_size,
                           box->text_height, item.BidiLevel());
        PlaceHyphen(item_result, hyphen_inline_size, line_box, box);
      } else if (UNLIKELY(Node().IsTextCombine())) {
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
      PlaceControlItem(item, *line_info, &item_result, line_box, box);
    } else if (item.Type() == InlineItem::kOpenTag) {
      box = HandleOpenTag(item, item_result, line_box, box_states_);
    } else if (item.Type() == InlineItem::kCloseTag) {
      box = HandleCloseTag(item, item_result, line_box, box);
    } else if (item.Type() == InlineItem::kAtomicInline) {
      box = PlaceAtomicInline(item, *line_info, &item_result, line_box);
      has_relative_positioned_items |=
          item.Style()->GetPosition() == EPosition::kRelative;
    } else if (item.Type() == InlineItem::kBlockInInline) {
      DCHECK(line_info->IsBlockInInline());
      PlaceBlockInInline(item, *line_info, &item_result, line_box);
    } else if (item.Type() == InlineItem::kListMarker) {
      PlaceListMarker(item, &item_result, *line_info);
    } else if (item.Type() == InlineItem::kOutOfFlowPositioned) {
      // An inline-level OOF child positions itself based on its direction, a
      // block-level OOF child positions itself based on the direction of its
      // block-level container.
      TextDirection direction =
          item.GetLayoutObject()->StyleRef().IsOriginalDisplayInlineType()
              ? item.Direction()
              : GetConstraintSpace().Direction();

      line_box->AddChild(item.GetLayoutObject(), item.BidiLevel(), direction);
      has_out_of_flow_positioned_items = true;
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
      has_floating_items = true;
      has_relative_positioned_items |=
          item.Style()->GetPosition() == EPosition::kRelative;
    } else if (item.Type() == InlineItem::kBidiControl) {
      line_box->AddChild(item.BidiLevel());
    } else if (UNLIKELY(item.Type() == InlineItem::kInitialLetterBox)) {
      // The initial letter does not increase the logical height of the line
      // box in which it participates[1]. So, we should not changes
      // `InlineBoxState::metrics`, or not call ` ComputeTextMetrics()` to
      // incorporate from `ComputedStyle::GetFont()` of the initial letter box.
      // See also `LineInfo::ComputeTotalBlockSize()` for calculation of
      // layout opportunities.
      // [1] https://drafts.csswg.org/css-inline/#initial-letter-block-position
      DCHECK(!initial_letter_item_result);
      initial_letter_item_result = &item_result;
      PlaceInitialLetterBox(item, *line_info, &item_result, line_box);
    }
  }

  box_states_->OnEndPlaceItems(GetConstraintSpace(), line_box, baseline_type_);

  if (UNLIKELY(Node().IsBidiEnabled())) {
    box_states_->PrepareForReorder(line_box);
    BidiReorder(line_info->BaseDirection(), line_box);
    box_states_->UpdateAfterReorder(line_box);
  } else {
    DCHECK(IsLtr(line_info->BaseDirection()));
  }
  const LayoutUnit hang_width = line_info->HangWidth();
  const LayoutUnit position =
      AdjustLineOffsetForHanging(line_info, line_offset_for_text_align);
  LayoutUnit inline_size = box_states_->ComputeInlinePositions(
      line_box, position, line_info->IsBlockInInline());
  if (UNLIKELY(hang_width)) {
    // If we've shifted the line items the inline-size is already correct.
    if (position == LayoutUnit())
      inline_size -= hang_width;
    container_builder_.SetHangInlineSize(hang_width);
  }

  // Truncate the line if:
  //  - 'text-overflow: ellipsis' is set and we *aren't* a line-clamp context.
  //  - If we've reached the line-clamp limit.
  if (UNLIKELY(((line_info->HasOverflow() &&
                 !GetConstraintSpace().IsLineClampContext() &&
                 node_.GetLayoutBlockFlow()->ShouldTruncateOverflowingText()) ||
                GetConstraintSpace().LinesUntilClamp() == 1) &&
               !line_info->IsBlockInInline())) {
    DCHECK(!line_info->IsBlockInInline());
    LineTruncator truncator(*line_info);
    auto* input =
        DynamicTo<HTMLInputElement>(node_.GetLayoutBlockFlow()->GetNode());
    if (input && input->ShouldApplyMiddleEllipsis()) {
      inline_size =
          truncator.TruncateLineInTheMiddle(inline_size, line_box, box_states_);
    } else {
      inline_size = truncator.TruncateLine(inline_size, line_box, box_states_);
    }
  }

  // Negative margins can make the position negative, but the inline size is
  // always positive or 0.
  inline_size = inline_size.ClampNegativeToZero();

  if (line_info->IsBlockInInline()) {
    container_builder_.SetBfcLineOffset(
        GetConstraintSpace().GetBfcOffset().line_offset);
  } else {
    // Other 'text-align' values than 'justify' move line boxes as a whole, but
    // indivisual items do not change their relative position to the line box.
    LayoutUnit bfc_line_offset =
        line_info->GetBfcOffset().line_offset + line_offset_for_text_align;

    if (IsLtr(line_info->BaseDirection()))
      bfc_line_offset += line_info->TextIndent();

    container_builder_.SetBfcLineOffset(bfc_line_offset);
  }

  // Force an editable empty line to have metrics, so that is has a height.
  if (UNLIKELY(line_info->HasLineEvenIfEmpty())) {
    box_states_->LineBoxState().EnsureTextMetrics(
        line_info->LineStyle(), *box_states_->LineBoxState().font,
        baseline_type_);
  } else if (UNLIKELY(initial_letter_item_result) &&
             box_states_->LineBoxState().metrics.IsEmpty()) {
    box_states_->LineBoxState().metrics = FontHeight();
  }

  const FontHeight& line_box_metrics = box_states_->LineBoxState().metrics;

  if (UNLIKELY(Node().HasRuby() && !line_info->IsEmptyLine())) {
    line_info->SetAnnotationBlockStartAdjustment(
        SetAnnotationOverflow(*line_info, *line_box, line_box_metrics));
  }

  if (UNLIKELY(initial_letter_item_result)) {
    DCHECK(!line_info->IsEmptyLine());
    // `container_builder_.BfcLineOffset()` holds left edge of current line
    // after applying `text-align` and `text-indent`.
    //
    //       *                                  *
    //       |                                  |
    //      +V------------------+          +----V---------------+
    //  LTR | this is line 1.   |     RTL  |     this is line 1.|
    //
    // Margins should be `BoxStrut` instead of `LineBoxStrut` for calculating
    // block offset. Test[1], for flipped line writing mode, verifies
    // differences between them.
    // [1]
    // https://wpt.live/css/css-inline/initial-letter/initial-letter-block-position-margins-vlr.html
    const ExclusionArea* exclusion = PostPlaceInitialLetterBox(
        line_box_metrics,
        BoxStrut(initial_letter_item_result->margins,
                 line_info->LineStyle().IsFlippedLinesWritingMode()),
        line_box,
        BfcOffset(container_builder_.BfcLineOffset(),
                  line_info->GetBfcOffset().block_offset),
        line_info);
    GetExclusionSpace().Add(exclusion);
  }

  // Place out-of-flow positioned objects.
  // This adjusts the LogicalLineItem::offset member to contain
  // the static position of the OOF positioned children relative to the linebox.
  if (has_out_of_flow_positioned_items) {
    DCHECK(!line_info->IsBlockInInline());
    PlaceOutOfFlowObjects(*line_info, line_box_metrics, line_box);
  }

  // Place floating objects.
  // This adjusts the  LogicalLineItem::offset member to
  // contain the position of the float relative to the linebox.
  // Additionally it will perform layout on any unpositioned floats which
  // needed the line height to correctly determine their final position.
  if (has_floating_items) {
    DCHECK(!line_info->IsBlockInInline());
    // Test[1] has a float to be pushed down to next line.
    // [1]
    // https://wpt.live/css/css-inline/initial-letter/initial-letter-floats-005.html
    PlaceFloatingObjects(line_box_metrics, opportunity,
                         line_info->ComputeBlockStartAdjustment(), line_info,
                         line_box);
  }

  // Apply any relative positioned offsets to *items* which have relative
  // positioning, (atomic-inlines, and floats). This will only move the
  // individual item.
  if (has_relative_positioned_items)
    PlaceRelativePositionedItems(line_box);

  // Apply any relative positioned offsets to any boxes (and their children).
  box_states_->ApplyRelativePositioning(GetConstraintSpace(), line_box);

  // Create box fragments if needed. After this point forward, |line_box| is a
  // tree structure.
  // The individual children don't move position within the |line_box|, rather
  // the children have their layout_result, fragment, (or similar) set to null,
  // creating a "hole" in the array.
  if (box_states_->HasBoxFragments()) {
    box_states_->CreateBoxFragments(GetConstraintSpace(), line_box,
                                    line_info->IsBlockInInline());
  }

  // Update item index of the box states in the context.
  context_->SetItemIndex(line_info->ItemsData().items,
                         line_info->EndItemIndex());

  if (line_info->UseFirstLineStyle())
    container_builder_.SetStyleVariant(StyleVariant::kFirstLine);

  // Even if we have something in-flow, it may just be empty items that
  // shouldn't trigger creation of a line. Exit now if that's the case.
  if (line_info->IsEmptyLine())
    return;

  if (!line_box_metrics.IsEmpty())
    container_builder_.SetMetrics(line_box_metrics);

  // |container_builder_| is already set up by |PlaceBlockInInline|.
  if (line_info->IsBlockInInline())
    return;

  // Up until this point, children are placed so that the dominant baseline is
  // at 0. Move them to the final baseline position, and set the logical top of
  // the line box to the line top.
  //
  // For SVG <text>, the block offset of the initial 'current text position'
  // should be 0. As for the inline offset, see
  // SvgTextLayoutAttributesBuilder::Build().
  //
  // For text-combine-upright:all, the block offset should be zero to make
  // combined text in 1em x 1em box.
  if (UNLIKELY(Node().IsTextCombine())) {
    // The effective size of combined text is 1em square[1]
    // [1] https://drafts.csswg.org/css-writing-modes-3/#text-combine-layout
    const auto one_em = Node().Style().ComputedFontSizeAsFixed();
    inline_size = std::min(inline_size, one_em);
  } else if (UNLIKELY(Node().IsInitialLetterBox())) {
    const FontHeight& adjusted_metrics =
        AdjustInitialLetterInTextPosition(line_box_metrics, line_box);
    if (!adjusted_metrics.IsEmpty()) {
      container_builder_.SetMetrics(adjusted_metrics);
      line_box->MoveInBlockDirection(adjusted_metrics.ascent);
    }
  } else if (LIKELY(!Node().IsSvgText())) {
    // Convert baseline relative block offset of `LogicalLineItem::rect` to
    // to line box relative block offset.
    line_box->MoveInBlockDirection(line_box_metrics.ascent);
  }

  container_builder_.SetInlineSize(inline_size);
}

void InlineLayoutAlgorithm::PlaceControlItem(const InlineItem& item,
                                             const LineInfo& line_info,
                                             InlineItemResult* item_result,
                                             LogicalLineItems* line_box,
                                             InlineBoxState* box) {
  DCHECK_EQ(item.Type(), InlineItem::kControl);
  DCHECK_GE(item.Length(), 1u);
  DCHECK(!item.TextShapeResult());
  DCHECK_NE(item.TextType(), TextItemType::kNormal);
#if DCHECK_IS_ON()
  item.CheckTextType(line_info.ItemsData().text_content);
#endif

  // Don't generate fragments if this is a generated (not in DOM) break
  // opportunity during the white space collapsing in InlineItemBuilder.
  if (UNLIKELY(item.IsGeneratedForLineBreak()))
    return;

  DCHECK(item.GetLayoutObject());
  DCHECK(item.GetLayoutObject()->IsText());
  if (!DisableLayoutSideEffectsScope::IsDisabled()) {
    item.GetLayoutObject()->ClearNeedsLayoutWithFullPaintInvalidation();
  }

  if (UNLIKELY(!item_result->Length())) {
    // Empty or fully collapsed text isn't needed for layout, but needs
    // `ClearNeedsLayout`. See `LineBreaker::HandleEmptyText`.
    return;
  }

  if (UNLIKELY(quirks_mode_ && !box->HasMetrics()))
    box->EnsureTextMetrics(*item.Style(), *box->font, baseline_type_);

  line_box->AddChild(item, std::move(item_result->shape_result),
                     item_result->TextOffset(), box->text_top,
                     item_result->inline_size, box->text_height,
                     item.BidiLevel());
}

void InlineLayoutAlgorithm::PlaceHyphen(const InlineItemResult& item_result,
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

InlineBoxState* InlineLayoutAlgorithm::PlaceAtomicInline(
    const InlineItem& item,
    const LineInfo& line_info,
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
      GetConstraintSpace(), item, *item_result, baseline_type_, *line_box);

  if (LIKELY(!IsA<LayoutTextCombine>(layout_object))) {
    PlaceLayoutResult(item_result, line_box, box, box->margin_inline_start);
  } else {
    // The metrics should be as text instead of atomic inline box.
    const auto& style = layout_object->Parent()->StyleRef();
    box->ComputeTextMetrics(style, style.GetFont(), baseline_type_);
    // Note: |item_result->spacing_before| is non-zero if this |item_result|
    // is |LayoutTextCombine| and after CJK character.
    // See "text-combine-justify.html".
    const LayoutUnit inline_offset =
        box->margin_inline_start + item_result->spacing_before;
    line_box->AddChild(std::move(item_result->layout_result),
                       LogicalOffset{inline_offset, box->text_top},
                       item_result->inline_size, /* children_count */ 0,
                       item.BidiLevel());
  }
  return box_states_->OnCloseTag(GetConstraintSpace(), line_box, box,
                                 baseline_type_);
}

// Place a LayoutResult into the line box.
void InlineLayoutAlgorithm::PlaceLayoutResult(InlineItemResult* item_result,
                                              LogicalLineItems* line_box,
                                              InlineBoxState* box,
                                              LayoutUnit inline_offset) {
  DCHECK(item_result->layout_result);
  DCHECK(item_result->item);
  const InlineItem& item = *item_result->item;
  DCHECK(item.Style());
  FontHeight metrics =
      LogicalBoxFragment(GetConstraintSpace().GetWritingDirection(),
                         To<PhysicalBoxFragment>(
                             item_result->layout_result->GetPhysicalFragment()))
          .BaselineMetrics(item_result->margins, baseline_type_);
  if (box)
    box->metrics.Unite(metrics);

  LayoutUnit line_top = item_result->margins.line_over - metrics.ascent;
  line_box->AddChild(std::move(item_result->layout_result),
                     LogicalOffset{inline_offset, line_top},
                     item_result->inline_size, /* children_count */ 0,
                     item.BidiLevel());
}

void InlineLayoutAlgorithm::PlaceBlockInInline(const InlineItem& item,
                                               const LineInfo& line_info,
                                               InlineItemResult* item_result,
                                               LogicalLineItems* line_box) {
  DCHECK_EQ(item.Type(), InlineItem::kBlockInInline);
  LayoutObject* layout_object = item.GetLayoutObject();
  DCHECK(layout_object);
  DCHECK(layout_object->IsAnonymous());
  DCHECK(!layout_object->IsInline());
  DCHECK(item_result->layout_result);
  const LayoutResult& result = *item_result->layout_result;
  const auto& box_fragment =
      To<PhysicalBoxFragment>(result.GetPhysicalFragment());
  LogicalBoxFragment fragment(GetConstraintSpace().GetWritingDirection(),
                              box_fragment);

  // Setup |container_builder_|. Set it up here instead of in |CreateLine|,
  // because there should be only one block-in-inline, and we need data from the
  // |LayoutResult|.
  container_builder_.SetIsBlockInInline();
  container_builder_.SetInlineSize(fragment.InlineSize());

  container_builder_.ClampBreakAppeal(result.GetBreakAppeal());

  if (!result.IsSelfCollapsing()) {
    // Block-in-inline is wrapped in an anonymous block that has no margins.
    const FontHeight metrics = fragment.BaselineMetrics(
        /* margins */ LineBoxStrut(), baseline_type_);
    box_states_->OnBlockInInline(metrics, line_box);
  }

  end_margin_strut_ = result.EndMarginStrut();
  container_builder_.SetExclusionSpace(result.GetExclusionSpace());
  container_builder_.SetAdjoiningObjectTypes(result.GetAdjoiningObjectTypes());
  lines_until_clamp_ = result.LinesUntilClamp();
  if (UNLIKELY(box_fragment.MayHaveDescendantAboveBlockStart()))
    container_builder_.SetMayHaveDescendantAboveBlockStart(true);

  line_box->AddChild(std::move(item_result->layout_result),
                     /* offset */ LogicalOffset(), item_result->inline_size,
                     /* children_count */ 0, item.BidiLevel());
}

void InlineLayoutAlgorithm::PlaceInitialLetterBox(const InlineItem& item,
                                                  const LineInfo& line_info,
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

// Place all out-of-flow objects in |line_box_|.
void InlineLayoutAlgorithm::PlaceOutOfFlowObjects(
    const LineInfo& line_info,
    const FontHeight& line_box_metrics,
    LogicalLineItems* line_box) {
  DCHECK(line_info.IsEmptyLine() || !line_box_metrics.IsEmpty())
      << "Non-empty lines must have a valid set of linebox metrics.";

  // All children within the linebox are positioned relative to the baseline,
  // then shifted later using LineBoxFragmentBuilder::MoveInBlockDirection.
  LayoutUnit baseline_adjustment =
      line_info.IsEmptyLine() ? LayoutUnit() : -line_box_metrics.ascent;

  LayoutUnit line_height =
      line_info.IsEmptyLine() ? LayoutUnit() : line_box_metrics.LineHeight();

  // The location of the "next" line.
  //
  // This uses ConstraintSpace::Direction rather than
  // LineInfo::BaseDirection as this is for a block-level object rather than
  // an inline-level object.
  //
  // Similarly this uses the available size to determine which edge to align
  // to, and *does not* avoid floats.
  LayoutUnit block_level_line_location =
      IsLtr(GetConstraintSpace().Direction())
          ? LayoutUnit()
          : GetConstraintSpace().AvailableSize().inline_size;

  // This offset represents the position of the "next" line, relative to the
  // line we are currently creating, (this takes into account text-indent, etc).
  LayoutUnit block_level_inline_offset =
      block_level_line_location -
      (container_builder_.BfcLineOffset() -
       GetConstraintSpace().GetBfcOffset().line_offset);

  // To correctly determine which "line" block-level out-of-flow positioned
  // object is placed on, we need to keep track of if there is any inline-level
  // content preceeding it.
  bool has_preceding_inline_level_content = false;
  bool has_rtl_block_level_out_of_flow_objects = false;
  bool is_ltr = IsLtr(line_info.BaseDirection());

  for (LogicalLineItem& child : *line_box) {
    has_preceding_inline_level_content |= child.HasInFlowFragment();

    const LayoutObject* box = child.out_of_flow_positioned_box;
    if (!box)
      continue;

    LogicalOffset static_offset(LayoutUnit(), baseline_adjustment);
    if (box->StyleRef().IsOriginalDisplayInlineType()) {
      // An inline-level OOF element positions itself within the line, at the
      // position it would have been if it was in-flow.
      static_offset.inline_offset = child.rect.offset.inline_offset;

      // The static-position of inline-level OOF-positioned nodes depends on
      // previous floats (if any).
      //
      // If we are an empty-inline we may not have the correct BFC block-offset
      // yet. Due to this we need to mark this node as having adjoining
      // objects, and perform a re-layout if our position shifts.
      container_builder_.AddAdjoiningObjectTypes(kAdjoiningInlineOutOfFlow);
    } else {
      // A block-level OOF element positions itself on the "next" line. However
      // only shifts down if there is preceding inline-level content.
      static_offset.inline_offset = block_level_inline_offset;
      if (is_ltr) {
        if (has_preceding_inline_level_content)
          static_offset.block_offset += line_height;
      } else {
        // "Preceding" is in logical order, but this loop is in visual order. In
        // RTL, move objects down in the reverse-order loop below.
        has_rtl_block_level_out_of_flow_objects = true;
      }
    }

    child.rect.offset = static_offset;
  }

  if (UNLIKELY(has_rtl_block_level_out_of_flow_objects)) {
    has_preceding_inline_level_content = false;
    for (LogicalLineItem& child : base::Reversed(*line_box)) {
      const LayoutObject* box = child.out_of_flow_positioned_box;
      if (!box) {
        has_preceding_inline_level_content |= child.HasInFlowFragment();
        continue;
      }
      if (has_preceding_inline_level_content &&
          !box->StyleRef().IsOriginalDisplayInlineType()) {
        child.rect.offset.block_offset += line_height;
      }
    }
  }
}

void InlineLayoutAlgorithm::PlaceFloatingObjects(
    const FontHeight& line_box_metrics,
    const LineLayoutOpportunity& opportunity,
    LayoutUnit ruby_block_start_adjust,
    LineInfo* line_info,
    LogicalLineItems* line_box) {
  DCHECK(line_info->IsEmptyLine() || !line_box_metrics.IsEmpty())
      << "Non-empty lines must have a valid set of linebox metrics.";

  // All children within the linebox are positioned relative to the baseline,
  // then shifted later using LineBoxFragmentBuilder::MoveInBlockDirection.
  LayoutUnit baseline_adjustment =
      line_info->IsEmptyLine() ? LayoutUnit() : -line_box_metrics.ascent;

  LayoutUnit line_height =
      line_info->IsEmptyLine() ? LayoutUnit() : line_box_metrics.LineHeight();

  // Any unpositioned floats we encounter need to be placed on the "next" line.
  // This BFC block-offset represents the start of the "next" line.
  LayoutUnit origin_bfc_block_offset =
      opportunity.bfc_block_offset + line_height;

  LayoutUnit bfc_line_offset = container_builder_.BfcLineOffset();
  LayoutUnit bfc_block_offset =
      line_info->IsEmptyLine()
          ? GetConstraintSpace().ExpectedBfcBlockOffset()
          : line_info->GetBfcOffset().block_offset + ruby_block_start_adjust;

  for (LogicalLineItem& child : *line_box) {
    // We need to position any floats which should be on the "next" line now.
    // If this is an empty inline, all floats are positioned during the
    // PositionLeadingFloats step.
    if (child.unpositioned_float) {
      // If we're resuming in a parallel fragmentation flow, the line breaker
      // should not leave any unpositioned floats behind.
      DCHECK(!GetBreakToken() || !GetBreakToken()->IsInParallelBlockFlow());

      PositionedFloat positioned_float =
          PositionFloat(origin_bfc_block_offset, child.unpositioned_float,
                        &GetExclusionSpace());
      const BlockBreakToken* break_token = positioned_float.BreakToken();
      if (break_token) {
        const auto* parallel_token =
            InlineBreakToken::CreateForParallelBlockFlow(
                node_, child.item_index, *break_token);
        line_info->PropagateParallelFlowBreakToken(parallel_token);
        if (positioned_float.minimum_space_shortage) {
          line_info->PropagateMinimumSpaceShortage(
              positioned_float.minimum_space_shortage);
        }
      }
      if (!break_token || !break_token->IsBreakBefore()) {
        child.layout_result = std::move(positioned_float.layout_result);
        child.bfc_offset = positioned_float.bfc_offset;
        child.unpositioned_float = nullptr;
      }
    }

    // Skip any children which aren't positioned floats.
    if (!child.layout_result ||
        !child.layout_result->GetPhysicalFragment().IsFloating()) {
      continue;
    }

    LayoutUnit block_offset =
        child.bfc_offset.block_offset - bfc_block_offset + baseline_adjustment;

    // We need to manually account for the flipped-lines writing mode here :(.
    if (IsFlippedLinesWritingMode(GetConstraintSpace().GetWritingMode())) {
      LogicalFragment fragment(GetConstraintSpace().GetWritingDirection(),
                               child.layout_result->GetPhysicalFragment());

      block_offset = -fragment.BlockSize() - block_offset;
    }

    child.rect.offset = {child.bfc_offset.line_offset - bfc_line_offset,
                         block_offset};
  }
}

void InlineLayoutAlgorithm::PlaceRelativePositionedItems(
    LogicalLineItems* line_box) {
  for (auto& child : *line_box) {
    const auto* physical_fragment = child.GetPhysicalFragment();
    if (!physical_fragment)
      continue;
    child.rect.offset += ComputeRelativeOffsetForInline(
        GetConstraintSpace(), physical_fragment->Style());
  }
}

// Place a list marker.
void InlineLayoutAlgorithm::PlaceListMarker(const InlineItem& item,
                                            InlineItemResult* item_result,
                                            const LineInfo& line_info) {
  if (UNLIKELY(quirks_mode_)) {
    box_states_->LineBoxState().EnsureTextMetrics(
        *item.Style(), item.Style()->GetFont(), baseline_type_);
  }
}

// Justify the line. This changes the size of items by adding spacing.
// Returns false if justification failed and should fall back to start-aligned.
absl::optional<LayoutUnit> InlineLayoutAlgorithm::ApplyJustify(
    LayoutUnit space,
    LineInfo* line_info) {
  // Empty lines should align to start.
  if (line_info->IsEmptyLine())
    return absl::nullopt;

  // Justify the end of visible text, ignoring preserved trailing spaces.
  unsigned end_offset = line_info->EndOffsetForJustify();

  // If this line overflows, fallback to 'text-align: start'.
  if (space <= 0)
    return absl::nullopt;

  // Can't justify an empty string.
  if (end_offset == line_info->StartOffset())
    return absl::nullopt;

  const UChar kTextCombineItemMarker = 0x3042;  // U+3042 Hiragana Letter A

  // Note: |line_info->StartOffset()| can be different from
  // |ItemsResults[0].StartOffset()|, e.g. <b><input> <input></b> when
  // line break before space (leading space). See http://crbug.com/1240791
  const unsigned line_text_start_offset =
      line_info->Results().front().StartOffset();

  // Construct the line text to compute spacing for.
  StringBuilder line_text_builder;
  if (UNLIKELY(line_info->MayHaveTextCombineItem())) {
    for (const InlineItemResult& item_result : line_info->Results()) {
      if (item_result.StartOffset() >= end_offset)
        break;
      if (item_result.item->IsTextCombine()) {
        // To apply justification before and after the combined text, we put
        // ideographic character to increment |ShapeResultSpacing::
        // expansion_opportunity_count_| for legacy layout compatibility.
        // See "fast/writing-mode/text-combine-justify.html".
        // Note: The spec[1] says we should treat combined text as U+FFFC.
        // [1] https://drafts.csswg.org/css-writing-modes-3/#text-combine-layout
        line_text_builder.Append(kTextCombineItemMarker);
        continue;
      }
      line_text_builder.Append(StringView(line_info->ItemsData().text_content,
                                          item_result.StartOffset(),
                                          item_result.Length()));
    }
  } else {
    line_text_builder.Append(StringView(line_info->ItemsData().text_content,
                                        line_text_start_offset,
                                        end_offset - line_text_start_offset));
  }

  // Append a hyphen if the last word is hyphenated. The hyphen is in
  // |ShapeResult|, but not in text. |ShapeResultSpacing| needs the text that
  // matches to the |ShapeResult|.
  DCHECK(!line_info->Results().empty());
  const InlineItemResult& last_item_result = line_info->Results().back();
  if (last_item_result.hyphen) {
    line_text_builder.Append(last_item_result.hyphen.Text());
  }

  // Compute the spacing to justify.
  // Releasing string, StringBuilder reset.
  String line_text = line_text_builder.ReleaseString();
  DCHECK_GT(line_text.length(), 0u);

  ShapeResultSpacing<String> spacing(line_text, Node().IsSvgText());
  spacing.SetExpansion(space, line_info->BaseDirection());
  const LayoutObject* box = Node().GetLayoutBox();
  if (!spacing.HasExpansion()) {
    // See AdjustInlineDirectionLineBounds() of LayoutRubyBase and
    // LayoutRubyText.
    if (box && (box->IsRubyText() || box->IsRubyBase()))
      return space / 2;
    return absl::nullopt;
  }

  LayoutUnit inset;
  // See AdjustInlineDirectionLineBounds() of LayoutRubyBase and
  // LayoutRubyText.
  if (box && (box->IsRubyText() || box->IsRubyBase())) {
    unsigned count = std::min(spacing.ExpansionOppotunityCount(),
                              static_cast<unsigned>(LayoutUnit::Max().Floor()));
    // Inset the ruby base/text by half the inter-ideograph expansion amount.
    inset = space / (count + 1);
    // For ruby text,  inset it by no more than a full-width ruby character on
    // each side.
    if (box->IsRubyText()) {
      inset =
          std::min(LayoutUnit(2 * line_info->LineStyle().FontSize()), inset);
    }
    spacing.SetExpansion(space - inset, line_info->BaseDirection());
  }

  for (InlineItemResult& item_result : *line_info->MutableResults()) {
    if (item_result.has_only_trailing_spaces)
      break;
    if (item_result.shape_result) {
      scoped_refptr<ShapeResult> shape_result =
          item_result.shape_result->CreateShapeResult();
      DCHECK_GE(item_result.StartOffset(), line_text_start_offset);
      DCHECK_EQ(shape_result->NumCharacters(), item_result.Length());
      shape_result->ApplySpacing(spacing, item_result.StartOffset() -
                                              line_text_start_offset -
                                              shape_result->StartIndex());
      item_result.inline_size = shape_result->SnappedWidth();
      if (UNLIKELY(item_result.is_hyphenated))
        item_result.inline_size += item_result.hyphen.InlineSize();
      item_result.shape_result = ShapeResultView::Create(shape_result.get());
    } else if (item_result.item->Type() == InlineItem::kAtomicInline) {
      float spacing_before = 0.0f;
      DCHECK_LE(line_text_start_offset, item_result.StartOffset());
      const unsigned line_text_offset =
          item_result.StartOffset() - line_text_start_offset;
      const float spacing_after =
          spacing.ComputeSpacing(line_text_offset, spacing_before);
      if (UNLIKELY(item_result.item->IsTextCombine())) {
        // |spacing_before| is non-zero if this |item_result| is after
        // non-CJK character. See "text-combine-justify.html".
        DCHECK_EQ(kTextCombineItemMarker, line_text[line_text_offset]);
        item_result.inline_size += spacing_after;
        item_result.spacing_before = LayoutUnit(spacing_before);
      } else {
        DCHECK_EQ(kObjectReplacementCharacter, line_text[line_text_offset]);
        item_result.inline_size += spacing_after;
        // |spacing_before| is non-zero only before CJK characters.
        DCHECK_EQ(spacing_before, 0.0f);
      }
    }
  }
  return inset / 2;
}

// Apply the 'text-align' property to |line_info|. Returns the amount to move
// the line in the inline direction.
LayoutUnit InlineLayoutAlgorithm::ApplyTextAlign(LineInfo* line_info) {
  // LineInfo::WidthForAlignment may return a negative value, as text-indent
  // can accept negative values. We need to use this un-clamped value for
  // alignment, instead of just LineInfo::Width.
  LayoutUnit space =
      line_info->AvailableWidth() - line_info->WidthForAlignment();

  ETextAlign text_align = line_info->TextAlign();
  if (text_align == ETextAlign::kJustify) {
    absl::optional<LayoutUnit> offset = ApplyJustify(space, line_info);
    if (offset)
      return *offset;

    // If justification fails, fallback to 'text-align: start'.
    text_align = ETextAlign::kStart;
  }

  return LineOffsetForTextAlign(text_align, line_info->BaseDirection(), space);
}

LayoutUnit InlineLayoutAlgorithm::SetAnnotationOverflow(
    const LineInfo& line_info,
    const LogicalLineItems& line_box,
    const FontHeight& line_box_metrics) {
  AnnotationMetrics annotation_metrics = ComputeAnnotationOverflow(
      line_box, line_box_metrics, line_info.LineStyle());
  LayoutUnit annotation_overflow_block_start;
  LayoutUnit annotation_overflow_block_end;
  LayoutUnit annotation_space_block_start;
  LayoutUnit annotation_space_block_end;
  if (!IsFlippedLinesWritingMode(line_info.LineStyle().GetWritingMode())) {
    annotation_overflow_block_start = annotation_metrics.overflow_over;
    annotation_overflow_block_end = annotation_metrics.overflow_under;
    annotation_space_block_start = annotation_metrics.space_over;
    annotation_space_block_end = annotation_metrics.space_under;
  } else {
    annotation_overflow_block_start = annotation_metrics.overflow_under;
    annotation_overflow_block_end = annotation_metrics.overflow_over;
    annotation_space_block_start = annotation_metrics.space_under;
    annotation_space_block_end = annotation_metrics.space_over;
  }

  LayoutUnit block_offset_shift = annotation_overflow_block_start;
  // If the previous line has block-end annotation overflow and this line has
  // block-start annotation space, shift up the block offset of this line.
  if (GetConstraintSpace().BlockStartAnnotationSpace() < LayoutUnit() &&
      annotation_space_block_start) {
    const LayoutUnit overflow =
        -GetConstraintSpace().BlockStartAnnotationSpace();
    block_offset_shift = -std::min(annotation_space_block_start, overflow);
  }

  // If this line has block-start annotation overflow and the previous line
  // has block-end annotation space, borrow the block-end space of the
  // previous line and shift down the block offset by |overflow - space|.
  if (annotation_overflow_block_start &&
      GetConstraintSpace().BlockStartAnnotationSpace() > LayoutUnit()) {
    block_offset_shift = (annotation_overflow_block_start -
                          GetConstraintSpace().BlockStartAnnotationSpace())
                             .ClampNegativeToZero();
  }

  if (annotation_overflow_block_end)
    container_builder_.SetAnnotationOverflow(annotation_overflow_block_end);
  else if (annotation_space_block_end)
    container_builder_.SetBlockEndAnnotationSpace(annotation_space_block_end);

  return block_offset_shift;
}

bool InlineLayoutAlgorithm::AddAnyClearanceAfterLine(
    const LineInfo& line_info) {
  const InlineItemResults& line_items = line_info.Results();
  if (line_items.empty())
    return true;

  // If the last item was a <br> we need to adjust the content_size to clear
  // floats if specified. The <br> element must be at the back of the item
  // result list as it forces a line to break.
  const InlineItemResult& item_result = line_items.back();
  DCHECK(item_result.item);
  const InlineItem& item = *item_result.item;
  const LayoutObject* layout_object = item.GetLayoutObject();
  LayoutUnit content_size = container_builder_.LineHeight();

  // layout_object may be null in certain cases, e.g. if it's a kBidiControl.
  if (layout_object && layout_object->IsBR()) {
    const LayoutUnit line_box_bfc_block_offset =
        *container_builder_.LineBoxBfcBlockOffset();
    BfcOffset bfc_offset = {LayoutUnit(),
                            line_box_bfc_block_offset + content_size};
    LayoutUnit block_end_offset_without_clearence = bfc_offset.block_offset;
    const auto clear_type = item.Style()->Clear(Style());
    if (clear_type != EClear::kNone) {
      AdjustToClearance(GetExclusionSpace().ClearanceOffset(clear_type),
                        &bfc_offset);

      // Unlike regular CSS clearance (which adds space *before* content), BR
      // clearance is about adding space *after* content. Store the amount of
      // space to add, so that we push subsequent content (and stretch the
      // container) past the relevant floats.
      container_builder_.SetClearanceAfterLine(
          bfc_offset.block_offset - block_end_offset_without_clearence);
    }

    if (GetConstraintSpace().HasBlockFragmentation() &&
        GetExclusionSpace().NeedsClearancePastFragmentainer(clear_type)) {
      return false;
    }
  }
  return true;
}

const LayoutResult* InlineLayoutAlgorithm::Layout() {
  const auto& constraint_space = GetConstraintSpace();
  ExclusionSpace initial_exclusion_space(constraint_space.GetExclusionSpace());

  // Clear break tokens (for fragmented floats) propagated from the previous
  // line (or even the *current* line, in cases where we retry layout after
  // having resolved the BFC offset).
  context_->ClearParallelFlowBreakTokens();

  end_margin_strut_ = constraint_space.GetMarginStrut();
  container_builder_.SetAdjoiningObjectTypes(
      constraint_space.GetAdjoiningObjectTypes());
  lines_until_clamp_ = constraint_space.LinesUntilClamp();

  // In order to get the correct list of layout opportunities, we need to
  // position any "leading" floats within the exclusion space first.
  LeadingFloats leading_floats;
  PositionLeadingFloats(initial_exclusion_space, leading_floats);

  // Determine our BFC block-offset, but *don't* set it on the builder yet as
  // we might be an empty line.
  bool is_pushed_by_floats = false;
  LayoutUnit bfc_block_offset =
      constraint_space.ForcedBfcBlockOffset().value_or(
          constraint_space.GetBfcOffset().block_offset +
          constraint_space.GetMarginStrut().Sum());

  // Also apply clearance if necessary.
  if (constraint_space.HasClearanceOffset() &&
      bfc_block_offset < constraint_space.ClearanceOffset()) {
    bfc_block_offset = constraint_space.ClearanceOffset();
    is_pushed_by_floats = true;
  }

  // For initial letter, we should clear previous block's initial letter[1][2]
  // if:
  //   - new formatting context
  //   - starts with an initial letter
  //   - `clear` in start direction of initial letter containing block.
  //
  // [1] https://drafts.csswg.org/css-inline/#short-para-initial-letter
  // [2]
  // https://wpt.live/css/css-inline/initial-letter/initial-letter-short-para-initial-letter-clears.html
  if (!context_->ItemsBuilder()->Size()) {
    const EClear clear_type =
        UNLIKELY(Node().HasInitialLetterBox())
            ? EClear::kBoth
            : Node().Style().Clear(constraint_space.Direction());
    const LayoutUnit initial_letter_clearance =
        constraint_space.GetExclusionSpace().InitialLetterClearanceOffset(
            clear_type);
    if (initial_letter_clearance > bfc_block_offset) {
      // The initial letter box causes container separation to reuse layout
      // result, e.g.
      //    <div class="initial-letter-1">abc</div>
      //    <!-- change to 11px and will result in a bad layout -->
      //    <div style="height: 1px"></div>
      //    <div class="initial-letter-2">xyz</div>
      //
      bfc_block_offset = initial_letter_clearance;
      is_pushed_by_floats = true;
    }
  }

  // We query all the layout opportunities on the initial exclusion space up
  // front, as if the line breaker may add floats and change the opportunities.
  const LayoutOpportunityVector& opportunities =
      initial_exclusion_space.AllLayoutOpportunities(
          {constraint_space.GetBfcOffset().line_offset, bfc_block_offset},
          constraint_space.AvailableSize().inline_size);

  const InlineBreakToken* break_token = GetBreakToken();

  if (break_token && break_token->IsInParallelBlockFlow()) {
    container_builder_.SetIsLineForParallelFlow();
  }

  FragmentItemsBuilder* const items_builder = context_->ItemsBuilder();
  DCHECK(items_builder);
  LogicalLineItems* const line_box = items_builder->AcquireLogicalLineItems();
  DCHECK(line_box);
  // Determine which line breaker to use.
  LineBreakStrategy line_break_strategy(context_, Node(), Style(), break_token,
                                        column_spanner_path_);
  bool is_line_created = false;
  bool is_end_paragraph = false;
  LayoutUnit line_block_size;
  LayoutUnit block_delta;
  const auto* opportunities_it = opportunities.begin();
  while (opportunities_it != opportunities.end()) {
    const LayoutOpportunity& opportunity = *opportunities_it;

#if DCHECK_IS_ON()
    // Make sure the last opportunity has the correct properties.
    if (opportunities_it + 1 == opportunities.end()) {
      // We shouldn't have any shapes affecting the last opportunity.
      DCHECK(!opportunity.HasShapeExclusions());
      DCHECK_EQ(line_block_size, LayoutUnit());
      DCHECK_EQ(block_delta, LayoutUnit());

      // The opportunity should match the given available size, (however need
      // to check if the inline-size got saturated first).
      if (opportunity.rect.InlineSize() != LayoutUnit::Max()) {
        DCHECK_EQ(opportunity.rect.InlineSize(),
                  constraint_space.AvailableSize().inline_size);
      }
      DCHECK_EQ(opportunity.rect.BlockSize(), LayoutUnit::Max());
    }
#endif

    // Reset any state that may have been modified in a previous pass.
    container_builder_.Reset();
    container_builder_.SetExclusionSpace(initial_exclusion_space);
    is_line_created = false;

    LineLayoutOpportunity line_opportunity =
        opportunity.ComputeLineLayoutOpportunity(constraint_space,
                                                 line_block_size, block_delta);
    if (UNLIKELY(line_break_strategy.NeedsToPrepare())) {
      line_break_strategy.Prepare(
          context_, Node(), constraint_space,
          base::make_span(opportunities_it, opportunities.end()),
          line_opportunity, leading_floats, break_token, &GetExclusionSpace());
    }
    bool is_line_info_cached = false;
    LineInfo& line_info =
        context_->GetLineInfo(break_token, is_line_info_cached);
    if (UNLIKELY(is_line_info_cached)) {
      // Update the BFC offset because it was not known when the `line_info` was
      // cached.
      line_info.SetBfcOffset({line_opportunity.line_left_offset,
                              line_opportunity.bfc_block_offset});
    } else {
      LineBreaker line_breaker(Node(), LineBreakerMode::kContent,
                               constraint_space, line_opportunity,
                               leading_floats, break_token,
                               column_spanner_path_, &GetExclusionSpace());
      line_break_strategy.SetupLineBreaker(context_, line_breaker);
      line_breaker.NextLine(&line_info);
    }

    if (UNLIKELY(Node().IsInitialLetterBox())) {
      // Because `LineBreaker` doesn't calculate the inline size of initial
      // letter box from text ink bounds as performance reason. We calculate
      // here for `LineInfo::Width()` for text alignment and RTL[1][2].
      // [1]
      // https://wpt.live/css/css-inline/initial-letter/initial-letter-indentation-rtl.html
      // [2]
      // https://wpt.live/css/css-inline/initial-letter/initial-letter-indentation.html
      line_info.SetWidth(line_info.AvailableWidth(),
                         CalculateInitialLetterBoxInlineSize(line_info));
    }

    const auto* block_in_inline_result = line_info.BlockInInlineLayoutResult();
    if (block_in_inline_result) {
      if (UNLIKELY(block_in_inline_result->Status() !=
                   LayoutResult::kSuccess)) {
        items_builder->ReleaseCurrentLogicalLineItems();
        return block_in_inline_result;
      }

      if (block_in_inline_result->IsPushedByFloats())
        container_builder_.SetIsPushedByFloats();
      else if (block_in_inline_result->SubtreeModifiedMarginStrut())
        container_builder_.SetSubtreeModifiedMarginStrut();
    }

    // Set our BFC block-offset if we aren't an empty line.
    if (!line_info.IsEmptyLine()) {
      if (block_in_inline_result && block_in_inline_result->BfcBlockOffset()) {
        const auto result_bfc_block_offset =
            block_in_inline_result->BfcBlockOffset();
        container_builder_.SetBfcBlockOffset(*result_bfc_block_offset);
        container_builder_.SetLineBoxBfcBlockOffset(*result_bfc_block_offset);
      } else {
        container_builder_.SetBfcBlockOffset(bfc_block_offset);
        container_builder_.SetLineBoxBfcBlockOffset(
            line_info.GetBfcOffset().block_offset);
        if (is_pushed_by_floats)
          container_builder_.SetIsPushedByFloats();
      }

      // Abort if something before needs to know the correct BFC block-offset.
      if (container_builder_.GetAdjoiningObjectTypes() &&
          bfc_block_offset != constraint_space.ExpectedBfcBlockOffset()) {
        items_builder->ReleaseCurrentLogicalLineItems();
        return container_builder_.Abort(LayoutResult::kBfcBlockOffsetResolved);
      }
    }

    // If this fragment will be larger than the inline-size of the opportunity,
    // *and* the opportunity is smaller than the available inline-size, and the
    // container autowraps, continue to the next opportunity.
    if (line_info.HasOverflow() &&
        !line_opportunity.IsEqualToAvailableFloatInlineSize(
            constraint_space.AvailableSize().inline_size) &&
        Node().Style().ShouldWrapLine()) {
      DCHECK(!line_info.IsBlockInInline());

      // Shapes are *special*. We need to potentially increment the block-delta
      // by 1px each loop to properly test each potential position of the line.
      if (UNLIKELY(opportunity.HasShapeExclusions()) &&
          block_delta < opportunity.rect.BlockSize() &&
          !opportunity.IsBlockDeltaBelowShapes(block_delta)) {
        block_delta += LayoutUnit(1);
        line_block_size = LayoutUnit();
        continue;
      }
      // We've either don't have any shapes, or run out of block-delta space
      // to test, proceed to the next layout opportunity.
      if (opportunities_it + 1 != opportunities.end()) {
        block_delta = LayoutUnit();
        line_block_size = LayoutUnit();
        ++opportunities_it;
        continue;
      }
      // Normally the last opportunity should fit the line, but arithmetic
      // overflow can lead to failures for all opportunities. Just let the line
      // to overflow in that case.
    }

    PrepareBoxStates(line_info, break_token);

    CreateLine(line_opportunity, &line_info, line_box);
    is_line_created = true;
    is_end_paragraph = line_info.IsEndParagraph();

    // Adjust the line BFC block-offset if we have a ruby annotation, raise
    // initial letter or sunken initial letter.
    const LayoutUnit block_start_adjust =
        line_info.ComputeBlockStartAdjustment();
    if (UNLIKELY(block_start_adjust)) {
      DCHECK(container_builder_.BfcBlockOffset());
      DCHECK(container_builder_.LineBoxBfcBlockOffset());
      DCHECK(!line_info.IsEmptyLine());
      container_builder_.SetLineBoxBfcBlockOffset(
          line_info.GetBfcOffset().block_offset + block_start_adjust);
      container_builder_.SetAnnotationBlockOffsetAdjustment(
          line_info.ComputeAnnotationBlockOffsetAdjustment());
    }

    // We now can check the block-size of the fragment, and it fits within the
    // opportunity. Also include the ruby annotations so that they don't
    // intersect with any floats.
    //
    // To use next opportunity, `total_block_size` contains initial letter box
    // block-size.
    //
    // opportunities[0] 111    ***** his is first line.
    //                  111      *   This is second line.
    // opportunities[1] 222222   *   This is third line.
    //                  222222   *   This is fourth line.
    //                  This is fifth line.
    // opportunities[2] 333 This is sixth line.
    //
    // where '1', '2', '3' are `float:left` with `clear:left`.
    //
    // If we don't set `initial_letter_box_block_size`, the test[1] fails.
    // [1]
    // https://wpt.live/css/css-inline/initial-letter/initial-letter-floats-003.html
    const LayoutUnit total_block_size = line_info.ComputeTotalBlockSize(
        container_builder_.LineHeight(),
        container_builder_.AnnotationOverflow().ClampNegativeToZero());

    // Now that we have the block-size of the line, we can re-test the layout
    // opportunity to see if we fit into the (potentially) non-rectangular
    // shape area.
    //
    // If the AvailableInlineSize changes we need to run the line breaker again
    // with the calculated line_block_size. This is *safe* as the line breaker
    // won't produce a line which has a larger block-size, (as it can only
    // decrease or stay the same size).
    //
    // We skip attempting to fit empty lines into the shape area, as they
    // should only contain floats and/or abs-pos which shouldn't be affected by
    // this logic.
    if (UNLIKELY(opportunity.HasShapeExclusions() &&
                 !line_info.IsEmptyLine())) {
      LineLayoutOpportunity line_opportunity_with_height =
          opportunity.ComputeLineLayoutOpportunity(
              constraint_space, total_block_size, block_delta);

      if (line_opportunity_with_height.AvailableInlineSize() !=
          line_opportunity.AvailableInlineSize()) {
        line_block_size = total_block_size;
        continue;
      }
    }

    // Check if the line will fit in the current opportunity.
    if (total_block_size + block_delta > opportunity.rect.BlockSize()) {
      block_delta = LayoutUnit();
      line_block_size = LayoutUnit();
      ++opportunities_it;
      continue;
    }

    // Success!
    container_builder_.SetBreakToken(line_info.GetBreakToken());
    container_builder_.SetBaseDirection(line_info.BaseDirection());

    // Propagate any break tokens for floats that we fragmented before or inside
    // to the block container in 3 steps: 1) in `PositionLeadingFloats`, 2) from
    // `LineInfo` here, 3) then `CreateLine` may propagate more.
    for (const BreakToken* parallel_token :
         line_info.ParallelFlowBreakTokens()) {
      DCHECK(parallel_token->IsInlineType());
      DCHECK(To<InlineBreakToken>(parallel_token)->IsInParallelBlockFlow());
      context_->PropagateParallelFlowBreakToken(parallel_token);
    }
    if (absl::optional<LayoutUnit> minimum_space_shortage =
            line_info.MinimumSpaceShortage()) {
      container_builder_.PropagateSpaceShortage(minimum_space_shortage);
    }

    if (line_info.IsEmptyLine()) {
      DCHECK_EQ(container_builder_.BlockSize(), LayoutUnit());
      DCHECK(!container_builder_.BfcBlockOffset());

      container_builder_.SetIsSelfCollapsing();
      container_builder_.SetIsEmptyLineBox();

      // Finally respect the forced BFC block-offset if present.
      if (auto forced_bfc_block_offset =
              constraint_space.ForcedBfcBlockOffset()) {
        container_builder_.SetBfcBlockOffset(*forced_bfc_block_offset);
        container_builder_.SetLineBoxBfcBlockOffset(*forced_bfc_block_offset);
      }
    } else {
      if (!AddAnyClearanceAfterLine(line_info)) {
        return container_builder_.Abort(LayoutResult::kOutOfFragmentainerSpace);
      }
      container_builder_.SetBlockSize(container_builder_.LineHeight());

      // Margins should only collapse across "certain zero-height line boxes".
      // https://drafts.csswg.org/css2/box.html#collapsing-margins
      if (!line_info.IsBlockInInline()) {
        end_margin_strut_ = MarginStrut();
        if (lines_until_clamp_)
          *lines_until_clamp_ = *lines_until_clamp_ - 1;
      }

      // As we aren't an empty inline we should have correctly placed all
      // our adjoining objects, and shouldn't propagate this information
      // to siblings.
      container_builder_.ResetAdjoiningObjectTypes();
    }
    break;
  }

  CHECK(is_line_created);
  container_builder_.SetEndMarginStrut(end_margin_strut_);
  container_builder_.SetLinesUntilClamp(lines_until_clamp_);

  DCHECK(items_builder);
  container_builder_.PropagateChildrenData(*line_box);
  const LayoutResult* layout_result = container_builder_.ToLineBoxFragment();
  items_builder->AssociateLogicalLineItems(
      line_box, layout_result->GetPhysicalFragment());
  line_break_strategy.DidCreateLine(is_end_paragraph);
  return layout_result;
}

// This positions any "leading" floats within the given exclusion space.
// If we are also an empty inline, it will add any out-of-flow descendants.
void InlineLayoutAlgorithm::PositionLeadingFloats(
    ExclusionSpace& exclusion_space,
    LeadingFloats& leading_floats) {
  if (GetBreakToken() && GetBreakToken()->IsInParallelBlockFlow()) {
    // Bail, and let the line breaker deal with any kind of parallel flow.
    return;
  }

  const HeapVector<InlineItem>& items =
      Node().ItemsData(/* is_first_line */ false).items;

  unsigned index = GetBreakToken() ? GetBreakToken()->StartItemIndex() : 0;
  PositionedFloatVector& positioned_floats = leading_floats.floats;
  for (; index < items.size(); ++index) {
    const InlineItem& item = items[index];

    // Abort if we've found something non-empty.
    if (!item.IsEmptyItem())
      break;

    if (item.Type() != InlineItem::kFloating) {
      continue;
    }

    container_builder_.AddAdjoiningObjectTypes(
        item.GetLayoutObject()->StyleRef().Floating(
            GetConstraintSpace().Direction()) == EFloat::kLeft
            ? kAdjoiningFloatLeft
            : kAdjoiningFloatRight);

    // Place any floats at the "expected" BFC block-offset, this may be an
    // optimistic guess.
    const LayoutUnit origin_bfc_block_offset =
        GetConstraintSpace().ExpectedBfcBlockOffset();
    PositionedFloat positioned_float = PositionFloat(
        origin_bfc_block_offset, item.GetLayoutObject(), &exclusion_space);

    if (GetConstraintSpace().HasBlockFragmentation()) {
      // Propagate any breaks before or inside floats to the block container.
      if (const auto* float_break_token = positioned_float.BreakToken()) {
        const auto* parallel_token =
            InlineBreakToken::CreateForParallelBlockFlow(
                node_, {index, item.StartOffset()}, *float_break_token);
        context_->PropagateParallelFlowBreakToken(parallel_token);
      }
    }

    positioned_floats.push_back(positioned_float);
  }

  leading_floats.handled_index = index;
}

PositionedFloat InlineLayoutAlgorithm::PositionFloat(
    LayoutUnit origin_bfc_block_offset,
    LayoutObject* floating_object,
    ExclusionSpace* exclusion_space) {
  const auto& space = GetConstraintSpace();
  BfcOffset origin_bfc_offset = {space.GetBfcOffset().line_offset,
                                 origin_bfc_block_offset};

  UnpositionedFloat unpositioned_float(
      BlockNode(To<LayoutBox>(floating_object)),
      /* break_token */ nullptr, space.AvailableSize(),
      space.PercentageResolutionSize(),
      space.ReplacedPercentageResolutionSize(), origin_bfc_offset, space,
      Style());

  PositionedFloat positioned_float =
      ::blink::PositionFloat(&unpositioned_float, exclusion_space);

  if (positioned_float.minimum_space_shortage) {
    container_builder_.PropagateSpaceShortage(
        positioned_float.minimum_space_shortage);
  }

  return positioned_float;
}

void InlineLayoutAlgorithm::BidiReorder(TextDirection base_direction,
                                        LogicalLineItems* line_box) {
  if (line_box->IsEmpty())
    return;

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
    if (item.has_only_trailing_spaces) {
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
      if (level == kOpaqueBidiLevel)
        level = last_level;
      else
        last_level = level;
    }
  }

  // Compute visual indices from resolved levels.
  Vector<int32_t, 32> indices_in_visual_order(levels.size());
  BidiParagraph::IndicesInVisualOrder(levels, &indices_in_visual_order);

  // Reorder to the visual order.
  LogicalLineItems& visual_items = context_->AcquireTempLogicalLineItems();
  visual_items.ReserveInitialCapacity(line_box->size());
  for (unsigned logical_index : indices_in_visual_order)
    visual_items.AddChild(std::move((*line_box)[logical_index]));
  DCHECK_EQ(line_box->size(), visual_items.size());
  line_box->swap(visual_items);
  context_->ReleaseTempLogicalLineItems(visual_items);
}

}  // namespace blink
