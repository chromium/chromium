// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/page_animator.h"

#include "base/auto_reset.h"
#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/validation_message_client.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/svg/svg_document_extensions.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

PageAnimator::PageAnimator(Page& page)
    : page_(page),
      servicing_animations_(false),
      updating_layout_and_style_for_painting_(false) {}

void PageAnimator::Trace(blink::Visitor* visitor) {
  visitor->Trace(page_);
}

void PageAnimator::ServiceScriptedAnimations(
    base::TimeTicks monotonic_animation_start_time) {
  base::AutoReset<bool> servicing(&servicing_animations_, true);

  // Once we are inside a frame's lifecycle, the AnimationClock should hold its
  // time value until the end of the frame.
  Clock().SetAllowedToDynamicallyUpdateTime(false);
  Clock().UpdateTime(monotonic_animation_start_time);

  HeapVector<Member<Document>, 32> documents;
  for (Frame* frame = page_->MainFrame(); frame;
       frame = frame->Tree().TraverseNext()) {
    if (auto* local_frame = DynamicTo<LocalFrame>(frame))
      documents.push_back(local_frame->GetDocument());
  }

  for (auto& document : documents) {
    ScopedFrameBlamer frame_blamer(document->GetFrame());
    TRACE_EVENT0("blink,rail", "PageAnimator::serviceScriptedAnimations");
    DocumentAnimations::UpdateAnimationTimingForAnimationFrame(*document);
    if (document->View()) {
      if (document->View()->ShouldThrottleRendering()) {
        document->SetCurrentFrameIsThrottled(true);
        continue;
      }
      // Disallow throttling in case any script needs to do a synchronous
      // lifecycle update in other frames which are throttled.
      DocumentLifecycle::DisallowThrottlingScope no_throttling_scope(
          document->Lifecycle());
      if (ScrollableArea* scrollable_area =
              document->View()->GetScrollableArea()) {
        scrollable_area->ServiceScrollAnimations(
            monotonic_animation_start_time.since_origin().InSecondsF());
      }

      if (const LocalFrameView::ScrollableAreaSet* animating_scrollable_areas =
              document->View()->AnimatingScrollableAreas()) {
        // Iterate over a copy, since ScrollableAreas may deregister
        // themselves during the iteration.
        HeapVector<Member<PaintLayerScrollableArea>>
            animating_scrollable_areas_copy;
        CopyToVector(*animating_scrollable_areas,
                     animating_scrollable_areas_copy);
        for (PaintLayerScrollableArea* scrollable_area :
             animating_scrollable_areas_copy) {
          scrollable_area->ServiceScrollAnimations(
              monotonic_animation_start_time.since_origin().InSecondsF());
        }
      }
      document->GetFrame()->AnimateSnapFling(monotonic_animation_start_time);
      SVGDocumentExtensions::ServiceOnAnimationFrame(*document);
    }
    // TODO(skyostil): This function should not run for documents without views.
    DocumentLifecycle::DisallowThrottlingScope no_throttling_scope(
        document->Lifecycle());
    document->ServiceScriptedAnimations(monotonic_animation_start_time);
  }

  page_->GetValidationMessageClient().LayoutOverlay();
}

void PageAnimator::PostAnimate() {
  HeapVector<Member<Document>, 32> documents;
  for (Frame* frame = page_->MainFrame(); frame;
       frame = frame->Tree().TraverseNext()) {
    if (frame->IsLocalFrame())
      documents.push_back(To<LocalFrame>(frame)->GetDocument());
  }

  // Run the post-animation frame callbacks. See
  // https://github.com/WICG/requestPostAnimationFrame
  for (auto& document : documents)
    document->RunPostAnimationFrameCallbacks();

  // If we don't have an imminently incoming frame, we need to let the
  // AnimationClock update its own time to properly service out-of-lifecycle
  // events such as setInterval (see https://crbug.com/995806). This isn't a
  // perfect heuristic, but at the very least we know that if there is a pending
  // RAF we will be getting a new frame and thus don't need to unlock the clock.
  bool next_frame_has_raf = false;
  for (auto& document : documents)
    next_frame_has_raf |= document->NextFrameHasPendingRAF();
  if (!next_frame_has_raf)
    Clock().SetAllowedToDynamicallyUpdateTime(true);
}

void PageAnimator::SetSuppressFrameRequestsWorkaroundFor704763Only(
    bool suppress_frame_requests) {
  // If we are enabling the suppression and it was already enabled then we must
  // have missed disabling it at the end of a previous frame.
  DCHECK(!suppress_frame_requests_workaround_for704763_only_ ||
         !suppress_frame_requests);
  suppress_frame_requests_workaround_for704763_only_ = suppress_frame_requests;
}

DISABLE_CFI_PERF
void PageAnimator::ScheduleVisualUpdate(LocalFrame* frame) {
  if (servicing_animations_ || updating_layout_and_style_for_painting_ ||
      suppress_frame_requests_workaround_for704763_only_) {
    return;
  }
  page_->GetChromeClient().ScheduleAnimation(frame->View());
}

void PageAnimator::UpdateAllLifecyclePhases(
    LocalFrame& root_frame,
    DocumentLifecycle::LifecycleUpdateReason reason) {
  LocalFrameView* view = root_frame.View();
  base::AutoReset<bool> servicing(&updating_layout_and_style_for_painting_,
                                  true);
  view->UpdateAllLifecyclePhases(reason);
}

void PageAnimator::UpdateAllLifecyclePhasesExceptPaint(LocalFrame& root_frame) {
  LocalFrameView* view = root_frame.View();
  base::AutoReset<bool> servicing(&updating_layout_and_style_for_painting_,
                                  true);
  view->UpdateAllLifecyclePhasesExceptPaint();
}

void PageAnimator::UpdateLifecycleToLayoutClean(LocalFrame& root_frame) {
  LocalFrameView* view = root_frame.View();
  base::AutoReset<bool> servicing(&updating_layout_and_style_for_painting_,
                                  true);
  view->UpdateLifecycleToLayoutClean();
}

}  // namespace blink
