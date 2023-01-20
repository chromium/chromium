// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/page_animator.h"

#include "base/auto_reset.h"
#include "cc/animation/animation_host.h"
#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/validation_message_client.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

namespace {

typedef HeapVector<Member<Document>, 32> DocumentsVector;

enum OnlyThrottledOrNot { kOnlyNonThrottled, kAllDocuments };

// We walk through all the frames in DOM tree order and get all the documents
DocumentsVector GetAllDocuments(Frame* main_frame,
                                OnlyThrottledOrNot which_documents) {
  DocumentsVector documents;
  for (Frame* frame = main_frame; frame; frame = frame->Tree().TraverseNext()) {
    if (auto* local_frame = DynamicTo<LocalFrame>(frame)) {
      Document* document = local_frame->GetDocument();
      if (which_documents == kAllDocuments || !document->View() ||
          !document->View()->CanThrottleRendering())
        documents.push_back(document);
    }
  }
  return documents;
}

}  // namespace

PageAnimator::PageAnimator(Page& page)
    : page_(page),
      servicing_animations_(false),
      updating_layout_and_style_for_painting_(false) {}

void PageAnimator::Trace(Visitor* visitor) const {
  visitor->Trace(page_);
}

void PageAnimator::ServiceScriptedAnimations(
    base::TimeTicks monotonic_animation_start_time) {
  base::AutoReset<bool> servicing(&servicing_animations_, true);

  // Once we are inside a frame's lifecycle, the AnimationClock should hold its
  // time value until the end of the frame.
  Clock().SetAllowedToDynamicallyUpdateTime(false);
  Clock().UpdateTime(monotonic_animation_start_time);

  DocumentsVector documents =
      GetAllDocuments(page_->MainFrame(), kAllDocuments);

  for (auto& document : documents) {
    // TODO(szager): The following logic evolved piecemeal, and this conditional
    // is suspect.
    if (!document->View() || !document->View()->CanThrottleRendering()) {
      TRACE_EVENT0("blink,rail", "PageAnimator::serviceScriptedAnimations");
    }
    if (!document->View()) {
      document->GetDocumentAnimations()
          .UpdateAnimationTimingForAnimationFrame();
      continue;
    }
    document->View()->ServiceScriptedAnimations(monotonic_animation_start_time);
  }

  page_->GetValidationMessageClient().LayoutOverlay();
}

void PageAnimator::PostAnimate() {
  // If we don't have an imminently incoming frame, we need to let the
  // AnimationClock update its own time to properly service out-of-lifecycle
  // events such as setInterval (see https://crbug.com/995806). This isn't a
  // perfect heuristic, but at the very least we know that if there is a pending
  // RAF we will be getting a new frame and thus don't need to unlock the clock.
  if (!next_frame_has_pending_raf_)
    Clock().SetAllowedToDynamicallyUpdateTime(true);
  next_frame_has_pending_raf_ = false;
}

void PageAnimator::SetHasCanvasInvalidation() {
  has_canvas_invalidation_ = true;
}

void PageAnimator::ReportFrameAnimations(cc::AnimationHost* animation_host) {
  if (animation_host) {
    animation_host->SetHasCanvasInvalidation(has_canvas_invalidation_);
    animation_host->SetHasInlineStyleMutation(has_inline_style_mutation_);
    animation_host->SetHasSmilAnimation(has_smil_animation_);
    animation_host->SetCurrentFrameHadRaf(current_frame_had_raf_);
    animation_host->SetNextFrameHasPendingRaf(next_frame_has_pending_raf_);
    animation_host->SetHasViewTransition(has_view_transition_);
  }
  has_canvas_invalidation_ = false;
  has_inline_style_mutation_ = false;
  has_smil_animation_ = false;
  current_frame_had_raf_ = false;
  // next_frame_has_pending_raf_ is reset at PostAnimate().
  // has_view_transition_ is reset when the transition ends.
}

void PageAnimator::SetSuppressFrameRequestsWorkaroundFor704763Only(
    bool suppress_frame_requests) {
  // If we are enabling the suppression and it was already enabled then we must
  // have missed disabling it at the end of a previous frame.
  DCHECK(!suppress_frame_requests_workaround_for704763_only_ ||
         !suppress_frame_requests);
  suppress_frame_requests_workaround_for704763_only_ = suppress_frame_requests;
}

void PageAnimator::SetHasInlineStyleMutation() {
  has_inline_style_mutation_ = true;
}

void PageAnimator::SetHasSmilAnimation() {
  has_smil_animation_ = true;
}

void PageAnimator::SetCurrentFrameHadRaf() {
  current_frame_had_raf_ = true;
}

void PageAnimator::SetNextFrameHasPendingRaf() {
  next_frame_has_pending_raf_ = true;
}

void PageAnimator::SetHasViewTransition(bool has_view_transition) {
  has_view_transition_ = has_view_transition;
}

DISABLE_CFI_PERF
void PageAnimator::ScheduleVisualUpdate(LocalFrame* frame) {
  if (servicing_animations_ || updating_layout_and_style_for_painting_ ||
      suppress_frame_requests_workaround_for704763_only_) {
    return;
  }
  page_->GetChromeClient().ScheduleAnimation(frame->View());
}

void PageAnimator::UpdateAllLifecyclePhases(LocalFrame& root_frame,
                                            DocumentUpdateReason reason) {
  LocalFrameView* view = root_frame.View();
  base::AutoReset<bool> servicing(&updating_layout_and_style_for_painting_,
                                  true);
  view->UpdateAllLifecyclePhases(reason);
}

void PageAnimator::UpdateLifecycleToPrePaintClean(LocalFrame& root_frame,
                                                  DocumentUpdateReason reason) {
  LocalFrameView* view = root_frame.View();
  base::AutoReset<bool> servicing(&updating_layout_and_style_for_painting_,
                                  true);
  view->UpdateLifecycleToPrePaintClean(reason);
}

void PageAnimator::UpdateLifecycleToLayoutClean(LocalFrame& root_frame,
                                                DocumentUpdateReason reason) {
  LocalFrameView* view = root_frame.View();
  base::AutoReset<bool> servicing(&updating_layout_and_style_for_painting_,
                                  true);
  view->UpdateLifecycleToLayoutClean(reason);
}

HeapVector<Member<Animation>> PageAnimator::GetAnimations(
    const TreeScope& tree_scope) {
  HeapVector<Member<Animation>> animations;
  DocumentsVector documents =
      GetAllDocuments(page_->MainFrame(), kAllDocuments);
  for (auto& document : documents) {
    document->GetDocumentAnimations().GetAnimationsTargetingTreeScope(
        animations, tree_scope);
  }
  return animations;
}

}  // namespace blink
