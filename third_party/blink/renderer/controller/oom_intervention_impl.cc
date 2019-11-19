// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/oom_intervention_impl.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/debug/crash_logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_for_context_dispose.h"
#include "third_party/blink/renderer/controller/crash_memory_metrics_reporter_impl.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"

namespace blink {

namespace {
enum class OomInterventionState {
  // Initial value for a variable.
  None,
  // Before the intervention has been triggered.
  Before,
  // While the intervention is active.
  During,
  // After the intervention has triggered at least once.
  After
};
void UpdateStateCrashKey(OomInterventionState next_state) {
  static OomInterventionState current_state = OomInterventionState::None;
  // Once an intervention is trigger, the state shall never go back to the
  // Before state.
  if (next_state == OomInterventionState::Before &&
      current_state != OomInterventionState::None)
    return;
  if (current_state == next_state)
    return;
  current_state = next_state;
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "oom_intervention_state", base::debug::CrashKeySize::Size32);
  switch (current_state) {
    case OomInterventionState::None:
      base::debug::SetCrashKeyString(crash_key, "none");
      break;
    case OomInterventionState::Before:
      base::debug::SetCrashKeyString(crash_key, "before");
      break;
    case OomInterventionState::During:
      base::debug::SetCrashKeyString(crash_key, "during");
      break;
    case OomInterventionState::After:
      base::debug::SetCrashKeyString(crash_key, "after");
      break;
  }
}
}  // namespace

// static
void OomInterventionImpl::Create(
    mojo::PendingReceiver<mojom::blink::OomIntervention> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<OomInterventionImpl>(),
                              std::move(receiver));
}

OomInterventionImpl::OomInterventionImpl()
    : delayed_report_timer_(Thread::MainThread()->GetTaskRunner(),
                            this,
                            &OomInterventionImpl::TimerFiredUMAReport) {
  UpdateStateCrashKey(OomInterventionState::Before);
}

OomInterventionImpl::~OomInterventionImpl() {
  UpdateStateCrashKey(OomInterventionState::After);
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

  // Report memory stats every second to send UMA.
  ReportMemoryStats(current_memory);

  if (oom_detected) {
    UpdateStateCrashKey(OomInterventionState::During);

    UMA_HISTOGRAM_MEMORY_MB(
        "Memory.Experimental.OomIntervention.V8UsageBefore",
        base::saturated_cast<int>(usage.v8_bytes / 1024 / 1024));

    if (navigate_ads_enabled_ || purge_v8_memory_enabled_) {
      for (const auto& page : Page::OrdinaryPages()) {
        for (Frame* frame = page->MainFrame(); frame;
             frame = frame->Tree().TraverseNext()) {
          auto* local_frame = DynamicTo<LocalFrame>(frame);
          if (!local_frame)
            continue;
          if (navigate_ads_enabled_)
            local_frame->GetDocument()->NavigateLocalAdsFrames();
          if (purge_v8_memory_enabled_)
            local_frame->ForciblyPurgeV8Memory();
        }
      }
    }

    if (renderer_pause_enabled_) {
      // The ScopedPagePauser is destroyed when the intervention is declined and
      // mojo strong binding is disconnected.
      pauser_.reset(new ScopedPagePauser);
    }

    host_->OnHighMemoryUsage();
    MemoryUsageMonitorInstance().RemoveObserver(this);
    // Send memory pressure notification to trigger GC.
    Thread::MainThread()->GetTaskRunner()->PostTask(FROM_HERE,
                                                    base::BindOnce(&TriggerGC));
    // Notify V8GCForContextDispose that page navigation gc is needed when
    // intervention runs, as it indicates that memory usage is high.
    V8GCForContextDispose::Instance().SetForcePageNavigationGC();

    // Report the memory impact of intervention after 10, 20, 30 seconds.
    metrics_at_intervention_ = current_memory;
    number_of_report_needed_ = 3;
    delayed_report_timer_.StartRepeating(base::TimeDelta::FromSeconds(10),
                                         FROM_HERE);
  }
}

