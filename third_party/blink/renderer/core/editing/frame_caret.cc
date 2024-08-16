/*
 * Copyright (C) 2004, 2008, 2009, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/frame_caret.h"

#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/editing/caret_display_item_client.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/local_caret_rect.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/selection_editor.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"
#include "ui/gfx/selection_bound.h"

namespace blink {

namespace {

}  // anonymous namespace

FrameCaret::FrameCaret(LocalFrame& frame,
                       const SelectionEditor& selection_editor)
    : selection_editor_(&selection_editor),
      frame_(frame),
      display_item_client_(MakeGarbageCollected<CaretDisplayItemClient>()),
      caret_blink_timer_(frame.GetTaskRunner(TaskType::kInternalDefault),
                         this,
                         &FrameCaret::CaretBlinkTimerFired),
      effect_(EffectPaintPropertyNode::Create(
          EffectPaintPropertyNode::Root(),
          CaretEffectNodeState(/*visible*/ true,
                               TransformPaintPropertyNode::Root()))) {
#if DCHECK_IS_ON()
  effect_->SetDebugName("Caret");
#endif
}

FrameCaret::~FrameCaret() = default;

void FrameCaret::Trace(Visitor* visitor) const {
  visitor->Trace(selection_editor_);
  visitor->Trace(frame_);
  visitor->Trace(display_item_client_);
  visitor->Trace(caret_blink_timer_);
  visitor->Trace(effect_);
}

EffectPaintPropertyNode::State FrameCaret::CaretEffectNodeState(
    bool visible,
    const TransformPaintPropertyNodeOrAlias& local_transform_space) const {
  EffectPaintPropertyNode::State state;
  // Use 0.001f instead of 0 to ensure cc will add quad for the caret layer.
  // This is especially useful on Mac to limit the damage during caret blinking
  // within the CALayer for the caret.
  state.opacity = visible ? 1.f : 0.001f;
  state.local_transform_space = &local_transform_space;
  DEFINE_STATIC_LOCAL(
      CompositorElementId, element_id,
      (CompositorElementIdFromUniqueObjectId(
          NewUniqueObjectId(), CompositorElementIdNamespace::kPrimaryEffect)));
  state.compositor_element_id = element_id;
  state.direct_compositing_reasons = CompositingReason::kActiveOpacityAnimation;
  return state;
}

const PositionWithAffinity FrameCaret::CaretPosition() const {
  const VisibleSelection& selection =
      selection_editor_->ComputeVisibleSelectionInDOMTree();
  if (!selection.IsCaret())
    return PositionWithAffinity();
  DCHECK(selection.Start().IsValidFor(*frame_->GetDocument()));
  return PositionWithAffinity(selection.Start(), selection.Affinity());
}

bool FrameCaret::IsActive() const {
  return CaretPosition().IsNotNull();
}

void FrameCaret::UpdateAppearance() {
  DCHECK_GE(frame_->GetDocument()->Lifecycle().GetState(),
            DocumentLifecycle::kLayoutClean);

  bool new_should_show_caret = ShouldShowCaret();
  if (new_should_show_caret != should_show_caret_) {
    should_show_caret_ = new_should_show_caret;
    ScheduleVisualUpdateForPaintInvalidationIfNeeded();
  }

  if (!should_show_caret_) {
    StopCaretBlinkTimer();
    return;
  }
  // Start blinking with a black caret. Be sure not to restart if we're
  // already blinking in the right location.
  StartBlinkCaret();
}

void FrameCaret::StopCaretBlinkTimer() {
  if (caret_blink_timer_.IsActive() || IsVisibleIfActive())
    ScheduleVisualUpdateForPaintInvalidationIfNeeded();
  caret_blink_timer_.Stop();
  display_item_client_->SetActive(false);
  SetVisibleIfActive(false);
}

void FrameCaret::StartBlinkCaret() {
  // Start blinking with a black caret. Be sure not to restart if we're
  // already blinking in the right location at the right rate.
  base::TimeDelta blink_interval = LayoutTheme::GetTheme().CaretBlinkInterval();
  if (caret_blink_timer_.IsActive()) {
    if (blink_interval == caret_blink_timer_.RepeatInterval()) {
      // Already blinking at the right rate.
      return;
    }

    // If it was active but we are changing the blink rate, reset state.
    StopCaretBlinkTimer();
  }

  if (!blink_interval.is_zero()) {
    caret_blink_timer_.StartRepeating(blink_interval, FROM_HERE);
  }

  display_item_client_->SetActive(true);
  SetVisibleIfActive(true);
  ScheduleVisualUpdateForPaintInvalidationIfNeeded();
}

void FrameCaret::SetCaretEnabled(bool enabled) {
  if (is_caret_enabled_ == enabled)
    return;

  is_caret_enabled_ = enabled;

  if (!is_caret_enabled_)
    StopCaretBlinkTimer();
  ScheduleVisualUpdateForPaintInvalidationIfNeeded();
}

void FrameCaret::LayoutBlockWillBeDestroyed(const LayoutBlock& block) {
  display_item_client_->LayoutBlockWillBeDestroyed(block);
}

