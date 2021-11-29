// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/prerender/prerender_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace content {

namespace {

PrerenderCancelledInterface GetCancelledInterfaceType(
    const std::string& interface_name) {
  if (interface_name == "device.mojom.GamepadHapticsManager")
    return PrerenderCancelledInterface::kGamepadHapticsManager;
  else if (interface_name == "device.mojom.GamepadMonitor")
    return PrerenderCancelledInterface::kGamepadMonitor;
  else if (interface_name == "blink.mojom.NotificationService")
    return PrerenderCancelledInterface::kNotificationService;
  return PrerenderCancelledInterface::kUnknown;
}

int32_t InterfaceNameHasher(const std::string& interface_name) {
  return static_cast<int32_t>(base::HashMetricNameAs32Bits(interface_name));
}

}  // namespace

// Called by MojoBinderPolicyApplier. This function records the Mojo interface
// that causes MojoBinderPolicyApplier to cancel prerendering.
void RecordPrerenderCancelledInterface(const std::string& interface_name) {
  const PrerenderCancelledInterface interface_type =
      GetCancelledInterfaceType(interface_name);
  base::UmaHistogramEnumeration(
      "Prerender.Experimental.PrerenderCancelledInterface", interface_type);
  if (interface_type == PrerenderCancelledInterface::kUnknown) {
    // These interfaces can be required by embedders, or not set to kCancel
    // expclitly, e.g., channel-associated interfaces. Record these interfaces
    // with the sparse histogram to ensure all of them are tracked.
    base::UmaHistogramSparse(
        "Prerender.Experimental.PrerenderCancelledUnknownInterface",
        InterfaceNameHasher(interface_name));
  }
}

void RecordPrerenderTriggered(ukm::SourceId ukm_id) {
  ukm::builders::PrerenderPageLoad(ukm_id).SetTriggeredPrerender(true).Record(
      ukm::UkmRecorder::Get());
}

void RecordPrerenderActivationTime(
    base::TimeDelta delta,
    PrerenderTriggerType trigger_type,
    const std::string& embedder_histogram_suffix) {
  switch (trigger_type) {
    case PrerenderTriggerType::kSpeculationRule:
      DCHECK(embedder_histogram_suffix.empty());
      base::UmaHistogramTimes(
          "Navigation.TimeToActivatePrerender.SpeculationRule", delta);
      return;
    case PrerenderTriggerType::kEmbedder:
      DCHECK(!embedder_histogram_suffix.empty());
      base::UmaHistogramTimes("Navigation.TimeToActivatePrerender.Embedder" +
                                  embedder_histogram_suffix,
                              delta);
      return;
  }
  NOTREACHED();
}

}  // namespace content
