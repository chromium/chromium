// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/oom_intervention_impl.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/debug/crash_logging.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_for_context_dispose.h"
#include "third_party/blink/renderer/controller/crash_memory_metrics_reporter_impl.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

base::debug::CrashKeyString* GetStateCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "oom_intervention_state", base::debug::CrashKeySize::Size32);
  return crash_key;
}

void NavigateLocalAdsFrames(LocalFrame* frame) {
  // This navigates all the frames detected as an advertisement to about:blank.
  DCHECK(frame);
  for (Frame* child = frame->Tree().FirstChild(); child;
       child = child->Tree().TraverseNext(frame)) {
    if (auto* child_local_frame = DynamicTo<LocalFrame>(child)) {
      if (child_local_frame->IsAdFrame()) {
        FrameLoadRequest request(frame->DomWindow(),
                                 ResourceRequest(BlankURL()));
        child_local_frame->Navigate(request, WebFrameLoadType::kStandard);
      }
    }
    // TODO(yuzus): Once AdsTracker for remote frames is implemented and OOPIF
    // is enabled on low-end devices, navigate remote ads as well.
  }
}

}  // namespace

// static
void OomInterventionImpl::BindReceiver(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    mojo::PendingReceiver<mojom::blink::OomIntervention> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<OomInterventionImpl>(
          base::PassKey<OomInterventionImpl>(), task_runner),
      std::move(receiver), task_runner);
}

OomInterventionImpl::OomInterventionImpl(
    base::PassKey<OomInterventionImpl> pass_key,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : OomInterventionImpl(std::move(task_runner)) {}

OomInterventionImpl::OomInterventionImpl(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {
  static bool initial_crash_key_set = false;
  if (!initial_crash_key_set) {
    initial_crash_key_set = true;
    base::debug::SetCrashKeyString(GetStateCrashKey(), "before");
  }
}

OomInterventionImpl::~OomInterventionImpl() {
  MemoryUsageMonitorInstance().RemoveObserver(this);
}

void OomInterventionImpl::StartDetection(
    mojo::PendingRemote<mojom::blink::OomInterventionHost> host,
    mojom::blink::DetectionArgsPtr detection_args,
    bool renderer_pause_enabled,
    bool navigate_ads_enabled,
    bool purge_v8_memory_enabled) {
  host_.Bind(std::move(host));

  detection_args_ = std::move(detection_args);
  renderer_pause_enabled_ = renderer_pause_enabled;
  navigate_ads_enabled_ = navigate_ads_enabled;
  purge_v8_memory_enabled_ = purge_v8_memory_enabled;

  MemoryUsageMonitorInstance().AddObserver(this);
}

MemoryUsageMonitor& OomInterventionImpl::MemoryUsageMonitorInstance() {
  return MemoryUsageMonitor::Instance();
}

void OomInterventionImpl::OnMemoryPing(MemoryUsage usage) {
  // Ignore pings without process memory usage information.
  if (std::isnan(usage.private_footprint_bytes) ||
      std::isnan(usage.swap_bytes) || std::isnan(usage.vm_size_bytes))
    return;
  Check(usage);
}

void OomInterventionImpl::Check(MemoryUsage usage) {
  DCHECK(host_);

  OomInterventionMetrics current_memory =
      CrashMemoryMetricsReporterImpl::MemoryUsageToMetrics(usage);

  bool oom_detected = false;

  oom_detected |= detection_args_->blink_workload_threshold > 0 &&
                  current_memory.current_blink_usage_kb * 1024 >
                      detection_args_->blink_workload_threshold;
  oom_detected |= detection_args_->private_footprint_threshold > 0 &&
                  current_memory.current_private_footprint_kb * 1024 >
                      detection_args_->private_footprint_threshold;
  oom_detected |=
      detection_args_->swap_threshold > 0 &&
      current_memory.current_swap_kb * 1024 > detection_args_->swap_threshold;
  oom_detected |= detection_args_->virtual_memory_thresold > 0 &&
                  current_memory.current_vm_size_kb * 1024 >
                      detection_args_->virtual_memory_thresold;

  if (oom_detected) {
    base::debug::SetCrashKeyString(GetStateCrashKey(), "during");

    if (navigate_ads_enabled_ || purge_v8_memory_enabled_) {
      for (const auto& page : Page::OrdinaryPages()) {
        for (Frame* frame = page->MainFrame(); frame;
             frame = frame->Tree().TraverseNext()) {
          auto* local_frame = DynamicTo<LocalFrame>(frame);
          if (!local_frame)
            continue;
          if (navigate_ads_enabled_)
            NavigateLocalAdsFrames(local_frame);
          if (purge_v8_memory_enabled_)
            local_frame->ForciblyPurgeV8Memory();
        }
      }
    }

    if (renderer_pause_enabled_) {
      // The ScopedPagePauser is destroyed when the intervention is declined and
      // mojo strong binding is disconnected.
      pauser_ = std::make_unique<ScopedPagePauser>();
    }

    host_->OnHighMemoryUsage();
    MemoryUsageMonitorInstance().RemoveObserver(this);
    // Send memory pressure notification to trigger GC.
    task_runner_->PostTask(FROM_HERE, WTF::BindOnce(&TriggerGC));
    // Notify V8GCForContextDispose that page navigation gc is needed when
    // intervention runs, as it indicates that memory usage is high.
    V8GCForContextDispose::Instance().SetForcePageNavigationGC();
  }
}

void OomInterventionImpl::TriggerGC() {
  Thread::MainThread()
      ->Scheduler()
      ->ToMainThreadScheduler()
      ->ForEachMainThreadIsolate(WTF::BindRepeating([](v8::Isolate* isolate) {
        isolate->MemoryPressureNotification(v8::MemoryPressureLevel::kCritical);
      }));
}

}  // namespace blink
