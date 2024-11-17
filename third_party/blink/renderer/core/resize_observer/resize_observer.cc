// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/resize_observer/resize_observer.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_resize_observer_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_resize_observer_options.h"
#include "third_party/blink/renderer/core/core_probes_inl.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observation.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_controller.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_entry.h"

namespace blink {

ResizeObserver* ResizeObserver::Create(ScriptState* script_state,
                                       V8ResizeObserverCallback* callback) {
  return MakeGarbageCollected<ResizeObserver>(
      callback, LocalDOMWindow::From(script_state));
}

ResizeObserver* ResizeObserver::Create(LocalDOMWindow* window,
                                       Delegate* delegate) {
  return MakeGarbageCollected<ResizeObserver>(delegate, window);
}

ResizeObserver::ResizeObserver(V8ResizeObserverCallback* callback,
                               LocalDOMWindow* window)
    : ActiveScriptWrappable<ResizeObserver>({}),
      ExecutionContextClient(window),
      callback_(callback),
      skipped_observations_(false) {
  DCHECK(callback_);
  if (window) {
    controller_ = ResizeObserverController::From(*window);
    controller_->AddObserver(*this);
  }
}

ResizeObserver::ResizeObserver(Delegate* delegate, LocalDOMWindow* window)
    : ActiveScriptWrappable<ResizeObserver>({}),
      ExecutionContextClient(window),
      delegate_(delegate),
      skipped_observations_(false) {
  DCHECK(delegate_);
  if (window) {
    controller_ = ResizeObserverController::From(*window);
    controller_->AddObserver(*this);
  }
}

ResizeObserverBoxOptions ResizeObserver::V8EnumToBoxOptions(
    V8ResizeObserverBoxOptions::Enum box_options) {
  switch (box_options) {
    case V8ResizeObserverBoxOptions::Enum::kBorderBox:
      return ResizeObserverBoxOptions::kBorderBox;
    case V8ResizeObserverBoxOptions::Enum::kContentBox:
      return ResizeObserverBoxOptions::kContentBox;
    case V8ResizeObserverBoxOptions::Enum::kDevicePixelContentBox:
      return ResizeObserverBoxOptions::kDevicePixelContentBox;
  }
  NOTREACHED();
}

void ResizeObserver::observeInternal(Element* target,
                                     ResizeObserverBoxOptions box_option) {
  auto& observer_map = target->EnsureResizeObserverData();

  if (observer_map.Contains(this)) {
    auto observation = observer_map.find(this);
    if ((*observation).value->ObservedBox() == box_option)
      return;

    // Unobserve target if box_option has changed and target already existed. If
    // there is an existing observation of a different box, this new observation
    // takes precedence. See:
    // https://drafts.csswg.org/resize-observer/#processing-model
    observations_.erase((*observation).value);
    auto index = active_observations_.Find((*observation).value);
    if (index != kNotFound) {
      active_observations_.EraseAt(index);
    }
    observer_map.erase(observation);
  }

  auto* observation =
      MakeGarbageCollected<ResizeObservation>(target, this, box_option);
  observations_.insert(observation);
  observer_map.Set(this, observation);

  if (LocalFrameView* frame_view = target->GetDocument().View())
    frame_view->ScheduleAnimation();
}

void ResizeObserver::observe(Element* target,
                             const ResizeObserverOptions* options) {
  ResizeObserverBoxOptions box_option =
      V8EnumToBoxOptions(options->box().AsEnum());
  observeInternal(target, box_option);
}

void ResizeObserver::observe(Element* target) {
  observeInternal(target, ResizeObserverBoxOptions::kContentBox);
}

void ResizeObserver::unobserve(Element* target) {
  auto* observer_map = target ? target->ResizeObserverData() : nullptr;
  if (!observer_map)
    return;
  auto observation = observer_map->find(this);
  if (observation != observer_map->end()) {
    observations_.erase((*observation).value);
    auto index = active_observations_.Find((*observation).value);
    if (index != kNotFound) {
      active_observations_.EraseAt(index);
    }
    observer_map->erase(observation);
  }
}

void ResizeObserver::disconnect() {
  ObservationList observations;
  observations_.Swap(observations);

  for (auto& observation : observations) {
    Element* target = (*observation).Target();
    if (target)
      target->EnsureResizeObserverData().erase(this);
  }
  ClearObservations();
}

size_t ResizeObserver::GatherObservations(size_t deeper_than) {
  DCHECK(active_observations_.empty());

  size_t min_observed_depth = ResizeObserverController::kDepthBottom;
  for (auto& observation : observations_) {
    if (!observation->ObservationSizeOutOfSync())
      continue;
    auto depth = observation->TargetDepth();
    if (depth > deeper_than) {
      active_observations_.push_back(*observation);
      min_observed_depth = std::min(min_observed_depth, depth);
    } else {
      skipped_observations_ = true;
    }
  }
  return min_observed_depth;
}

void ResizeObserver::DeliverObservations() {
  if (active_observations_.empty())
    return;

  HeapVector<Member<ResizeObserverEntry>> entries;

  for (auto& observation : active_observations_) {
    // In case that the observer and the target belong to different execution
    // contexts and the target's execution context is already gone, then skip
    // such a target.
    Element* target = observation->Target();
    if (!target)
      continue;
    ExecutionContext* execution_context = target->GetExecutionContext();
    if (!execution_context || execution_context->IsContextDestroyed())
      continue;

    observation->SetObservationSize(observation->ComputeTargetSize());
    auto* entry =
        MakeGarbageCollected<ResizeObserverEntry>(observation->Target());
    entries.push_back(entry);
  }

  if (entries.size() == 0) {
    // No entry to report.
    // Note that, if |active_observations_| is not empty but |entries| is empty,
    // it means that it's possible that no target element is making |callback_|
    // alive. In this case, we must not touch |callback_|.
    ClearObservations();
    return;
  }

  DCHECK(callback_ || delegate_);
  if (callback_) {
    callback_->InvokeAndReportException(this, entries, this);
  }
  if (delegate_)
    delegate_->OnResize(entries);
  ClearObservations();
}

void ResizeObserver::ClearObservations() {
  active_observations_.clear();
  skipped_observations_ = false;
}

bool ResizeObserver::HasPendingActivity() const {
  return !active_observations_.empty();
}

void ResizeObserver::Trace(Visitor* visitor) const {
  visitor->Trace(callback_);
  visitor->Trace(delegate_);
  visitor->Trace(observations_);
  visitor->Trace(active_observations_);
  visitor->Trace(controller_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
