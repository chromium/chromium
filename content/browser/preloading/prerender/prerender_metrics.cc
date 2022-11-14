// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/prerender_metrics.h"

#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/string_util.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/public/browser/prerender_trigger_type.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace content {

namespace {

// Do not add new value.
// These values are used to persists sparse metrics to logs.
enum HeaderMismatchType : uint32_t {
  kMatch = 0,
  kMissingInPrerendering = 1,
  kMissingInActivation = 2,
  kValueMismatch = 3,
  kMaxValue = kValueMismatch
};

PrerenderCancelledInterface GetCancelledInterfaceType(
    const std::string& interface_name) {
  if (interface_name == "device.mojom.GamepadHapticsManager")
    return PrerenderCancelledInterface::kGamepadHapticsManager;
  else if (interface_name == "device.mojom.GamepadMonitor")
    return PrerenderCancelledInterface::kGamepadMonitor;
  else if (interface_name == "chrome.mojom.SyncEncryptionKeysExtension")
    return PrerenderCancelledInterface::kSyncEncryptionKeysExtension;
  return PrerenderCancelledInterface::kUnknown;
}

int32_t InterfaceNameHasher(const std::string& interface_name) {
  return static_cast<int32_t>(base::HashMetricNameAs32Bits(interface_name));
}

int32_t HeaderMismatchHasher(const std::string& header,
                             HeaderMismatchType mismatch_type) {
  // Throw two bits away to encode the mismatch type.
  // {0---30} bits are the encoded hash number.
  // {31, 32} bits encode the mismatch type.
  static_assert(HeaderMismatchType::kMaxValue == 3u,
                "HeaderMismatchType should use 2 bits at most.");
  return static_cast<int32_t>(base::HashMetricNameAs32Bits(header) << 2 |
                              mismatch_type);
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

void ReportHeaderMismatch(const std::string& key,
                          HeaderMismatchType mismatch_type,
                          PrerenderTriggerType trigger_type,
                          const std::string& embedder_histogram_suffix) {
  base::UmaHistogramSparse(
      GenerateHistogramName("Prerender.Experimental.ActivationHeadersMismatch",
                            trigger_type, embedder_histogram_suffix),
      HeaderMismatchHasher(base::ToLowerASCII(key), mismatch_type));
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

void RecordPrerenderReasonForInactivePageRestriction(uint16_t reason,
                                                     RenderFrameHostImpl& rfh) {
  FrameTreeNode* outermost_frame =
      rfh.GetOutermostMainFrameOrEmbedder()->frame_tree_node();
  PrerenderHost* prerender_host =
      rfh.delegate()->GetPrerenderHostRegistry()->FindNonReservedHostById(
          outermost_frame->frame_tree_node_id());
  if (prerender_host) {
    base::UmaHistogramSparse(
        GenerateHistogramName("Prerender.CanceledForInactivePageRestriction."
                              "DisallowActivationReason",
                              prerender_host->trigger_type(),
                              prerender_host->embedder_histogram_suffix()),
        reason);
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

void RecordPrerenderFinalStatus(PrerenderFinalStatus status,
                                const PrerenderAttributes& attributes,
                                ukm::SourceId prerendered_ukm_id) {
  base::UmaHistogramEnumeration(
      GenerateHistogramName("Prerender.Experimental.PrerenderHostFinalStatus",
                            attributes.trigger_type,
                            attributes.embedder_histogram_suffix),
      status);

  if (attributes.initiator_ukm_id != ukm::kInvalidSourceId) {
    // `initiator_ukm_id` must be valid for the speculation rules.
    DCHECK_EQ(attributes.trigger_type, PrerenderTriggerType::kSpeculationRule);
    ukm::builders::PrerenderPageLoad(attributes.initiator_ukm_id)
        .SetFinalStatus(static_cast<int>(status))
        .Record(ukm::UkmRecorder::Get());
  }

  if (prerendered_ukm_id != ukm::kInvalidSourceId) {
    // `prerendered_ukm_id` must be valid only when the prerendered page gets
    // activated.
    DCHECK_EQ(status, PrerenderFinalStatus::kActivated);
    ukm::builders::PrerenderPageLoad(prerendered_ukm_id)
        .SetFinalStatus(static_cast<int>(status))
        .Record(ukm::UkmRecorder::Get());
  }

  // The kActivated case is recorded in `PrerenderHost::Activate`, and the
  // kMojoBinderPolicy case is recorded in
  // RenderFrameHostImpl::CancelPrerenderingByMojoBinderPolicy for storing the
  // interface detail. Browser initiated prerendering doesn't report
  // cancellation reasons to the DevTools as it doesn't have the initiator frame
  // associated with DevTools agents.
  if (!attributes.IsBrowserInitiated() &&
      status != PrerenderFinalStatus::kActivated &&
      status != PrerenderFinalStatus::kMojoBinderPolicy) {
    auto* ftn = FrameTreeNode::GloballyFindByID(
        attributes.initiator_frame_tree_node_id);
    DCHECK(ftn);
    devtools_instrumentation::DidCancelPrerender(attributes.prerendering_url,
                                                 ftn, status, "");
  }
}

void RecordPrerenderActivationNavigationParamsMatch(
    PrerenderHost::ActivationNavigationParamsMatch result,
    PrerenderTriggerType trigger_type,
    const std::string& embedder_suffix) {
  base::UmaHistogramEnumeration(
      GenerateHistogramName(
          "Prerender.Experimental.ActivationNavigationParamsMatch",
          trigger_type, embedder_suffix),
      result);
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

void AnalyzePrerenderActivationHeader(
    net::HttpRequestHeaders potential_activation_headers,
    net::HttpRequestHeaders prerender_headers,
    PrerenderTriggerType trigger_type,
    const std::string& embedder_histogram_suffix) {
  using HeaderPair = net::HttpRequestHeaders::HeaderKeyValuePair;
  auto potential_header_dict = base::MakeFlatMap<std::string, std::string>(
      potential_activation_headers.GetHeaderVector(), /*comp=default*/ {},
      [](HeaderPair x) {
        return std::make_pair(base::ToLowerASCII(x.key), x.value);
      });

  // Masking code for whether the corresponding enum has been removed or not.
  // Instead of removing an element from flat_map, we set the mask bit as the
  // cost of removal is O(N).
  std::vector<bool> removal_set(potential_header_dict.size(), false);
  bool detected = false;
  for (auto& prerender_header : prerender_headers.GetHeaderVector()) {
    std::string key = base::ToLowerASCII(prerender_header.key);
    auto potential_it = potential_header_dict.find(key);
    if (potential_it == potential_header_dict.end()) {
      // The potential activation headers does not contain it.
      ReportHeaderMismatch(key, HeaderMismatchType::kMissingInActivation,
                           trigger_type, embedder_histogram_suffix);
      detected = true;
      continue;
    }
    if (!base::EqualsCaseInsensitiveASCII(prerender_header.value,
                                          potential_it->second)) {
      ReportHeaderMismatch(key, HeaderMismatchType::kValueMismatch,
                           trigger_type, embedder_histogram_suffix);
      detected = true;
    }

    // Remove it, since we will report the remaining ones, i.e., the headers
    // that are not found in prerendering.
    removal_set.at(potential_it - potential_header_dict.begin()) = true;
  }

  // Iterate over the remaining potential prerendering headers and report it to
  // UMA.
  for (auto potential_it = potential_header_dict.begin();
       potential_it != potential_header_dict.end(); potential_it++) {
    if (removal_set.at(potential_it - potential_header_dict.begin())) {
      continue;
    }
    detected = true;
    ReportHeaderMismatch(potential_it->first,
                         HeaderMismatchType::kMissingInPrerendering,
                         trigger_type, embedder_histogram_suffix);
  }

  // Use the empty string for the matching case; we use this value for detecting
  // bug, that is, comparing strings is wrong.
  if (!detected) {
    ReportHeaderMismatch("", HeaderMismatchType::kMatch, trigger_type,
                         embedder_histogram_suffix);
  }
}

}  // namespace content
