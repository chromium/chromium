// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/prerender_metrics.h"

#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/notreached.h"
#include "base/process/kill.h"
#include "base/strings/string_util.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
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

PrerenderProcessTerminationStatus TranslateToPrerenderUmaTerminationStatus(
    base::TerminationStatus status) {
  switch (status) {
    case base::TerminationStatus::TERMINATION_STATUS_NORMAL_TERMINATION:
      return PrerenderProcessTerminationStatus::kNormalTermination;
    case base::TerminationStatus::TERMINATION_STATUS_ABNORMAL_TERMINATION:
      return PrerenderProcessTerminationStatus::kAbnormalTermination;
    case base::TerminationStatus::TERMINATION_STATUS_PROCESS_WAS_KILLED:
      return PrerenderProcessTerminationStatus::kProcessWasKilled;
    case base::TerminationStatus::TERMINATION_STATUS_PROCESS_CRASHED:
      return PrerenderProcessTerminationStatus::kProcessCrashed;
    case base::TerminationStatus::TERMINATION_STATUS_STILL_RUNNING:
      return PrerenderProcessTerminationStatus::kStillRunning;
#if BUILDFLAG(IS_CHROMEOS)
    case base::TerminationStatus::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
      return PrerenderProcessTerminationStatus::kProcessWasKilledByOom;
#endif  // BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_ANDROID)
    case base::TerminationStatus::TERMINATION_STATUS_OOM_PROTECTED:
      return PrerenderProcessTerminationStatus::kOomProtected;
#endif  // BUILDFLAG(IS_ANDROID)
    case base::TerminationStatus::TERMINATION_STATUS_LAUNCH_FAILED:
      return PrerenderProcessTerminationStatus::kLaunchFailed;
    case base::TerminationStatus::TERMINATION_STATUS_OOM:
      return PrerenderProcessTerminationStatus::kOom;
#if BUILDFLAG(IS_WIN)
    case base::TerminationStatus::TERMINATION_STATUS_INTEGRITY_FAILURE:
      return PrerenderProcessTerminationStatus::kIntegrityFailure;
#endif  //  BUILDFLAG(IS_WIN)
    case base::TerminationStatus::TERMINATION_STATUS_MAX_ENUM:
      NOTREACHED();
      return PrerenderProcessTerminationStatus::kInvalid;
  }
  NOTREACHED();
  return PrerenderProcessTerminationStatus::kInvalid;
}

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

void RecordRendererProcessKilledTerminationStatus(
    base::TerminationStatus status_code,
    PrerenderTriggerType trigger_type,
    const std::string& embedder_histogram_suffix) {
  base::UmaHistogramEnumeration(
      GenerateHistogramName(
          "Prerender.Experimental.KilledPrerenderProcessTerminationStatus",
          trigger_type, embedder_histogram_suffix),
      TranslateToPrerenderUmaTerminationStatus(status_code));
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

// static
PrerenderCancellationReason
PrerenderCancellationReason::BuildForRendererProcessGone(
    base::TerminationStatus status_code) {
  if (status_code == base::TERMINATION_STATUS_PROCESS_CRASHED) {
    return PrerenderCancellationReason(
        PrerenderFinalStatus::kRendererProcessCrashed);
  }
  return PrerenderCancellationReason(
      PrerenderFinalStatus::kRendererProcessKilled, status_code);
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
      DCHECK(absl::holds_alternative<uint64_t>(explanation_));
      base::UmaHistogramSparse(
          GenerateHistogramName("Prerender.CanceledForInactivePageRestriction."
                                "DisallowActivationReason",
                                trigger_type, embedder_histogram_suffix),

          absl::get<uint64_t>(explanation_));
      break;
    case PrerenderFinalStatus::kMojoBinderPolicy:
      DCHECK(absl::holds_alternative<std::string>(explanation_));
      RecordPrerenderCancelledInterface(absl::get<std::string>(explanation_),
                                        trigger_type,
                                        embedder_histogram_suffix);
      break;
    case PrerenderFinalStatus::kRendererProcessKilled:
      DCHECK(absl::holds_alternative<base::TerminationStatus>(explanation_));
      RecordRendererProcessKilledTerminationStatus(
          absl::get<base::TerminationStatus>(explanation_), trigger_type,
          embedder_histogram_suffix);
      break;
    default:
      DCHECK(absl::holds_alternative<absl::monostate>(explanation_));
      // Other types need not to report.
      break;
  }
}

std::string PrerenderCancellationReason::ToDevtoolReasonString() const {
  switch (final_status_) {
    case PrerenderFinalStatus::kInactivePageRestriction:
      DCHECK(absl::holds_alternative<uint64_t>(explanation_));
      // TODO(https://crbug.com/1328365): It seems we have to return an integer.
      // And devtool has to handle it based on the enum.xml, as the content
      // layer cannot know about the enums added by the embedder layer.
      return "";
    case PrerenderFinalStatus::kRendererProcessKilled:
      DCHECK(absl::holds_alternative<base::TerminationStatus>(explanation_));
      // We do not have a plan to send the detailed crash reason to devtools
      // yet.
      return "";
    case PrerenderFinalStatus::kMojoBinderPolicy:
      DCHECK(absl::holds_alternative<std::string>(explanation_));
      return absl::get<std::string>(explanation_);
    default:
      DCHECK(absl::holds_alternative<absl::monostate>(explanation_));
      return "";
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
  DCHECK_NE(cancellation_reason.final_status(),
            PrerenderFinalStatus::kActivated);
  RecordPrerenderFinalStatusUma(cancellation_reason.final_status(),
                                attributes.trigger_type,
                                attributes.embedder_histogram_suffix);

  if (attributes.initiator_ukm_id != ukm::kInvalidSourceId) {
    // `initiator_ukm_id` must be valid for the speculation rules.
    DCHECK_EQ(attributes.trigger_type, PrerenderTriggerType::kSpeculationRule);
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
    DCHECK(ftn);
    // TODO(https://crbug.com/1332377): Discuss with devtools to finalize the
    // message protocol.
    devtools_instrumentation::DidCancelPrerender(
        attributes.prerendering_url, ftn, cancellation_reason.final_status(),
        cancellation_reason.ToDevtoolReasonString());
  }
}

void ReportSuccessActivation(const PrerenderAttributes& attributes,
                             ukm::SourceId prerendered_ukm_id) {
  RecordPrerenderFinalStatusUma(PrerenderFinalStatus::kActivated,
                                attributes.trigger_type,
                                attributes.embedder_histogram_suffix);
  if (attributes.initiator_ukm_id != ukm::kInvalidSourceId) {
    // `initiator_ukm_id` must be valid only for the speculation rules.
    DCHECK_EQ(attributes.trigger_type, PrerenderTriggerType::kSpeculationRule);
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
