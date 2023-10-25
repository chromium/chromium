// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/page_animator.h"

#include "base/auto_reset.h"
#include "base/time/time.h"
#include "cc/animation/animation_host.h"
#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/dom/document_lifecycle.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/scripted_animation_controller.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/validation_message_client.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/svg/svg_document_extensions.h"
#include "third_party/blink/renderer/core/view_transition/view_transition.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

namespace {

// We walk through all the frames in DOM tree order and get all the documents
DocumentsVector GetAllDocuments(Frame* main_frame) {
  DocumentsVector documents;
  for (Frame* frame = main_frame; frame; frame = frame->Tree().TraverseNext()) {
    if (auto* local_frame = DynamicTo<LocalFrame>(frame)) {
      Document* document = local_frame->GetDocument();
      bool can_throttle =
          document->View() ? document->View()->CanThrottleRendering() : false;
      documents.emplace_back(std::make_pair(document, can_throttle));
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

  DocumentsVector documents = GetAllDocuments(page_->MainFrame());

  TRACE_EVENT0("blink,rail", "PageAnimator::serviceScriptedAnimations");
  for (const auto& [document, can_throttle] : documents) {
    if (!document->View()) {
      document->GetDocumentAnimations()
          .UpdateAnimationTimingForAnimationFrame();
    } else {
      if (!can_throttle) {
        document->View()->ServiceScrollAnimations(
            monotonic_animation_start_time);
      }
    }
  }
  ControllersVector controllers{};
  for (const auto& document : documents) {
    controllers.emplace_back(document.first->GetScriptedAnimationController(),
                             document.second);
  }
  ServiceScriptedAnimations(monotonic_animation_start_time, controllers);
  page_->GetValidationMessageClient().LayoutOverlay();
}

void PageAnimator::ServiceScriptedAnimations(
    base::TimeTicks monotonic_time_now,
    const ControllersVector& controllers) {
  Vector<wtf_size_t> active_controllers_ids{};
  HeapVector<Member<ScriptedAnimationController>> active_controllers{};
  for (wtf_size_t i = 0; i < controllers.size(); ++i) {
    auto& [controller, can_throttle] = controllers[i];
    if (!controller->GetExecutionContext() ||
        controller->GetExecutionContext()->IsContextFrozenOrPaused()) {
      continue;
    }

    LocalDOMWindow* window = controller->GetWindow();
    auto* loader = window->document()->Loader();
    if (!loader) {
      continue;
    }

    controller->SetCurrentFrameTimeMs(
        window->document()->Timeline().CurrentTimeMilliseconds().value());
    controller->SetCurrentFrameLegacyTimeMs(
        loader->GetTiming()
            .MonotonicTimeToPseudoWallTime(monotonic_time_now)
            .InMillisecondsF());
    if (can_throttle) {
      continue;
    }
    auto* animator = controller->GetPageAnimator();
    if (animator && controller->HasFrameCallback()) {
      animator->SetCurrentFrameHadRaf();
    }
    if (!controller->HasScheduledFrameTasks()) {
      continue;
    }
    active_controllers_ids.emplace_back(i);
    active_controllers.emplace_back(controller);
  }

  Vector<base::TimeDelta> time_intervals(active_controllers.size());
  // TODO(rendering-dev): calls to Now() are expensive on ARM architectures.
  // We can avoid some of these calls by filtering out calls to controllers
  // where the function() invocation won't do any work (e.g., because there
  // are no events to dispatch).
  const auto run_for_all_active_controllers_with_timing =
      [&](const auto& function) {
        auto start_time = base::TimeTicks::Now();
        for (wtf_size_t i = 0; i < active_controllers.size(); ++i) {
          function(i);
          auto end_time = base::TimeTicks::Now();
          time_intervals[i] += end_time - start_time;
          start_time = end_time;
        }
      };

  // https://html.spec.whatwg.org/multipage/webappapis.html#event-loop-processing-model

  // TODO(bokan): Requires an update to "update the rendering" steps in HTML.
  run_for_all_active_controllers_with_timing([&](wtf_size_t i) {
    if (RuntimeEnabledFeatures::PageRevealEventEnabled()) {
      active_controllers[i]->DispatchEvents([](const Event* event) {
        return event->type() == event_type_names::kPagereveal;
      });
    }

    if (RuntimeEnabledFeatures::ViewTransitionOnNavigationEnabled()) {
      CHECK(RuntimeEnabledFeatures::PageRevealEventEnabled());
      if (const LocalDOMWindow* window = active_controllers[i]->GetWindow()) {
        CHECK(window->document());
        if (ViewTransition* transition =
                ViewTransitionUtils::GetTransition(*window->document())) {
          transition->ActivateFromSnapshot();
        }
      }
    }
  });

  // 6. For each fully active Document in docs, flush autofocus
  // candidates for that Document if its browsing context is a top-level
  // browsing context.
  run_for_all_active_controllers_with_timing([&](wtf_size_t i) {
    if (const auto* window = active_controllers[i]->GetWindow()) {
      window->document()->FlushAutofocusCandidates();
    }
  });

  // 7. For each fully active Document in docs, run the resize steps
  // for that Document, passing in now as the timestamp.
  wtf_size_t active_controller_id = 0;
  auto start_time = base::TimeTicks::Now();
  for (wtf_size_t i = 0; i < controllers.size(); ++i) {
    auto& [controller, can_throttle] = controllers[i];
    controller->DispatchEvents([](const Event* event) {
      return event->type() == event_type_names::kResize;
    });
    auto end_time = base::TimeTicks::Now();
    if (active_controller_id < active_controllers_ids.size() &&
        i == active_controllers_ids[active_controller_id]) {
      time_intervals[active_controller_id++] += end_time - start_time;
    } else {
      // For non active controllers (e.g. which can throttle)
      // that's the only timing we need to measure.
      if (const auto* window = controller->GetWindow()) {
        if (auto* frame = window->document()->GetFrame()) {
          frame->GetFrameScheduler()->AddTaskTime(end_time - start_time);
        }
      }
    }
    start_time = end_time;
  }

  // 8. For each fully active Document in docs, run the scroll steps
  // for that Document, passing in now as the timestamp.
  run_for_all_active_controllers_with_timing([&](wtf_size_t i) {
    active_controllers[i]->DispatchEvents([](const Event* event) {
      return event->type() == event_type_names::kScroll ||
             event->type() == event_type_names::kScrollend;
    });
  });

  // 9. For each fully active Document in docs, evaluate media
  // queries and report changes for that Document, passing in now as the
  // timestamp
  run_for_all_active_controllers_with_timing([&](wtf_size_t i) {
    active_controllers[i]->CallMediaQueryListListeners();
  });

  // 10. For each fully active Document in docs, update animations and
  // send events for that Document, passing in now as the timestamp.
  run_for_all_active_controllers_with_timing(
      [&](wtf_size_t i) { active_controllers[i]->DispatchEvents(); });

  // 11. For each fully active Document in docs, run the fullscreen
  // steps for that Document, passing in now as the timestamp.
  run_for_all_active_controllers_with_timing(
      [&](wtf_size_t i) { active_controllers[i]->RunTasks(); });

  // Run the fulfilled HTMLVideoELement.requestVideoFrameCallback() callbacks.
  // See https://wicg.github.io/video-rvfc/.
  run_for_all_active_controllers_with_timing([&](wtf_size_t i) {
    active_controllers[i]->ExecuteVideoFrameCallbacks();
  });

  // 13. For each fully active Document in docs, run the animation
  // frame callbacks for that Document, passing in now as the timestamp.
  run_for_all_active_controllers_with_timing([&](wtf_size_t i) {
    active_controllers[i]->ExecuteFrameCallbacks();
    if (!active_controllers[i]->GetExecutionContext()) {
      return;
    }
    auto* animator = active_controllers[i]->GetPageAnimator();
    if (animator && active_controllers[i]->HasFrameCallback()) {
      animator->SetNextFrameHasPendingRaf();
    }
    // See LocalFrameView::RunPostLifecycleSteps() for 14.
    active_controllers[i]->ScheduleAnimationIfNeeded();
  });

  // Add task timings.
  for (wtf_size_t i = 0; i < active_controllers.size(); ++i) {
    if (const auto* window = active_controllers[i]->GetWindow()) {
      if (auto* frame = window->document()->GetFrame()) {
        frame->GetFrameScheduler()->AddTaskTime(time_intervals[i]);
      }
    }
  }
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
  DocumentsVector documents = GetAllDocuments(page_->MainFrame());
  for (auto& [document, can_throttle] : documents) {
    document->GetDocumentAnimations().GetAnimationsTargetingTreeScope(
        animations, tree_scope);
  }
  return animations;
}

}  // namespace blink