void OomInterventionImpl::ReportMemoryStats(
    OomInterventionMetrics& current_memory) {
  UMA_HISTOGRAM_MEMORY_MB(
      "Memory.Experimental.OomIntervention.RendererBlinkUsage",
      base::saturated_cast<base::Histogram::Sample>(
          current_memory.current_blink_usage_kb / 1024));
  UMA_HISTOGRAM_MEMORY_LARGE_MB(
      "Memory.Experimental.OomIntervention."
      "RendererPrivateMemoryFootprint",
      base::saturated_cast<base::Histogram::Sample>(
          current_memory.current_private_footprint_kb / 1024));
  UMA_HISTOGRAM_MEMORY_MB(
      "Memory.Experimental.OomIntervention.RendererSwapFootprint",
      base::saturated_cast<base::Histogram::Sample>(
          current_memory.current_swap_kb / 1024));
  UMA_HISTOGRAM_MEMORY_LARGE_MB(
      "Memory.Experimental.OomIntervention.RendererVmSize",
      base::saturated_cast<base::Histogram::Sample>(
          current_memory.current_vm_size_kb / 1024));
}

int ToMemoryUsageDeltaSample(uint64_t after_kb, uint64_t before_kb) {
  int delta_mb = (base::saturated_cast<int>(before_kb) -
                  base::saturated_cast<int>(after_kb)) /
                 1024;
  return std::min(std::max(delta_mb, -500), 500);
}

void OomInterventionImpl::TimerFiredUMAReport(TimerBase*) {
  MemoryUsage usage = MemoryUsageMonitorInstance().GetCurrentMemoryUsage();
  OomInterventionMetrics current_memory =
      CrashMemoryMetricsReporterImpl::MemoryUsageToMetrics(usage);
  int blink_usage_delta =
      ToMemoryUsageDeltaSample(current_memory.current_blink_usage_kb,
                               metrics_at_intervention_.current_blink_usage_kb);
  int private_footprint_delta = ToMemoryUsageDeltaSample(
      current_memory.current_private_footprint_kb,
      metrics_at_intervention_.current_private_footprint_kb);
  int v8_usage_mb = base::saturated_cast<int>(usage.v8_bytes / 1024 / 1024);
  switch (number_of_report_needed_--) {
    case 3:
      UMA_HISTOGRAM_MEMORY_MB(
          "Memory.Experimental.OomIntervention.V8UsageAfter10secs",
          v8_usage_mb);
      base::UmaHistogramSparse(
          "Memory.Experimental.OomIntervention.ReducedBlinkUsageAfter10secs2",
          blink_usage_delta);
      base::UmaHistogramSparse(
          "Memory.Experimental.OomIntervention.ReducedRendererPMFAfter10secs2",
          private_footprint_delta);
      break;
    case 2:
      UMA_HISTOGRAM_MEMORY_MB(
          "Memory.Experimental.OomIntervention.V8UsageAfter20secs",
          v8_usage_mb);
      base::UmaHistogramSparse(
          "Memory.Experimental.OomIntervention.ReducedBlinkUsageAfter20secs2",
          blink_usage_delta);
      base::UmaHistogramSparse(
          "Memory.Experimental.OomIntervention.ReducedRendererPMFAfter20secs2",
          private_footprint_delta);
      break;
    case 1:
      UMA_HISTOGRAM_MEMORY_MB(
          "Memory.Experimental.OomIntervention.V8UsageAfter30secs",
          v8_usage_mb);
      base::UmaHistogramSparse(
          "Memory.Experimental.OomIntervention.ReducedBlinkUsageAfter30secs2",
          blink_usage_delta);
      base::UmaHistogramSparse(
          "Memory.Experimental.OomIntervention.ReducedRendererPMFAfter30secs2",
          private_footprint_delta);
      delayed_report_timer_.Stop();
      break;
  }
}

void OomInterventionImpl::TriggerGC() {
  V8PerIsolateData::MainThreadIsolate()->MemoryPressureNotification(
      v8::MemoryPressureLevel::kCritical);
}

}  // namespace blink
