// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/prerender_metrics.h"

#include <cmath>

#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/string_util.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/preloading/prerender/prerender_trigger_type_impl.h"
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
  else if (interface_name ==
           "chrome.mojom.TrustedVaultEncryptionKeysExtension") {
    return PrerenderCancelledInterface::kTrustedVaultEncryptionKeys;
  }
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
      CHECK(embedder_suffix.empty());
      return std::string(histogram_base_name) + ".SpeculationRule";
    case PrerenderTriggerType::kSpeculationRuleFromIsolatedWorld:
      CHECK(embedder_suffix.empty());
      return std::string(histogram_base_name) +
             ".SpeculationRuleFromIsolatedWorld";
    case PrerenderTriggerType::kEmbedder:
      CHECK(!embedder_suffix.empty());
      return std::string(histogram_base_name) + ".Embedder_" + embedder_suffix;
  }
  NOTREACHED_NORETURN();
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

void RecordPrerenderFinalStatusUma(
    PrerenderFinalStatus final_status,
    PrerenderTriggerType trigger_type,
    const std::string& embedder_histogram_suffix) {
  base::UmaHistogramEnumeration(
      GenerateHistogramName("Prerender.Experimental.PrerenderHostFinalStatus",
                            trigger_type, embedder_histogram_suffix),
      final_status);
}

void RecordDidFailLoadErrorType(int32_t error_code,
                                PrerenderTriggerType trigger_type,
                                const std::string& embedder_histogram_suffix) {
  base::UmaHistogramSparse(
      GenerateHistogramName(
          "Prerender.Experimental.PrerenderLoadingFailureError", trigger_type,
          embedder_histogram_suffix),
      std::abs(error_code));
}

}  // namespace

// static
PrerenderCancellationReason
PrerenderCancellationReason::BuildForDisallowActivationState(
    uint64_t disallow_activation_reason) {
  return PrerenderCancellationReason(
      PrerenderFinalStatus::kInactivePageRestriction,
      disallow_activation_reason);
}

// static
PrerenderCancellationReason
PrerenderCancellationReason::BuildForMojoBinderPolicy(
    const std::string& interface_name) {
  return PrerenderCancellationReason(PrerenderFinalStatus::kMojoBinderPolicy,
                                     interface_name);
}

//  static
PrerenderCancellationReason PrerenderCancellationReason::BuildForLoadingError(
    int32_t error_code) {
  return PrerenderCancellationReason(PrerenderFinalStatus::kDidFailLoad,
                                     error_code);
}

PrerenderCancellationReason::PrerenderCancellationReason(
    PrerenderFinalStatus final_status)
    : PrerenderCancellationReason(final_status, DetailedReasonVariant()) {}

PrerenderCancellationReason::PrerenderCancellationReason(
    PrerenderCancellationReason&& reason) = default;

PrerenderCancellationReason::~PrerenderCancellationReason() = default;

PrerenderCancellationReason::PrerenderCancellationReason(
    PrerenderFinalStatus final_status,
    DetailedReasonVariant explanation)
    : final_status_(final_status), explanation_(std::move(explanation)) {}

void PrerenderCancellationReason::ReportMetrics(
    PrerenderTriggerType trigger_type,
    const std::string& embedder_histogram_suffix) const {
  switch (final_status_) {
    case PrerenderFinalStatus::kInactivePageRestriction:
      CHECK(absl::holds_alternative<uint64_t>(explanation_));
      base::UmaHistogramSparse(
          GenerateHistogramName("Prerender.CanceledForInactivePageRestriction."
                                "DisallowActivationReason",
                                trigger_type, embedder_histogram_suffix),

          absl::get<uint64_t>(explanation_));
      break;
    case PrerenderFinalStatus::kMojoBinderPolicy:
      CHECK(absl::holds_alternative<std::string>(explanation_));
      RecordPrerenderCancelledInterface(absl::get<std::string>(explanation_),
                                        trigger_type,
                                        embedder_histogram_suffix);
      break;
    case PrerenderFinalStatus::kDidFailLoad:
      CHECK(absl::holds_alternative<int32_t>(explanation_));
      RecordDidFailLoadErrorType(absl::get<int32_t>(explanation_), trigger_type,
                                 embedder_histogram_suffix);
      break;
    default:
      CHECK(absl::holds_alternative<absl::monostate>(explanation_));
      // Other types need not to report.
      break;
  }
}

std::string PrerenderCancellationReason::ToDevtoolReasonString() const {
  switch (final_status_) {
    case PrerenderFinalStatus::kInactivePageRestriction:
      CHECK(absl::holds_alternative<uint64_t>(explanation_));
      // TODO(https://crbug.com/1328365): It seems we have to return an integer.
      // And devtool has to handle it based on the enum.xml, as the content
      // layer cannot know about the enums added by the embedder layer.
      return std::string();
    case PrerenderFinalStatus::kMojoBinderPolicy:
      CHECK(absl::holds_alternative<std::string>(explanation_));
      return absl::get<std::string>(explanation_);
    case PrerenderFinalStatus::kDidFailLoad:
      CHECK(absl::holds_alternative<int32_t>(explanation_));
      return std::string();
    default:
      CHECK(absl::holds_alternative<absl::monostate>(explanation_));
      return std::string();
  }
}

