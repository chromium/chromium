// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/idleness_detector.h"

#include "base/check.h"
#include "base/time/default_tick_clock.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/timing/first_meaningful_paint_detector.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/instrumentation/resource_coordinator/document_resource_coordinator.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"

namespace blink {

constexpr base::TimeDelta IdlenessDetector::kNetworkQuietWindow;
constexpr base::TimeDelta IdlenessDetector::kNetworkQuietWatchdog;

void IdlenessDetector::Shutdown() {
  Stop();
  local_frame_ = nullptr;
}

void IdlenessDetector::WillCommitLoad() {
  in_network_2_quiet_period_ = false;
  in_network_0_quiet_period_ = false;
  network_2_quiet_ = base::TimeTicks();
  network_0_quiet_ = base::TimeTicks();
  network_2_quiet_start_time_ = base::TimeTicks();
  network_0_quiet_start_time_ = base::TimeTicks();
}

void IdlenessDetector::DomContentLoadedEventFired() {
  Start();
}

void IdlenessDetector::DidDropNavigation() {
  // Only process dropped navigation that occurred if we haven't
  // started yet, that is, not currently active and not finished.
  if (!task_observer_added_ && network_2_quiet_start_time_.is_null() &&
      network_0_quiet_start_time_.is_null()) {
    Start();
  }
}

void IdlenessDetector::Start() {
  if (!local_frame_)
    return;

  if (!task_observer_added_) {
    Thread::Current()->AddTaskTimeObserver(this);
    task_observer_added_ = true;
  }

  in_network_2_quiet_period_ = true;
  in_network_0_quiet_period_ = true;
  network_2_quiet_ = base::TimeTicks();
  network_0_quiet_ = base::TimeTicks();

  OnDidLoadResource();
}

void IdlenessDetector::OnWillSendRequest(ResourceFetcher* fetcher) {
  // If |fetcher| is not the current fetcher of the Document, then that means
  // it's a new navigation, bail out in this case since it shouldn't affect the
  // current idleness of the local frame.
  if (!local_frame_ || fetcher != local_frame_->GetDocument()->Fetcher())
    return;

  // When OnWillSendRequest is called, the new loader hasn't been added to the
  // fetcher, thus we need to add 1 as the total request count.
  int request_count = fetcher->ActiveRequestCount() + 1;
  // If we are above the allowed number of active requests, reset timers.
  if (in_network_2_quiet_period_ && request_count > 2)
    network_2_quiet_ = base::TimeTicks();
  if (in_network_0_quiet_period_ && request_count > 0)
    network_0_quiet_ = base::TimeTicks();
}

// This function is called when the number of active connections is decreased.
// Note that the number of active connections doesn't decrease monotonically.
void IdlenessDetector::OnDidLoadResource() {
  if (!local_frame_)
    return;

  // Document finishes parsing after DomContentLoadedEventEnd is fired,
  // check the status in order to avoid false signals.
  if (!local_frame_->GetDocument()->HasFinishedParsing())
    return;

  // If we already reported quiet time, bail out.
  if (HasCompleted()) {
    return;
  }

  if (local_frame_->Loader().HasProvisionalNavigation()) {
    return;
  }

  int request_count =
      local_frame_->GetDocument()->Fetcher()->ActiveRequestCount();
  // If we did not achieve either 0 or 2 active connections, bail out.
  if (request_count > 2)
    return;

  base::TimeTicks timestamp = clock_->NowTicks();
  // Arriving at =2 updates the quiet_2 base timestamp.
  // Arriving at <2 sets the quiet_2 base timestamp only if
  // it was not already set.
  if (request_count == 2 && in_network_2_quiet_period_) {
    network_2_quiet_ = timestamp;
    network_2_quiet_start_time_ = timestamp;
  } else if (request_count < 2 && in_network_2_quiet_period_ &&
             network_2_quiet_.is_null()) {
    network_2_quiet_ = timestamp;
    network_2_quiet_start_time_ = timestamp;
  }

  if (request_count == 0 && in_network_0_quiet_period_) {
    network_0_quiet_ = timestamp;
    network_0_quiet_start_time_ = timestamp;
  }

  if (!network_quiet_timer_.IsActive()) {
    network_quiet_timer_.StartOneShot(kNetworkQuietWatchdog, FROM_HERE);
  }
}

base::TimeTicks IdlenessDetector::GetNetworkAlmostIdleTime() {
  return network_2_quiet_start_time_;
}

base::TimeTicks IdlenessDetector::GetNetworkIdleTime() {
  return network_0_quiet_start_time_;
}

void IdlenessDetector::WillProcessTask(base::TimeTicks start_time) {
  // If we have idle time and we are network_quiet_window_ seconds past it, emit
  // idle signals.
  DocumentLoader* loader = local_frame_->Loader().GetDocumentLoader();
  if (in_network_2_quiet_period_ && !network_2_quiet_.is_null() &&
      start_time - network_2_quiet_ > network_quiet_window_) {
    probe::LifecycleEvent(
        local_frame_, loader, "networkAlmostIdle",
        network_2_quiet_start_time_.since_origin().InSecondsF());
    DCHECK(local_frame_->GetDocument());
    if (auto* document_resource_coordinator =
            local_frame_->GetDocument()->GetResourceCoordinator()) {
      document_resource_coordinator->SetNetworkAlmostIdle();
    }
    if (WebServiceWorkerNetworkProvider* service_worker_network_provider =
            loader->GetServiceWorkerNetworkProvider()) {
      service_worker_network_provider->DispatchNetworkQuiet();
    }
    FirstMeaningfulPaintDetector::From(*local_frame_->GetDocument())
        .OnNetwork2Quiet();
    in_network_2_quiet_period_ = false;
    network_2_quiet_ = base::TimeTicks();
  }

  if (in_network_0_quiet_period_ && !network_0_quiet_.is_null() &&
      start_time - network_0_quiet_ > network_quiet_window_) {
    probe::LifecycleEvent(
        local_frame_, loader, "networkIdle",
        network_0_quiet_start_time_.since_origin().InSecondsF());
    in_network_0_quiet_period_ = false;
    network_0_quiet_ = base::TimeTicks();
  }

  if (HasCompleted()) {
    Stop();
  }
}

void IdlenessDetector::DidProcessTask(base::TimeTicks start_time,
                                      base::TimeTicks end_time) {
  // Shift idle timestamps with the duration of the task, we were not idle.
  if (in_network_2_quiet_period_ && !network_2_quiet_.is_null())
    network_2_quiet_ += end_time - start_time;
  if (in_network_0_quiet_period_ && !network_0_quiet_.is_null())
    network_0_quiet_ += end_time - start_time;
}

IdlenessDetector::IdlenessDetector(LocalFrame* local_frame,
                                   const base::TickClock* clock)
    : local_frame_(local_frame),
      task_observer_added_(false),
      clock_(clock),
      network_quiet_timer_(
          local_frame->GetTaskRunner(TaskType::kInternalLoading),
          this,
          &IdlenessDetector::NetworkQuietTimerFired) {
  if (local_frame->GetSettings()) {
    network_quiet_window_ =
        base::Seconds(local_frame->GetSettings()->GetNetworkQuietTimeout());
  }
}

void IdlenessDetector::Stop() {
  network_quiet_timer_.Stop();
  if (!task_observer_added_)
    return;
  Thread::Current()->RemoveTaskTimeObserver(this);
  task_observer_added_ = false;
}

void IdlenessDetector::NetworkQuietTimerFired(TimerBase*) {
  // TODO(lpy) Reduce the number of timers.
  if ((in_network_0_quiet_period_ && !network_0_quiet_.is_null()) ||
      (in_network_2_quiet_period_ && !network_2_quiet_.is_null())) {
    network_quiet_timer_.StartOneShot(kNetworkQuietWatchdog, FROM_HERE);
  }
}

void IdlenessDetector::Trace(Visitor* visitor) const {
  visitor->Trace(local_frame_);
  visitor->Trace(network_quiet_timer_);
}

}  // namespace blink
