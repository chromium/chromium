// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

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
#include "third_party/blink/renderer/core/layout/inline/justification_utils.h"
#include "third_party/blink/renderer/core/layout/inline/line_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/inline/line_breaker.h"
#include "third_party/blink/renderer/core/layout/inline/line_info.h"
#include "third_party/blink/renderer/core/layout/inline/line_truncator.h"
#include "third_party/blink/renderer/core/layout/inline/line_widths.h"
#include "third_party/blink/renderer/core/layout/inline/logical_line_builder.h"
#include "third_party/blink/renderer/core/layout/inline/logical_line_container.h"
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
#include "third_party/blink/renderer/platform/heap/collection_support/clear_collection_scope.h"

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
      const TextWrapStyle text_wrap = block_style.GetTextWrapStyle();
      if (text_wrap == TextWrapStyle::kBalance) [[unlikely]] {
        score_line_break_context_ = context->GetScoreLineBreakContext();
        initiate_balancing_ = !break_token;
        if (initiate_balancing_) {
          DCHECK(!score_line_break_context_ ||
                 score_line_break_context_->IsActive());
          use_score_line_break_ = score_line_break_context_;
        }
      } else if (text_wrap == TextWrapStyle::kPretty) [[unlikely]] {
        score_line_break_context_ = context->GetScoreLineBreakContext();
        use_score_line_break_ =
            score_line_break_context_ && score_line_break_context_->IsActive();
      } else {
        DCHECK(ShouldWrapLineGreedy(text_wrap));
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
    if (const std::optional<LayoutUnit>& balanced_available_width =
            context->BalancedAvailableWidth()) [[unlikely]] {
      DCHECK(!score_line_break_context_ ||
             !score_line_break_context_->CurrentLineBreakPoint());
      line_breaker.OverrideAvailableWidth(*balanced_available_width);
    } else if (score_line_break_context_) [[unlikely]] {
      if (const LineBreakPoint* break_point =
              score_line_break_context_->CurrentLineBreakPoint()) {
        line_breaker.SetBreakAt(*break_point);
      }
    }
  }

  void DidCreateLine(bool is_end_paragraph) {
    if (score_line_break_context_) [[unlikely]] {
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
      if (const std::optional<LayoutUnit> balanced_available_width =
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
  }

  void Optimize(const InlineNode& node,
                const ConstraintSpace& space,
                const base::span<const LayoutOpportunity>& opportunities,
                const LeadingFloats& leading_floats,
                const InlineBreakToken* break_token,
                ExclusionSpace* exclusion_space) {
    DCHECK(score_line_break_context_->GetLineBreakPoints().empty());
    DCHECK_EQ(score_line_break_context_->LineBreakPointsIndex(), 0u);
    if (!score_line_break_context_->IsActive()) [[unlikely]] {
      return;
    }
    const base::ElapsedTimer timer;
    LineWidths line_widths;
    if (!line_widths.Set(node, opportunities, break_token)) [[unlikely]] {
      // The next line may have less opportunities that keep running, without
      // suspending the context.
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
    }
  }

  bool initiate_balancing_ = false;
  bool use_score_line_break_ = false;
  ScoreLineBreakContext* score_line_break_context_ = nullptr;
};

void PlaceRelativePositionedItems(const ConstraintSpace& constraint_space,
                                  LogicalLineItems* line_box) {
  for (auto& child : *line_box) {
    const auto* physical_fragment = child.GetPhysicalFragment();
    if (!physical_fragment) {
      continue;
    }
    child.rect.offset += ComputeRelativeOffsetForInline(
        constraint_space, physical_fragment->Style());
  }
}

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

bool InlineLayoutAlgorithm::HasContainerBorderPaddingAtBlockStart() const {
  return context_->ContainerBuilder()->BorderPadding().block_start !=
         LayoutUnit();
}

bool InlineLayoutAlgorithm::HasContainerBorderPaddingAtBlockEnd() const {
  return context_->ContainerBuilder()->BorderPadding().block_end !=
         LayoutUnit();
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
  LogicalLineBuilder(Node(), GetConstraintSpace(), nullptr, box_states_,
                     context_)
      .RebuildBoxStates(line_info, 0u, break_token->StartItemIndex());
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

#if EXPENSIVE_DCHECKS_ARE_ON()
void InlineLayoutAlgorithm::CheckBoxStates(const LineInfo& line_info) const {
  if (!is_box_states_from_context_) {
    return;
  }
  InlineLayoutStateStack rebuilt;
  LogicalLineBuilder(Node(), GetConstraintSpace(), nullptr, &rebuilt, context_)
      .RebuildBoxStates(line_info, 0u, GetBreakToken()->StartItemIndex());
  LogicalLineItems& line_box = context_->AcquireTempLogicalLineItems();
  rebuilt.OnBeginPlaceItems(Node(), line_info.LineStyle(), baseline_type_,
                            quirks_mode_, &line_box);
  DCHECK(box_states_);
  box_states_->CheckSame(rebuilt);
  context_->ReleaseTempLogicalLineItems(line_box);
}
#endif

ALWAYS_INLINE InlineLayoutAlgorithm::LineClampState
InlineLayoutAlgorithm::GetLineClampState(const LineInfo* line_info,
                                         LayoutUnit line_box_height) const {
  const ConstraintSpace& space = GetConstraintSpace();
  LineClampData line_clamp_data = space.GetLineClampData();
  if (line_clamp_data.IsLineClampContext()) {
    if (!line_info->IsBlockInInline() && line_clamp_data.IsAtClampPoint()) {
      return LineClampState::kEllipsize;
    }
    if (line_clamp_data.ShouldHideForPaint()) {
      return LineClampState::kHide;
    }
  } else if (!line_info->IsBlockInInline() && line_info->HasOverflow() &&
             node_.GetLayoutBlockFlow()->ShouldTruncateOverflowingText()) {
    return LineClampState::kEllipsize;
  }

  return LineClampState::kShow;
}

void InlineLayoutAlgorithm::CreateLine(const LineLayoutOpportunity& opportunity,
                                       LineInfo* line_info,
                                       LogicalLineContainer* line_container) {
  LogicalLineItems* line_box = &line_container->BaseLine();
  // Apply justification before placing items, because it affects size/position
  // of items, which are needed to compute inline static positions.
  LayoutUnit line_offset_for_text_align = ApplyTextAlign(line_info);

  // Clear the current line without releasing the buffer.
  line_container->Shrink();

  LogicalLineBuilder line_builder(Node(), GetConstraintSpace(), GetBreakToken(),
                                  box_states_, context_);
  line_builder.CreateLine(line_info, line_box, this);

  const LayoutUnit hang_width = line_info->HangWidth();
  const LayoutUnit position =
      AdjustLineOffsetForHanging(line_info, line_offset_for_text_align);
  LayoutUnit inline_size = box_states_->ComputeInlinePositions(
      line_box, position, line_info->IsBlockInInline());
  if (hang_width) [[unlikely]] {
    // If we've shifted the line items the inline-size is already correct.
    if (position == LayoutUnit())
      inline_size -= hang_width;
    container_builder_.SetHangInlineSize(hang_width);
  }

  // Force an editable empty line or a line with ruby annotations to have
  // metrics, so that is has a height.
  if (line_info->HasLineEvenIfEmpty() || !box_states_->RubyColumnList().empty())
      [[unlikely]] {
    box_states_->LineBoxState().EnsureTextMetrics(
        line_info->LineStyle(), *box_states_->LineBoxState().font,
        baseline_type_);
  } else if (line_builder.InitialLetterItemResult() &&
             box_states_->LineBoxState().metrics.IsEmpty()) [[unlikely]] {
    box_states_->LineBoxState().metrics = FontHeight();
  }

  const FontHeight& line_box_metrics = box_states_->LineBoxState().metrics;

  if (Node().HasRuby() && !line_info->IsEmptyLine()) [[unlikely]] {
    std::optional<FontHeight> annotation_metrics;
    if (!box_states_->RubyColumnList().empty()) {
      HeapVector<Member<LogicalRubyColumn>>& column_list =
          box_states_->RubyColumnList();
      UpdateRubyColumnInlinePositions(*line_box, inline_size, column_list);
      RubyBlockPositionCalculator calculator;
      calculator.GroupLines(column_list)
          .PlaceLines(*line_box, line_box_metrics)
          .AddLinesTo(*line_container);
      annotation_metrics = calculator.AnnotationMetrics();
    }
    line_info->SetAnnotationBlockStartAdjustment(SetAnnotationOverflow(
        *line_info, *line_box, line_box_metrics, annotation_metrics));
  }

  // Truncate the line if:
  //  - 'text-overflow: ellipsis' is set and we *aren't* a line-clamp context.
  //  - If we've reached the line-clamp limit.
  const LineClampState line_clamp_state =
      GetLineClampState(line_info, line_box_metrics.LineHeight());
  if (line_clamp_state == LineClampState::kEllipsize) [[unlikely]] {
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

  // With the CSSLineClamp feature, if we're past the clamp point, we mark every
  // inline item in the line as hidden for paint.
  if (line_clamp_state == LineClampState::kHide) [[unlikely]] {
    container_builder_.SetIsHiddenForPaint(true);
    for (auto& child : *line_box) {
      child.is_hidden_for_paint = true;
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

  if (line_builder.InitialLetterItemResult()) [[unlikely]] {
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
        BoxStrut(line_builder.InitialLetterItemResult()->margins,
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
  if (line_builder.HasOutOfFlowPositionedItems()) {
    DCHECK(!line_info->IsBlockInInline());
    PlaceOutOfFlowObjects(*line_info, line_box_metrics, line_box);
  }

  // Place floating objects.
  // This adjusts the  LogicalLineItem::offset member to
  // contain the position of the float relative to the linebox.
  // Additionally it will perform layout on any unpositioned floats which
  // needed the line height to correctly determine their final position.
  if (line_builder.HasFloatingItems()) {
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
  if (line_builder.HasRelativePositionedItems()) {
    PlaceRelativePositionedItems(GetConstraintSpace(), line_box);
  }
  for (auto annotation_line : line_container->AnnotationLineList()) {
    PlaceRelativePositionedItems(GetConstraintSpace(),
                                 annotation_line.line_items);
  }

  // Apply any relative positioned offsets to any boxes (and their children).
  box_states_->ApplyRelativePositioning(GetConstraintSpace(), line_box,
                                        nullptr);

  // Create box fragments if needed. After this point forward, |line_box| is a
  // tree structure.
  // The individual children don't move position within the |line_box|, rather
  // the children have their layout_result, fragment, (or similar) set to null,
  // creating a "hole" in the array.
  box_states_->CreateBoxFragments(GetConstraintSpace(), line_box,
                                  line_info->IsBlockInInline());
  box_states_->ClearRubyColumnList();

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

  const ConstraintSpace& space = GetConstraintSpace();
  if (space.ShouldTextBoxTrimStart() || space.ShouldTextBoxTrimEnd())
      [[unlikely]] {
    bool is_truncated = line_clamp_state == LineClampState::kEllipsize ||
                        space.GetLineClampData().state ==
                            LineClampData::kMeasureLinesUntilBfcOffset;
    ApplyTextBoxTrim(*line_info, is_truncated);
  }

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
  if (Node().IsTextCombine()) [[unlikely]] {
    // The effective size of combined text is 1em square[1]
    // [1] https://drafts.csswg.org/css-writing-modes-3/#text-combine-layout
    const auto one_em = Node().Style().ComputedFontSizeAsFixed();
    inline_size = std::min(inline_size, one_em);
  } else if (Node().IsInitialLetterBox()) [[unlikely]] {
    const FontHeight& adjusted_metrics =
        AdjustInitialLetterInTextPosition(line_box_metrics, line_box);
    if (!adjusted_metrics.IsEmpty()) {
      container_builder_.SetMetrics(adjusted_metrics);
      line_container->MoveInBlockDirection(adjusted_metrics.ascent);
    }
  } else if (!Node().IsSvgText()) [[likely]] {
    // Convert baseline relative block offset of `LogicalLineItem::rect` to
    // to line box relative block offset.
    line_container->MoveInBlockDirection(line_box_metrics.ascent);
  }

  container_builder_.SetInlineSize(inline_size);
}

void InlineLayoutAlgorithm::ApplyTextBoxTrim(LineInfo& line_info,
                                             bool is_truncated) {
  const ConstraintSpace& space = GetConstraintSpace();
  if (const LayoutResult* block_in_inline =
          line_info.BlockInInlineLayoutResult()) {
    // If this is a wrapper line of a block-in-inline, the trimming is applied
    // to the block. Propagate the result from the block, without trimming the
    // wrapper line.
    if (block_in_inline->IsBlockStartTrimmed() &&
        space.ShouldTextBoxTrimStart()) {
      container_builder_.SetIsBlockStartTrimmed();
    }
    if (block_in_inline->IsBlockEndTrimmed() && space.ShouldTextBoxTrimEnd()) {
      container_builder_.SetIsBlockEndTrimmed();
    }
    return;
  }

  const bool should_apply_start =
      space.ShouldTextBoxTrimStart() && line_info.IsFirstFormattedLine();
  const bool should_apply_end = space.ShouldTextBoxTrimEnd() &&
                                (is_truncated || !line_info.GetBreakToken() ||
                                 space.ShouldForceTextBoxTrimEnd());
  if (!should_apply_start && !should_apply_end) {
    return;
  }

  const ComputedStyle& line_style = line_info.LineStyle();
  const bool is_flipped_line = line_style.IsFlippedLinesWritingMode();
  bool should_apply_over = should_apply_start;
  bool should_apply_under = should_apply_end;
  if (is_flipped_line) [[unlikely]] {
    should_apply_over = should_apply_end;
    should_apply_under = should_apply_start;
  }

  const FontHeight line_box_metrics = container_builder_.Metrics();
  FontHeight intrinsic_metrics = line_box_metrics;
  InlineBoxState::AdjustEdges(
      space.EffectiveTextBoxEdge(), line_style.GetFont(), baseline_type_,
      should_apply_over, should_apply_under, intrinsic_metrics);

  if (should_apply_start) {
    if (HasContainerBorderPaddingAtBlockStart()) [[unlikely]] {
      // If there is intervening non-zero padding or borders, there is no
      // effect, but report that it is applied to stop the propagation.
      container_builder_.SetIsBlockStartTrimmed();
    } else {
      // Apply `text-box-trim: start` if this is the first formatted line.
      LayoutUnit offset_for_trimming_box;
      if (is_flipped_line) [[unlikely]] {
        offset_for_trimming_box =
            intrinsic_metrics.descent - line_box_metrics.descent;
      } else {
        offset_for_trimming_box =
            intrinsic_metrics.ascent - line_box_metrics.ascent;
      }
      container_builder_.SetLineBoxBfcBlockOffset(
          container_builder_.LineBoxBfcBlockOffset()
              ? offset_for_trimming_box +
                    container_builder_.LineBoxBfcBlockOffset().value()
              : offset_for_trimming_box);
      container_builder_.SetIsBlockStartTrimmed();

      // Cancel adjusting the block start for the initial letters and Ruby
      // annotation. The use of the `text-box-trim` accepts the risk of
      // collisions for the finer control of the alignment of the body text in
      // the block direction.
      line_info.SetAnnotationBlockStartAdjustment(LayoutUnit());
      line_info.SetInitialLetterBlockStartAdjustment(LayoutUnit());
    }
  }

  if (should_apply_end) {
    if (HasContainerBorderPaddingAtBlockEnd()) [[unlikely]] {
      // If there is intervening non-zero padding or borders, there is no
      // effect, but report that it is applied to stop the propagation.
      container_builder_.SetIsBlockEndTrimmed();
    } else {
      // Ask the block layout algorithm to trim the end of the line box.
      LayoutUnit block_end_to_be_trimmed;
      if (is_flipped_line) [[unlikely]] {
        block_end_to_be_trimmed =
            line_box_metrics.ascent - intrinsic_metrics.ascent;
      } else {
        block_end_to_be_trimmed =
            line_box_metrics.descent - intrinsic_metrics.descent;
      }
      container_builder_.SetTrimBlockEndBy(block_end_to_be_trimmed);
      container_builder_.SetIsBlockEndTrimmed();
    }
  }
}

void InlineLayoutAlgorithm::PlaceBlockInInline(const InlineItem& item,
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
  if (box_fragment.MayHaveDescendantAboveBlockStart()) [[unlikely]] {
    container_builder_.SetMayHaveDescendantAboveBlockStart(true);
  }

  line_box->AddChild(std::move(item_result->layout_result),
                     /* offset */ LogicalOffset(), item_result->inline_size,
                     /* children_count */ 0, item.BidiLevel());
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

  if (has_rtl_block_level_out_of_flow_objects) [[unlikely]] {
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
    JustificationTarget target = JustificationTarget::kNormal;
    if (Node().IsSvgText()) {
      target = JustificationTarget::kSvgText;
    } else if (line_info->IsRubyBase()) {
      target = JustificationTarget::kRubyBase;
    } else if (line_info->IsRubyText()) {
      target = JustificationTarget::kRubyText;
    }
    std::optional<LayoutUnit> offset =
        ApplyJustification(space, target, line_info);
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
    const FontHeight& line_box_metrics,
    std::optional<FontHeight> annotation_font_height) {
  AnnotationMetrics annotation_metrics =
      ComputeAnnotationOverflow(line_box, line_box_metrics,
                                line_info.LineStyle(), annotation_font_height);
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
  const LayoutUnit content_size =
      container_builder_.LineHeight() -
      container_builder_.TrimBlockEndBy().value_or(LayoutUnit());

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
  lines_until_clamp_ = constraint_space.GetLineClampData().LinesUntilClamp(
      /*show_measured_lines*/ true);

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
    EClear clear_type;
    if (Node().HasInitialLetterBox()) [[unlikely]] {
      clear_type = EClear::kBoth;
    } else {
      clear_type = Node().Style().Clear(constraint_space.Direction());
    }
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
  LayoutOpportunityVector opportunities =
      initial_exclusion_space.AllLayoutOpportunities(
          {constraint_space.GetBfcOffset().line_offset, bfc_block_offset},
          constraint_space.AvailableSize().inline_size);
  ClearCollectionScope scope(&opportunities);

  const InlineBreakToken* break_token = GetBreakToken();

  if (break_token && break_token->IsInParallelBlockFlow()) {
    container_builder_.SetIsLineForParallelFlow();
  }

  FragmentItemsBuilder* const items_builder = context_->ItemsBuilder();
  DCHECK(items_builder);
  LogicalLineContainer* const line_container =
      items_builder->AcquireLogicalLineContainer();
  DCHECK(line_container);
  // Determine which line breaker to use.
  LineBreakStrategy line_break_strategy(context_, Node(), Style(), break_token,
                                        column_spanner_path_);
  bool is_line_created = false;
  bool is_end_paragraph = false;
  LayoutUnit line_block_size;
  LayoutUnit block_delta;
  auto opportunities_it = opportunities.begin();
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
    if (line_break_strategy.NeedsToPrepare()) [[unlikely]] {
      line_break_strategy.Prepare(
          context_, Node(), constraint_space,
          base::make_span(opportunities_it, opportunities.end()),
          line_opportunity, leading_floats, break_token, &GetExclusionSpace());
    }
    bool is_line_info_cached = false;
    LineInfo& line_info =
        context_->GetLineInfo(break_token, is_line_info_cached);
    if (is_line_info_cached) [[unlikely]] {
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

    if (Node().IsInitialLetterBox()) [[unlikely]] {
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
      if (block_in_inline_result->Status() != LayoutResult::kSuccess)
          [[unlikely]] {
        items_builder->ReleaseCurrentLogicalLineContainer();
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
        items_builder->ReleaseCurrentLogicalLineContainer();
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
      if (opportunity.HasShapeExclusions() &&
          block_delta < opportunity.rect.BlockSize() &&
          !opportunity.IsBlockDeltaBelowShapes(block_delta)) [[unlikely]] {
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

    CreateLine(line_opportunity, &line_info, line_container);
    is_line_created = true;
    is_end_paragraph = line_info.IsEndParagraph();

    // Adjust the line BFC block-offset if we have a ruby annotation, raise
    // initial letter or sunken initial letter.
    const LayoutUnit block_start_adjust =
        line_info.ComputeBlockStartAdjustment();
    if (block_start_adjust) [[unlikely]] {
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
    if (opportunity.HasShapeExclusions() && !line_info.IsEmptyLine())
        [[unlikely]] {
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
    for (const InlineBreakToken* parallel_token :
         line_info.ParallelFlowBreakTokens()) {
      DCHECK(parallel_token->IsInParallelBlockFlow());
      context_->PropagateParallelFlowBreakToken(parallel_token);
    }
    if (std::optional<LayoutUnit> minimum_space_shortage =
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

        if (lines_until_clamp_) {
          if (constraint_space.GetLineClampData().state ==
              LineClampData::kClampByLines) {
            *lines_until_clamp_ = *lines_until_clamp_ - 1;
          } else {
            DCHECK_EQ(constraint_space.GetLineClampData().state,
                      LineClampData::kMeasureLinesUntilBfcOffset);
            *lines_until_clamp_ = *lines_until_clamp_ + 1;
          }
        }
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
  if (lines_until_clamp_) {
    container_builder_.SetLinesUntilClamp(lines_until_clamp_);
  }

  DCHECK(items_builder);
  container_builder_.PropagateChildrenData(*line_container);
  const LayoutResult* layout_result = container_builder_.ToLineBoxFragment();
  items_builder->AssociateLogicalLineContainer(
      line_container, layout_result->GetPhysicalFragment());
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
  HeapVector<PositionedFloat>& positioned_floats = leading_floats.floats;
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

  // The BFC offset passed to `ShouldHideForPaint` should be the bottom offset
  // of the line, which we don't know at this point. However, since block layout
  // will relayout to fix the clamp BFC offset to the bottom of the last line
  // before clamp, we now that if the line's BFC offset is equal or greater than
  // the clamp BFC offset in the final relayout, the line will be hidden.
  bool is_hidden_for_paint =
      GetConstraintSpace().GetLineClampData().ShouldHideForPaint();
  UnpositionedFloat unpositioned_float(
      BlockNode(To<LayoutBox>(floating_object)),
      /* break_token */ nullptr, space.AvailableSize(),
      space.PercentageResolutionSize(),
      space.ReplacedPercentageResolutionSize(), origin_bfc_offset, space,
      Style(), space.FragmentainerBlockSize(), space.FragmentainerOffset(),
      is_hidden_for_paint);

  PositionedFloat positioned_float =
      ::blink::PositionFloat(&unpositioned_float, exclusion_space);

  if (positioned_float.minimum_space_shortage) {
    container_builder_.PropagateSpaceShortage(
        positioned_float.minimum_space_shortage);
  }

  return positioned_float;
}

}  // namespace blink