absl::optional<std::string>
PrerenderCancellationReason::DisallowedMojoInterface() const {
  switch (final_status_) {
    case PrerenderFinalStatus::kMojoBinderPolicy:
      return absl::get<std::string>(explanation_);
    default:
      return absl::nullopt;
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

void RecordFailedPrerenderFinalStatus(
    const PrerenderCancellationReason& cancellation_reason,
    const PrerenderAttributes& attributes) {
  CHECK_NE(cancellation_reason.final_status(),
           PrerenderFinalStatus::kActivated);
  RecordPrerenderFinalStatusUma(cancellation_reason.final_status(),
                                attributes.trigger_type,
                                attributes.embedder_histogram_suffix);

  if (attributes.initiator_ukm_id != ukm::kInvalidSourceId) {
    // `initiator_ukm_id` must be valid for the speculation rules.
    CHECK(IsSpeculationRuleType(attributes.trigger_type));
    ukm::builders::PrerenderPageLoad(attributes.initiator_ukm_id)
        .SetFinalStatus(static_cast<int>(cancellation_reason.final_status()))
        .Record(ukm::UkmRecorder::Get());
  }

  // Browser initiated prerendering doesn't report cancellation reasons to the
  // DevTools as it doesn't have the initiator frame associated with DevTools
  // agents.
  if (!attributes.IsBrowserInitiated()) {
    auto* ftn = FrameTreeNode::GloballyFindByID(
        attributes.initiator_frame_tree_node_id);
    CHECK(ftn);
    // TODO(https://crbug.com/1332377): Discuss with devtools to finalize the
    // message protocol.
    if (attributes.initiator_devtools_navigation_token.has_value()) {
      devtools_instrumentation::DidCancelPrerender(
          ftn, attributes.prerendering_url,
          attributes.initiator_devtools_navigation_token.value(),
          cancellation_reason.final_status(),
          cancellation_reason.ToDevtoolReasonString());
    }
  }
}

void ReportSuccessActivation(const PrerenderAttributes& attributes,
                             ukm::SourceId prerendered_ukm_id) {
  RecordPrerenderFinalStatusUma(PrerenderFinalStatus::kActivated,
                                attributes.trigger_type,
                                attributes.embedder_histogram_suffix);
  if (attributes.initiator_ukm_id != ukm::kInvalidSourceId) {
    // `initiator_ukm_id` must be valid only for the speculation rules.
    CHECK(IsSpeculationRuleType(attributes.trigger_type));
    ukm::builders::PrerenderPageLoad(attributes.initiator_ukm_id)
        .SetFinalStatus(static_cast<int>(PrerenderFinalStatus::kActivated))
        .Record(ukm::UkmRecorder::Get());
  }

  if (prerendered_ukm_id != ukm::kInvalidSourceId) {
    ukm::builders::PrerenderPageLoad(prerendered_ukm_id)
        .SetFinalStatus(static_cast<int>(PrerenderFinalStatus::kActivated))
        .Record(ukm::UkmRecorder::Get());
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
  CHECK_EQ(trigger_type, PrerenderTriggerType::kEmbedder);
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
  CHECK_EQ(trigger_type, PrerenderTriggerType::kEmbedder);
  base::UmaHistogramEnumeration(
      GenerateHistogramName(
          "Prerender.Experimental.CrossOriginRedirectionProtocolChange",
          trigger_type, embedder_histogram_suffix),
      change_type);
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

void RecordPrerenderActivationTransition(
    int32_t potential_activation_transition,
    PrerenderTriggerType trigger_type,
    const std::string& embedder_histogram_suffix) {
  base::UmaHistogramSparse(
      GenerateHistogramName(
          "Prerender.Experimental.ActivationTransitionMismatch", trigger_type,
          embedder_histogram_suffix),
      potential_activation_transition);
}

void RecordPrerenderNavigationErrorCode(
    net::Error error_code,
    PrerenderTriggerType trigger_type,
    const std::string& embedder_histogram_suffix) {
  base::UmaHistogramSparse(
      GenerateHistogramName(
          "Prerender.Experimental.PrerenderNavigationRequestNetworkErrorCode",
          trigger_type, embedder_histogram_suffix),
      std::abs(error_code));
}

static_assert(
    static_cast<int>(PrerenderBackNavigationEligibility::kMaxValue) +
        static_cast<int>(
            PreloadingEligibility::kPreloadingEligibilityContentStart2) <
    static_cast<int>(PreloadingEligibility::kPreloadingEligibilityContentEnd2));

PreloadingEligibility ToPreloadingEligibility(
    PrerenderBackNavigationEligibility eligibility) {
  if (eligibility == PrerenderBackNavigationEligibility::kEligible) {
    return PreloadingEligibility::kEligible;
  }

  return static_cast<PreloadingEligibility>(
      static_cast<int>(eligibility) +
      static_cast<int>(
          PreloadingEligibility::kPreloadingEligibilityContentStart2));
}

void RecordPrerenderBackNavigationEligibility(
    PreloadingPredictor predictor,
    PrerenderBackNavigationEligibility eligibility,
    PreloadingAttempt* preloading_attempt) {
  const std::string histogram_name =
      std::string("Preloading.PrerenderBackNavigationEligibility.") +
      std::string(predictor.name());
  base::UmaHistogramEnumeration(histogram_name, eligibility);

  if (preloading_attempt) {
    preloading_attempt->SetEligibility(ToPreloadingEligibility(eligibility));
  }
}

void RecordPrerenderActivationCommitDeferTime(
    base::TimeDelta time_delta,
    PrerenderTriggerType trigger_type,
    const std::string& embedder_histogram_suffix) {
  base::UmaHistogramTimes(
      GenerateHistogramName("Navigation.Prerender.ActivationCommitDeferTime",
                            trigger_type, embedder_histogram_suffix),
      time_delta);
}

void RecordBlockedByClientResourceType(
    network::mojom::RequestDestination request_destination,
    PrerenderTriggerType trigger_type,
    const std::string& embedder_histogram_suffix) {
  base::UmaHistogramEnumeration(
      GenerateHistogramName(
          "Prerender.Experimental.ResourceLoadingBlockedByClientByType",
          trigger_type, embedder_histogram_suffix),
      request_destination);
}

}  // namespace content
