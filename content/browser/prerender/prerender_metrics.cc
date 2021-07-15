// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/prerender/prerender_metrics.h"

#include "base/metrics/histogram_functions.h"
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

}  // namespace

// Called by MojoBinderPolicyApplier. This function records the Mojo interface
// that causes MojoBinderPolicyApplier to cancel prerendering.
void RecordPrerenderCancelledInterface(const std::string& interface_name) {
  const PrerenderCancelledInterface interface_type =
      GetCancelledInterfaceType(interface_name);
  base::UmaHistogramEnumeration(
      "Prerender.Experimental.PrerenderCancelledInterface", interface_type);
}

void RecordPrerenderTriggered(
    RenderFrameHostImpl& initiator_render_frame_host) {
  ukm::builders::PrerenderPageLoad(
      initiator_render_frame_host.GetPageUkmSourceId())
      .SetTriggeredPrerender(true)
      .Record(ukm::UkmRecorder::Get());
}

}  // namespace content
