// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_METRICS_H_
#define CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_METRICS_H_

#include <cstdint>
#include <string>

#include "base/time/time.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/preloading/prerender/prerender_host.h"
#include "content/public/browser/prerender_trigger_type.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/fetch_api.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace content {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Note: Please update GetCancelledInterfaceType() in the corresponding .cc file
// and the enum of PrerenderCancelledUnknownInterface in
// tools/metrics/histograms/enums.xml if you add a new item.
enum class PrerenderCancelledInterface {
  kUnknown = 0,  // For kCancel interfaces added by embedders or tests.
  kGamepadHapticsManager = 1,
  kGamepadMonitor = 2,
  // kNotificationService = 3,   Deprecated.
  kTrustedVaultEncryptionKeys = 4,
  kMaxValue = kTrustedVaultEncryptionKeys
};

// Used by PrerenderNavigationThrottle, to track the cross-origin cancellation
// reason, and break it down into more cases.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PrerenderCrossOriginRedirectionMismatch {
  kShouldNotBeReported = 0,
  kPortMismatch = 1,
  kHostMismatch = 2,
  kHostPortMismatch = 3,
  kSchemeMismatch = 4,
  kSchemePortMismatch = 5,
  kSchemeHostMismatch = 6,
  kSchemeHostPortMismatch = 7,
  kMaxValue = kSchemeHostPortMismatch
};

// Assembles PrerenderHostFinalStatus with a detailed explanation if applicable.
// Some FinalStatus enums cover multiple sub cases. To explain them in detail,
// some explanations can be attached to the status.
class PrerenderCancellationReason {
 public:
  // Tagged by `final_status_`. See `BuildFor*` and `ToDevtoolReasonString`.
  using DetailedReasonVariant =
      absl::variant<absl::monostate, int32_t, uint64_t, std::string>;

  static PrerenderCancellationReason BuildForDisallowActivationState(
      uint64_t disallow_activation_reason);

  static PrerenderCancellationReason BuildForMojoBinderPolicy(
      const std::string& interface_name);

  static PrerenderCancellationReason BuildForLoadingError(int32_t error_code);

  explicit PrerenderCancellationReason(PrerenderFinalStatus final_status);
  ~PrerenderCancellationReason();

  PrerenderCancellationReason(PrerenderCancellationReason&& reason);

  // Reports UMA and UKM metrics.
  void ReportMetrics(PrerenderTriggerType trigger_type,
                     const std::string& embedder_histogram_suffix) const;

  PrerenderFinalStatus final_status() const { return final_status_; }

  // This is mainly used for displaying a detailed reason on devtools panel.
  std::string ToDevtoolReasonString() const;
  // Returns disallowed Mojo interface name iff final status is
  // `kMojoBinderPolicy`.
  absl::optional<std::string> DisallowedMojoInterface() const;

 private:
  PrerenderCancellationReason(PrerenderFinalStatus final_status,
                              DetailedReasonVariant explanation);

  const PrerenderFinalStatus final_status_;

  const DetailedReasonVariant explanation_;
};

// Used by PrerenderNavigationThrottle. This is a breakdown enum for
// PrerenderCrossOriginRedirectionMismatch.kSchemePortMismatch.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PrerenderCrossOriginRedirectionProtocolChange {
  kHttpProtocolUpgrade = 0,
  kHttpProtocolDowngrade = 1,
  kMaxValue = kHttpProtocolDowngrade
};

void RecordPrerenderTriggered(ukm::SourceId ukm_id);

void RecordPrerenderActivationTime(
    base::TimeDelta delta,
    PrerenderTriggerType trigger_type,
    const std::string& embedder_histogram_suffix);

// Used by failing prerender attempts. Records the status to UMA and UKM, and
// reports the failing reason to devtools. In the attributes, `initiator_ukm_id`
// represents the page that starts prerendering.
void RecordFailedPrerenderFinalStatus(
    const PrerenderCancellationReason& cancellation_reason,
    const PrerenderAttributes& attributes);

