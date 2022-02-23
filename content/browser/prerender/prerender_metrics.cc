// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/prerender/prerender_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "content/public/browser/prerender_trigger_type.h"
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
  else if (interface_name == "chrome.mojom.SyncEncryptionKeysExtension")
    return PrerenderCancelledInterface::kSyncEncryptionKeysExtension;
  return PrerenderCancelledInterface::kUnknown;
}

int32_t InterfaceNameHasher(const std::string& interface_name) {
  return static_cast<int32_t>(base::HashMetricNameAs32Bits(interface_name));
}

std::string GenerateHistogramName(const std::string& histogram_base_name,
                                  PrerenderTriggerType trigger_type,
                                  const std::string& embedder_suffix) {
  switch (trigger_type) {
    case PrerenderTriggerType::kSpeculationRule:
      DCHECK(embedder_suffix.empty());
      return std::string(histogram_base_name) + ".SpeculationRule";
    case PrerenderTriggerType::kEmbedder:
      DCHECK(!embedder_suffix.empty());
      return std::string(histogram_base_name) + ".Embedder_" + embedder_suffix;
  }
  NOTREACHED();
}

}  // namespace

// Called by MojoBinderPolicyApplier. This function records the Mojo interface
// that causes MojoBinderPolicyApplier to cancel prerendering.
void RecordPrerenderCancelledInterface(
    const std::string& interface_name,
    PrerenderTriggerType trigger_type,
    const std::string& embedder_histogram_suffix) {
  const PrerenderCancelledInterface interface_type =
      GetCancelledInterfaceType(interface_name);
  base::UmaHistogramEnumeration(
      GenerateHistogramName(
          "Prerender.Experimental.PrerenderCancelledInterface", trigger_type,
          embedder_histogram_suffix),
      interface_type);
  if (interface_type == PrerenderCancelledInterface::kUnknown) {
    // These interfaces can be required by embedders, or not set to kCancel
    // expclitly, e.g., channel-associated interfaces. Record these interfaces
    // with the sparse histogram to ensure all of them are tracked.
    base::UmaHistogramSparse(
        GenerateHistogramName(
            "Prerender.Experimental.PrerenderCancelledUnknownInterface",
            trigger_type, embedder_histogram_suffix),
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
  base::UmaHistogramTimes(
      GenerateHistogramName("Navigation.TimeToActivatePrerender", trigger_type,
                            embedder_histogram_suffix),
      delta);
}

void RecordPrerenderHostFinalStatus(
    PrerenderHost::FinalStatus status,
    PrerenderTriggerType trigger_type,
    const std::string& embedder_histogram_suffix,
    ukm::SourceId initiator_ukm_id,
    ukm::SourceId prerendered_ukm_id) {
  base::UmaHistogramEnumeration(
      GenerateHistogramName("Prerender.Experimental.PrerenderHostFinalStatus",
                            trigger_type, embedder_histogram_suffix),
      status);

  if (initiator_ukm_id != ukm::kInvalidSourceId) {
    // `initiator_ukm_id` must be valid for the speculation rules.
    DCHECK_EQ(trigger_type, PrerenderTriggerType::kSpeculationRule);
    ukm::builders::PrerenderPageLoad(initiator_ukm_id)
        .SetFinalStatus(static_cast<int>(status))
        .Record(ukm::UkmRecorder::Get());
  }

  if (prerendered_ukm_id != ukm::kInvalidSourceId) {
    // `prerendered_ukm_id` must be valid only when the prerendered page gets
    // activated.
    DCHECK_EQ(status, PrerenderHost::FinalStatus::kActivated);
    ukm::builders::PrerenderPageLoad(prerendered_ukm_id)
        .SetFinalStatus(static_cast<int>(status))
        .Record(ukm::UkmRecorder::Get());
  }
}

void RecordPrerenderRedirectionMismatchType(
    PrerenderCrossOriginRedirectionMismatch mismatch_type,
    PrerenderTriggerType trigger_type,
    const std::string& embedder_histogram_suffix) {
  DCHECK_EQ(trigger_type, PrerenderTriggerType::kEmbedder);
  base::UmaHistogramEnumeration(
      GenerateHistogramName(
          "Prerender.Experimental.PrerenderCrossOriginRedirectionMismatch",
          trigger_type, embedder_histogram_suffix),
      mismatch_type);
}

void RecordPrerenderRedirectionProtocolChange(
    PrerenderCrossOriginRedirectionProtocolChange change_type,
    PrerenderTriggerType trigger_type,
    const std::string& embedder_histogram_suffix) {
  DCHECK_EQ(trigger_type, PrerenderTriggerType::kEmbedder);
  base::UmaHistogramEnumeration(
      GenerateHistogramName(
          "Prerender.Experimental.CrossOriginRedirectionProtocolChange",
          trigger_type, embedder_histogram_suffix),
      change_type);
}

void RecordPrerenderRedirectionDomain(
    PrerenderCrossOriginRedirectionDomain domain_type,
    PrerenderTriggerType trigger_type,
    const std::string& embedder_histogram_suffix) {
  DCHECK_EQ(trigger_type, PrerenderTriggerType::kEmbedder);
  base::UmaHistogramEnumeration(
      GenerateHistogramName(
          "Prerender.Experimental.CrossOriginRedirectionDomain", trigger_type,
          embedder_histogram_suffix),
      domain_type);
}

}  // namespace content