void FrameCaret::UpdateStyleAndLayoutIfNeeded() {
  DCHECK_GE(frame_->GetDocument()->Lifecycle().GetState(),
            DocumentLifecycle::kLayoutClean);
  UpdateAppearance();
  display_item_client_->UpdateStyleAndLayoutIfNeeded(
      should_show_caret_ ? CaretPosition() : PositionWithAffinity());
}

void FrameCaret::InvalidatePaint(const LayoutBlock& block,
                                 const PaintInvalidatorContext& context) {
  display_item_client_->InvalidatePaint(block, context);
}

gfx::Rect FrameCaret::AbsoluteCaretBounds() const {
  DCHECK_NE(frame_->GetDocument()->Lifecycle().GetState(),
            DocumentLifecycle::kInPrePaint);
  DCHECK(!frame_->GetDocument()->NeedsLayoutTreeUpdate());
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      frame_->GetDocument()->Lifecycle());

  return AbsoluteCaretBoundsOf(CaretPosition());
}

void FrameCaret::EnsureInvalidationOfPreviousLayoutBlock() {
  display_item_client_->EnsureInvalidationOfPreviousLayoutBlock();
}

bool FrameCaret::ShouldPaintCaret(const LayoutBlock& block) const {
  return display_item_client_->ShouldPaintCaret(block);
}

bool FrameCaret::ShouldPaintCaret(
    const PhysicalBoxFragment& box_fragment) const {
  return display_item_client_->ShouldPaintCaret(box_fragment);
}

void FrameCaret::SetVisibleIfActive(bool visible) {
  if (visible == IsVisibleIfActive())
    return;

  DCHECK(frame_);
  DCHECK(effect_);
  if (!frame_->View())
    return;

  auto change_type = effect_->Update(
      *effect_->Parent(),
      CaretEffectNodeState(visible, effect_->LocalTransformSpace()));
  DCHECK_EQ(PaintPropertyChangeType::kChangedOnlySimpleValues, change_type);
  if (auto* compositor = frame_->View()->GetPaintArtifactCompositor()) {
    if (compositor->DirectlyUpdateCompositedOpacityValue(*effect_)) {
      effect_->CompositorSimpleValuesUpdated();
      return;
    }
  }
  // Fallback to full update if direct update is not available.
  frame_->View()->SetPaintArtifactCompositorNeedsUpdate();
}

void FrameCaret::PaintCaret(GraphicsContext& context,
                            const PhysicalOffset& paint_offset) const {
  if (effect_->Update(
          context.GetPaintController().CurrentPaintChunkProperties().Effect(),
          CaretEffectNodeState(IsVisibleIfActive(),
                               context.GetPaintController()
                                   .CurrentPaintChunkProperties()
                                   .Transform())) !=
      PaintPropertyChangeType::kUnchanged) {
    // Needs full PaintArtifactCompositor update if the parent or the local
    // transform space changed.
    frame_->View()->SetPaintArtifactCompositorNeedsUpdate();
  }
  ScopedPaintChunkProperties scoped_properties(context.GetPaintController(),
                                               *effect_, *display_item_client_,
                                               DisplayItem::kCaret);

  display_item_client_->PaintCaret(context, paint_offset, DisplayItem::kCaret);

  if (!frame_->Selection().IsHidden()) {
    auto type = frame_->Selection().IsHandleVisible()
                    ? gfx::SelectionBound::Type::CENTER
                    : gfx::SelectionBound::Type::HIDDEN;

    if (type == gfx::SelectionBound::Type::CENTER ||
        base::FeatureList::IsEnabled(blink::features::kHiddenSelectionBounds)) {
      display_item_client_->RecordSelection(context, paint_offset, type);
    }
  }
}

bool FrameCaret::ShouldShowCaret() const {
  // Don't show the caret if it isn't visible or positioned.
  if (!is_caret_enabled_ || !IsActive())
    return false;

  Element* root = RootEditableElementOf(CaretPosition().GetPosition());
  if (root) {
    // Caret is contained in editable content. If there is no focused element,
    // don't show the caret.
    Element* focused_element = root->GetDocument().FocusedElement();
    if (!focused_element)
      return false;
  } else {
    // Caret is not contained in editable content--see if caret browsing is
    // enabled. If it isn't, don't show the caret.
    if (!frame_->IsCaretBrowsingEnabled())
      return false;
  }

  if (!IsEditablePosition(
          selection_editor_->ComputeVisibleSelectionInDOMTree().Start()) &&
      !frame_->IsCaretBrowsingEnabled())
    return false;

  // Only show the caret if the selection has focus.
  return frame_->Selection().SelectionHasFocus();
}

void FrameCaret::CaretBlinkTimerFired(TimerBase*) {
  DCHECK(is_caret_enabled_);
  if (IsCaretBlinkingSuspended() && IsVisibleIfActive())
    return;
  SetVisibleIfActive(!IsVisibleIfActive());
  ScheduleVisualUpdateForPaintInvalidationIfNeeded();
}

void FrameCaret::ScheduleVisualUpdateForPaintInvalidationIfNeeded() {
  if (LocalFrameView* frame_view = frame_->View())
    frame_view->ScheduleVisualUpdateForPaintInvalidationIfNeeded();
}

void FrameCaret::RecreateCaretBlinkTimerForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::TickClock* tick_clock) {
  caret_blink_timer_.SetTaskRunnerForTesting(std::move(task_runner),
                                             tick_clock);
}

}  // namespace blink