// Records a success activation to UMA and UKM.
// `prerendered_ukm_id` is the UKM ID of the activated page.
void ReportSuccessActivation(const PrerenderAttributes& attributes,
                             ukm::SourceId prerendered_ukm_id);

// Records which navigation parameters are different between activation and
// initial prerender navigation when activation fails.
void RecordPrerenderActivationNavigationParamsMatch(
    PrerenderHost::ActivationNavigationParamsMatch result,
    PrerenderTriggerType trigger_type,
    const std::string& embedder_suffix);

// Records the detailed types of the cross-origin redirection, e.g., changes to
// scheme, host name etc.
void RecordPrerenderRedirectionMismatchType(
    PrerenderCrossOriginRedirectionMismatch case_type,
    PrerenderTriggerType trigger_type,
    const std::string& embedder_histogram_suffix);

// Records whether the redirection was caused by HTTP protocol upgrade.
void RecordPrerenderRedirectionProtocolChange(
    PrerenderCrossOriginRedirectionProtocolChange change_type,
    PrerenderTriggerType trigger_type,
    const std::string& embedder_histogram_suffix);

// Takes the headers of incoming navigation which can potentially activate a
// prerendering navigation as the input, and compares them with the prerendering
// navigation's. The comparison is order-insensitive and case-insensitive,
// unlike converting the headers to strings and comparing the strings naively.
// For each mismatch case, this function reports a record to UMA, so that we can
// track the use of each header and tell if prerender sets all headers as
// expected.
// Assuming the given headers mismatch, this function will report a record if it
// finds that everything matches. This record will be used to ensure the current
// activation logic which compares the headers in strings is correct.
void CONTENT_EXPORT AnalyzePrerenderActivationHeader(
    net::HttpRequestHeaders potential_activation_headers,
    net::HttpRequestHeaders prerender_headers,
    PrerenderTriggerType trigger_type,
    const std::string& embedder_histogram_suffix);

// Records ui::PageTransition of prerender activation navigation when transition
// mismatch happens on prerender activation.
void RecordPrerenderActivationTransition(
    int32_t potential_activation_transition,
    PrerenderTriggerType trigger_type,
    const std::string& embedder_histogram_suffix);

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// These are also mapped onto the second content internal range of
// `PreloadingEligibility`.
enum class PrerenderBackNavigationEligibility {
  kEligible = 0,

  kNoBackEntry = 1,
  kTargetIsSameDocument = 2,
  kMethodNotGet = 3,
  kTargetIsFailedNavigation = 4,
  kBfcacheEntryExists = 5,
  kTargetIsSameSite = 6,
  kNoHttpCacheEntry = 7,
  kTargetingOtherWindow = 8,
  kTargetIsNonHttp = 9,
  kRelatedActiveContents = 10,

  kMaxValue = kRelatedActiveContents,
};

// Maps `eligibility` onto a content internal range of PreloadingEligibility.
CONTENT_EXPORT PreloadingEligibility
ToPreloadingEligibility(PrerenderBackNavigationEligibility eligibility);

void RecordPrerenderBackNavigationEligibility(
    PreloadingPredictor predictor,
    PrerenderBackNavigationEligibility eligibility,
    PreloadingAttempt* preloading_attempt);

void RecordPrerenderActivationCommitDeferTime(
    base::TimeDelta time_delta,
    PrerenderTriggerType trigger_type,
    const std::string& embedder_histogram_suffix);

void RecordBlockedByClientResourceType(
    network::mojom::RequestDestination request_destination,
    PrerenderTriggerType trigger_type,
    const std::string& embedder_histogram_suffix);

void RecordReceivedPrerendersPerPrimaryPageChangedCount(
    int number,
    PrerenderTriggerType trigger_type,
    const std::string& eagerness_category);

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_METRICS_H_
