// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/private_aggregation/private_aggregation_host.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <bit>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/extend.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/numerics/clamped_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "base/uuid.h"
#include "base/values.h"
#include "components/aggregation_service/aggregation_coordinator_utils.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/private_aggregation/private_aggregation_budget_key.h"
#include "content/browser/private_aggregation/private_aggregation_caller_api.h"
#include "content/browser/private_aggregation/private_aggregation_features.h"
#include "content/browser/private_aggregation/private_aggregation_manager.h"
#include "content/browser/private_aggregation/private_aggregation_pending_contributions.h"
#include "content/browser/private_aggregation/private_aggregation_utils.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom.h"
#include "third_party/blink/public/mojom/private_aggregation/private_aggregation_host.mojom.h"
#include "url/origin.h"

namespace content {

namespace {

void RecordPipeResultHistogram(PrivateAggregationHost::PipeResult result) {
  base::UmaHistogramEnumeration(
      "PrivacySandbox.PrivateAggregation.Host.PipeResult", result);
}

void RecordTimeoutResultHistogram(
    PrivateAggregationHost::TimeoutResult result) {
  base::UmaHistogramEnumeration(
      "PrivacySandbox.PrivateAggregation.Host.TimeoutResult", result);
}

void RecordFilteringIdStatusHistogram(bool has_filtering_id,
                                      bool has_custom_max_bytes) {
  PrivateAggregationHost::FilteringIdStatus status;

  if (has_filtering_id) {
    if (has_custom_max_bytes) {
      status = PrivateAggregationHost::FilteringIdStatus::
          kFilteringIdProvidedWithCustomMaxBytes;
    } else {
      status = PrivateAggregationHost::FilteringIdStatus::
          kFilteringIdProvidedWithDefaultMaxBytes;
    }
  } else {
    if (has_custom_max_bytes) {
      status = PrivateAggregationHost::FilteringIdStatus::
          kNoFilteringIdWithCustomMaxBytes;
    } else {
      status = PrivateAggregationHost::FilteringIdStatus::
          kNoFilteringIdWithDefaultMaxBytes;
    }
  }
  base::UmaHistogramEnumeration(
      "PrivacySandbox.PrivateAggregation.Host.FilteringIdStatus", status);
}

std::vector<std::string_view> GetSuffixesForHistograms(
    PrivateAggregationCallerApi caller_api,
    bool has_timeout) {
  constexpr std::string_view kProtectedAudienceSuffix = ".ProtectedAudience";
  constexpr std::string_view kSharedStorageSuffix = ".SharedStorage";
  constexpr std::string_view kSharedStorageReducedDelaySuffix =
      ".SharedStorage.ReducedDelay";
  constexpr std::string_view kSharedStorageFullDelaySuffix =
      ".SharedStorage.FullDelay";

  switch (caller_api) {
    case PrivateAggregationCallerApi::kProtectedAudience:
      return {kProtectedAudienceSuffix};
    case PrivateAggregationCallerApi::kSharedStorage:
      return {kSharedStorageSuffix, has_timeout
                                        ? kSharedStorageReducedDelaySuffix
                                        : kSharedStorageFullDelaySuffix};
    default:
      NOTREACHED();
  }
}

// `num_merge_keys_sent_or_truncated` is the total number of merge keys (i.e.
// unique bucket and filtering ID pairs) that passed through the mojo pipe.
void RecordNumberOfContributionMergeKeysHistogram(
    size_t num_merge_keys_sent_or_truncated,
    PrivateAggregationCallerApi caller_api,
    bool has_timeout) {
  constexpr std::string_view kMergeKeysHistogramBase =
      "PrivacySandbox.PrivateAggregation.Host.NumContributionMergeKeysInPipe";

  base::UmaHistogramCounts10000(kMergeKeysHistogramBase,
                                num_merge_keys_sent_or_truncated);

  for (std::string_view histogram_suffix :
       GetSuffixesForHistograms(caller_api, has_timeout)) {
    base::UmaHistogramCounts10000(
        base::StrCat({kMergeKeysHistogramBase, histogram_suffix}),
        num_merge_keys_sent_or_truncated);
  }
}

using ContributionMergeKey =
    PrivateAggregationPendingContributions::ContributionMergeKey;

}  // namespace

struct PrivateAggregationHost::ReceiverContext {
  url::Origin worklet_origin;
  url::Origin top_frame_origin;
  PrivateAggregationCallerApi caller_api;
  std::optional<std::string> context_id;
  std::optional<url::Origin> aggregation_coordinator_origin;
  size_t filtering_id_max_bytes;
  size_t effective_max_contributions;
  NullReportBehavior null_report_behavior = NullReportBehavior::kSendNullReport;

  // These fields are only used when `kPrivateAggregationApiErrorReporting` is
  // disabled.
  // TODO(crbug.com/381788013): Remove once feature is fully launched and flag
  // is removed.
  struct {
    // If contributions have been truncated, tracks this for triggering the
    // right histogram value.
    bool did_truncate_contributions = false;

    // Contributions passed to `ContributeToHistogram()` for this receiver,
    // associated with their merge keys.
    std::map<ContributionMergeKey,
             blink::mojom::AggregatableReportHistogramContribution>
        accepted_contributions;
  } pending_contributions_if_error_reporting_disabled;

  // Handles both unconditional and conditional contributions for this receiver.
  // Only populated if `kPrivateAggregationApiErrorReporting` is enabled.
  // TODO(crbug.com/381788013): Remove the optional wrapper once the feature is
  // fully launched and its flag is removed as we will no longer need the
  // Wrapper.
  std::optional<PrivateAggregationPendingContributions>
      pending_contributions_if_error_reporting_enabled;

  // For metrics only. Tracks those dropped due to the contribution limit.
  std::set<ContributionMergeKey> truncated_merge_keys;

  // The debug mode details to use if a non-null report is sent. Cannot be null.
  blink::mojom::DebugModeDetailsPtr report_debug_details =
      blink::mojom::DebugModeDetails::New();

  // If a timeout is specified by the client, this timer will be used to
  // schedule the timeout task. This should be nullptr iff no timeout is
  // specified by the client.
  std::unique_ptr<base::OneShotTimer> timeout_timer;

  // Tracks the duration of time that the mojo pipe has been open. Used for
  // duration measurement to ensure each pipe is being closed appropriately.
  base::ElapsedTimer pipe_duration_timer;
};

PrivateAggregationHost::PrivateAggregationHost(
    base::RepeatingCallback<
        void(ReportRequestGenerator,
             PrivateAggregationPendingContributions::Wrapper,
             PrivateAggregationBudgetKey,
             NullReportBehavior)> on_report_request_details_received,
    BrowserContext* browser_context)
    : should_not_delay_reports_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kPrivateAggregationDeveloperMode)),
      on_report_request_details_received_(
          std::move(on_report_request_details_received)),
      browser_context_(CHECK_DEREF(browser_context)) {
  CHECK(!on_report_request_details_received_.is_null());

  // `base::Unretained()` is safe as `receiver_set_` is owned by `this`.
  receiver_set_.set_disconnect_handler(base::BindRepeating(
      &PrivateAggregationHost::OnReceiverDisconnected, base::Unretained(this)));
}

PrivateAggregationHost::~PrivateAggregationHost() {
  for (const auto& [id, context_ptr] : receiver_set_.GetAllContexts()) {
    ReceiverContext& context = CHECK_DEREF(context_ptr);

    base::UmaHistogramLongTimes(
        "PrivacySandbox.PrivateAggregation.Host.PipeOpenDurationOnShutdown",
        context.pipe_duration_timer.Elapsed());

    if (context.timeout_timer) {
      RecordTimeoutResultHistogram(TimeoutResult::kStillScheduledOnShutdown);
    }
  }
}

// static
base::StrictNumeric<size_t>
PrivateAggregationHost::GetEffectiveMaxContributions(
    PrivateAggregationCallerApi caller_api,
    std::optional<size_t> requested_max_contributions) {
  // These constants define the maximum number of contributions that can go in
  // an `AggregatableReport` after merging.
  static constexpr size_t kMaxContributionsSharedStorage = 20;
  static constexpr size_t kMaxContributionsProtectedAudience = 100;
  static constexpr size_t kMaxContributionsWhenCustomized = 1000;

  if (requested_max_contributions.has_value()) {
    // Calling APIs should not pass the `maxContributions` field through when
    // the feature is disabled.
    CHECK(base::FeatureList::IsEnabled(
        blink::features::kPrivateAggregationApiMaxContributions));
    // Calling APIs must not pass a value of zero.
    CHECK_GT(*requested_max_contributions, 0u);
    return std::min(*requested_max_contributions,
                    kMaxContributionsWhenCustomized);
  }

  switch (caller_api) {
    case PrivateAggregationCallerApi::kSharedStorage:
      return kMaxContributionsSharedStorage;
    case PrivateAggregationCallerApi::kProtectedAudience:
      return kMaxContributionsProtectedAudience;
  }
  NOTREACHED();
}

bool PrivateAggregationHost::BindNewReceiver(
    url::Origin worklet_origin,
    url::Origin top_frame_origin,
    PrivateAggregationCallerApi caller_api,
    std::optional<std::string> context_id,
    std::optional<base::TimeDelta> timeout,
    std::optional<url::Origin> aggregation_coordinator_origin,
    size_t filtering_id_max_bytes,
    std::optional<size_t> max_contributions,
    mojo::PendingReceiver<blink::mojom::PrivateAggregationHost>
        pending_receiver) {
  // If rejected, let the pending receiver be destroyed as it goes out of scope
  // so none of its requests are processed.
  if (!network::IsOriginPotentiallyTrustworthy(worklet_origin)) {
    return false;
  }

  if (context_id.has_value() &&
      context_id.value().size() > kMaxContextIdLength) {
    return false;
  }

  if (aggregation_coordinator_origin.has_value() &&
      !aggregation_service::IsAggregationCoordinatorOriginAllowed(
          aggregation_coordinator_origin.value())) {
    return false;
  }

  if (filtering_id_max_bytes < 1 ||
      filtering_id_max_bytes >
          AggregationServicePayloadContents::kMaximumFilteringIdMaxBytes) {
    return false;
  }

  if (max_contributions.has_value() &&
      !base::FeatureList::IsEnabled(
          blink::features::kPrivateAggregationApiMaxContributions)) {
    return false;
  }

  const bool needs_deterministic_report_count =
      PrivateAggregationManager::ShouldSendReportDeterministically(
          caller_api, context_id, filtering_id_max_bytes, max_contributions);

  // Enforce that reduced delay is used iff null reports are enabled.
  if (timeout.has_value() != needs_deterministic_report_count) {
    return false;
  }

  size_t effective_max_contributions = GetEffectiveMaxContributions(
      caller_api, /*requested_max_contributions=*/max_contributions);

  std::optional<PrivateAggregationPendingContributions>
      pending_contributions_if_error_reporting_enabled;
  if (base::FeatureList::IsEnabled(
          blink::features::kPrivateAggregationApiErrorReporting)) {
    pending_contributions_if_error_reporting_enabled =
        PrivateAggregationPendingContributions(
            effective_max_contributions,
            GetSuffixesForHistograms(caller_api, timeout.has_value()));
  }

  mojo::ReceiverId id = receiver_set_.Add(
      this, std::move(pending_receiver),
      ReceiverContext{
          .worklet_origin = std::move(worklet_origin),
          .top_frame_origin = std::move(top_frame_origin),
          .caller_api = caller_api,
          .context_id = std::move(context_id),
          .aggregation_coordinator_origin =
              std::move(aggregation_coordinator_origin),
          .filtering_id_max_bytes = filtering_id_max_bytes,
          .effective_max_contributions = effective_max_contributions,
          .null_report_behavior = needs_deterministic_report_count
                                      ? NullReportBehavior::kSendNullReport
                                      : NullReportBehavior::kDontSendReport,
          .pending_contributions_if_error_reporting_enabled =
              std::move(pending_contributions_if_error_reporting_enabled),
      });

  if (timeout.has_value()) {
    CHECK(timeout->is_positive());

    ReceiverContext& context = CHECK_DEREF(receiver_set_.GetContext(id));
    context.timeout_timer = std::make_unique<base::OneShotTimer>();
    context.timeout_timer->Start(
        FROM_HERE, *timeout,
        base::BindOnce(
            &PrivateAggregationHost::OnTimeoutBeforeDisconnect,
            // Passing `base::Unretained(this)` is safe as `this` owns the
            // receiver context and the receiver context owns the timer.
            base::Unretained(this), id));
  }

  return true;
}

bool PrivateAggregationHost::IsDebugModeAllowed(
    const url::Origin& top_frame_origin,
    const url::Origin& reporting_origin) {
  if (!blink::features::kPrivateAggregationApiDebugModeEnabledAtAll.Get()) {
    return false;
  }

  if (!base::FeatureList::IsEnabled(
          kPrivateAggregationApiDebugModeRequires3pcEligibility)) {
    return true;
  }

  return GetContentClient()->browser()->IsPrivateAggregationDebugModeAllowed(
      &*browser_context_, top_frame_origin, reporting_origin);
}

bool PrivateAggregationHost::ValidateContributeCall(
    const std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>&
        contribution_ptrs) {
  const url::Origin& reporting_origin =
      receiver_set_.current_context().worklet_origin;
  CHECK(network::IsOriginPotentiallyTrustworthy(reporting_origin));

  if (!GetContentClient()->browser()->IsPrivateAggregationAllowed(
          &*browser_context_, receiver_set_.current_context().top_frame_origin,
          reporting_origin, /*out_block_is_site_setting_specific=*/nullptr)) {
    CloseCurrentPipe(PipeResult::kApiDisabledInSettings);
    return false;
  }

  using ContributionPtr =
      blink::mojom::AggregatableReportHistogramContributionPtr;

  // Null pointers should fail mojo validation.
  CHECK(std::ranges::none_of(contribution_ptrs, &ContributionPtr::is_null));

  if (std::ranges::any_of(contribution_ptrs,
                          [](const ContributionPtr& contribution) {
                            return contribution->value < 0;
                          })) {
    mojo::ReportBadMessage("Negative value encountered");
    CloseCurrentPipe(PipeResult::kNegativeValue);
    return false;
  }

  if (std::ranges::any_of(
          contribution_ptrs, [&](const ContributionPtr& contribution) {
            return static_cast<size_t>(
                       std::bit_width(contribution->filtering_id.value_or(0))) >
                   8 * receiver_set_.current_context().filtering_id_max_bytes;
          })) {
    mojo::ReportBadMessage("Filtering ID too big for max bytes");
    CloseCurrentPipe(PipeResult::kFilteringIdInvalid);
    return false;
  }

  return true;
}

void PrivateAggregationHost::ContributeToHistogram(
    std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
        contribution_ptrs) {
  if (!ValidateContributeCall(contribution_ptrs)) {
    return;
  }

  if (base::FeatureList::IsEnabled(
          blink::features::kPrivateAggregationApiErrorReporting)) {
    std::vector<blink::mojom::AggregatableReportHistogramContribution>
        contributions;
    base::Extend(
        contributions, std::move(contribution_ptrs),
        &blink::mojom::AggregatableReportHistogramContributionPtr::operator*);

    receiver_set_.current_context()
        .pending_contributions_if_error_reporting_enabled
        ->AddUnconditionalContributions(std::move(contributions));
    return;
  }

  std::map<ContributionMergeKey,
           blink::mojom::AggregatableReportHistogramContribution>&
      accepted_contributions =
          receiver_set_.current_context()
              .pending_contributions_if_error_reporting_disabled
              .accepted_contributions;

  for (blink::mojom::AggregatableReportHistogramContributionPtr& contribution :
       contribution_ptrs) {
    if (contribution->value == 0) {
      // Drop the contribution
      continue;
    }

    ContributionMergeKey merge_key(contribution);

    CHECK_LE(accepted_contributions.size(),
             receiver_set_.current_context().effective_max_contributions);

    auto accepted_contributions_it = accepted_contributions.find(merge_key);

    if (accepted_contributions_it == accepted_contributions.end()) {
      if (accepted_contributions.size() ==
          receiver_set_.current_context().effective_max_contributions) {
        receiver_set_.current_context()
            .pending_contributions_if_error_reporting_disabled
            .did_truncate_contributions = true;

        // Bound worst-case memory usage
        constexpr size_t kMaxTruncatedMergeKeysTracked = 10'000;
        if (receiver_set_.current_context().truncated_merge_keys.size() <
            kMaxTruncatedMergeKeysTracked) {
          receiver_set_.current_context().truncated_merge_keys.insert(
              std::move(merge_key));
        }
        continue;
      }
      accepted_contributions.emplace(std::move(merge_key),
                                     *std::move(contribution));
    } else {
      accepted_contributions_it->second.value =
          base::ClampedNumeric(accepted_contributions_it->second.value) +
          contribution->value;
    }
  }
}

void PrivateAggregationHost::ContributeToHistogramOnEvent(
    blink::mojom::PrivateAggregationErrorEvent error_event,
    std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
        contribution_ptrs) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kPrivateAggregationApiErrorReporting)) {
    mojo::ReportBadMessage(
        "ContributeToHistogramOnErrorEvent() called when error reporting "
        "feature is disabled");
    CloseCurrentPipe(PipeResult::kNecessaryFeatureNotEnabled);
    return;
  }

  if (!ValidateContributeCall(contribution_ptrs)) {
    return;
  }

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      contributions;
  base::Extend(
      contributions, std::move(contribution_ptrs),
      &blink::mojom::AggregatableReportHistogramContributionPtr::operator*);

  receiver_set_.current_context()
      .pending_contributions_if_error_reporting_enabled
      ->AddConditionalContributions(error_event, std::move(contributions));
}

AggregatableReportRequest PrivateAggregationHost::GenerateReportRequest(
    base::ElapsedTimer timeout_or_disconnect_timer,
    blink::mojom::DebugModeDetailsPtr debug_mode_details,
    base::Time scheduled_report_time,
    AggregatableReportRequest::DelayType delay_type,
    base::Uuid report_id,
    const url::Origin& reporting_origin,
    PrivateAggregationCallerApi caller_api,
    std::optional<std::string> context_id,
    std::optional<url::Origin> aggregation_coordinator_origin,
    size_t filtering_id_max_bytes,
    size_t max_contributions,
    std::vector<blink::mojom::AggregatableReportHistogramContribution>
        contributions) {
  // When there are zero contributions, we should only reach here if we are
  // sending a report deterministically.
  CHECK(!contributions.empty() ||
        PrivateAggregationManager::ShouldSendReportDeterministically(
            caller_api, context_id, filtering_id_max_bytes, max_contributions));
  CHECK(debug_mode_details);

  RecordFilteringIdStatusHistogram(
      /*has_filtering_id=*/std::ranges::any_of(
          contributions,
          [](blink::mojom::AggregatableReportHistogramContribution&
                 contribution) {
            return contribution.filtering_id.has_value();
          }),
      /*has_custom_max_bytes=*/filtering_id_max_bytes !=
          kDefaultFilteringIdMaxBytes);

  AggregationServicePayloadContents payload_contents(
      AggregationServicePayloadContents::Operation::kHistogram,
      std::move(contributions),
      std::move(aggregation_coordinator_origin),
      /*max_contributions_allowed=*/max_contributions, filtering_id_max_bytes);

  AggregatableReportSharedInfo shared_info(
      scheduled_report_time, std::move(report_id), reporting_origin,
      debug_mode_details->is_enabled
          ? AggregatableReportSharedInfo::DebugMode::kEnabled
          : AggregatableReportSharedInfo::DebugMode::kDisabled,
      /*additional_fields=*/base::Value::Dict(),
      /*api_version=*/kApiReportVersion,
      /*api_identifier=*/
      private_aggregation::GetApiIdentifier(caller_api));

  std::string reporting_path = private_aggregation::GetReportingPath(
      caller_api,
      /*is_immediate_debug_report=*/false);

  std::optional<uint64_t> debug_key;
  if (!debug_mode_details->debug_key.is_null()) {
    CHECK(debug_mode_details->is_enabled);
    debug_key = debug_mode_details->debug_key->value;
  }

  base::flat_map<std::string, std::string> additional_fields;
  if (context_id.has_value()) {
    additional_fields["context_id"] = context_id.value();
  }

  std::optional<AggregatableReportRequest> report_request =
      AggregatableReportRequest::Create(
          std::move(payload_contents), std::move(shared_info), delay_type,
          std::move(reporting_path), debug_key, std::move(additional_fields));

  // All failure cases should've been handled by earlier validation code.
  CHECK(report_request.has_value());

  if (context_id.has_value()) {
    base::UmaHistogramTimes(
        "PrivacySandbox.PrivateAggregation.Host."
        "TimeToGenerateReportRequestWithContextId",
        timeout_or_disconnect_timer.Elapsed());
  }

  return std::move(report_request).value();
}

void PrivateAggregationHost::EnableDebugMode(
    blink::mojom::DebugKeyPtr debug_key) {
  if (receiver_set_.current_context().report_debug_details->is_enabled) {
    mojo::ReportBadMessage("EnableDebugMode() called multiple times");
    CloseCurrentPipe(PipeResult::kEnableDebugModeCalledMultipleTimes);
    return;
  }

  receiver_set_.current_context().report_debug_details->is_enabled = true;
  receiver_set_.current_context().report_debug_details->debug_key =
      std::move(debug_key);
}

void PrivateAggregationHost::CloseCurrentPipe(PipeResult pipe_result) {
  // We should only reach here after an error.
  CHECK_NE(pipe_result, PipeResult::kReportSuccess);
  CHECK_NE(pipe_result,
           PipeResult::kReportSuccessButTruncatedDueToTooManyContributions);
  CHECK_NE(pipe_result, PipeResult::kNoReportButNoError);

  RecordPipeResultHistogram(pipe_result);

  if (receiver_set_.current_context().timeout_timer) {
    CHECK(receiver_set_.current_context().timeout_timer->IsRunning());
    RecordTimeoutResultHistogram(TimeoutResult::kCanceledDueToError);
  }

  mojo::ReceiverId current_receiver = receiver_set_.current_receiver();
  receiver_set_.Remove(current_receiver);
}

void PrivateAggregationHost::OnTimeoutBeforeDisconnect(mojo::ReceiverId id) {
  ReceiverContext& receiver_context = CHECK_DEREF(receiver_set_.GetContext(id));
  SendReportOnTimeoutOrDisconnect(
      receiver_context,
      /*remaining_timeout=*/base::TimeDelta(),
      PrivateAggregationPendingContributions::TimeoutOrDisconnect::kTimeout);

  RecordTimeoutResultHistogram(
      TimeoutResult::kOccurredBeforeRemoteDisconnection);

  receiver_set_.Remove(id);
}

void PrivateAggregationHost::OnReceiverDisconnected() {
  ReceiverContext& current_context = receiver_set_.current_context();
  if (!current_context.timeout_timer) {
    SendReportOnTimeoutOrDisconnect(current_context,
                                    /*remaining_timeout=*/base::TimeDelta(),
                                    PrivateAggregationPendingContributions::
                                        TimeoutOrDisconnect::kDisconnect);
    return;
  }

  CHECK(current_context.timeout_timer->IsRunning());

  RecordTimeoutResultHistogram(
      TimeoutResult::kOccurredAfterRemoteDisconnection);

  // TODO(https://crbug.com/354124875) Add UMA histogram to measure the
  // magnitude of negative `remaining_timeout` values. Also in
  // `OnTimeoutBeforeDisconnect()`.
  base::TimeDelta remaining_timeout =
      current_context.timeout_timer->desired_run_time() -
      base::TimeTicks::Now();

  if (remaining_timeout.is_negative()) {
    remaining_timeout = base::TimeDelta();
  }

  // Speed up tests when developer mode is enabled by ignoring the remaining
  // timeout. See https://crbug.com/362901607#comment7 for context.
  if (should_not_delay_reports_) {
    remaining_timeout = base::TimeDelta();
  }

  SendReportOnTimeoutOrDisconnect(
      current_context, remaining_timeout,
      PrivateAggregationPendingContributions::TimeoutOrDisconnect::kDisconnect);
}

void PrivateAggregationHost::SendReportOnTimeoutOrDisconnect(
    ReceiverContext& receiver_context,
    base::TimeDelta remaining_timeout,
    PrivateAggregationPendingContributions::TimeoutOrDisconnect
        timeout_or_disconnect) {
  CHECK(!remaining_timeout.is_negative());
  base::ElapsedTimer timeout_or_disconnect_timer;

  const url::Origin& reporting_origin = receiver_context.worklet_origin;
  CHECK(network::IsOriginPotentiallyTrustworthy(reporting_origin));

  if (!GetContentClient()->browser()->IsPrivateAggregationAllowed(
          &*browser_context_, receiver_context.top_frame_origin,
          reporting_origin, /*out_block_is_site_setting_specific=*/nullptr)) {
    // No need to remove the pipe from `receiver_set_` as it's already
    // disconnected or will get disconnected synchronously.
    RecordPipeResultHistogram(PipeResult::kApiDisabledInSettings);
    return;
  }

  if (receiver_context.report_debug_details->is_enabled &&
      !IsDebugModeAllowed(receiver_context.top_frame_origin,
                          reporting_origin)) {
    receiver_context.report_debug_details =
        blink::mojom::DebugModeDetails::New();
  }

  std::optional<PrivateAggregationPendingContributions::Wrapper>
      pending_contributions_wrapper;
  bool is_pending_contributions_empty;

  if (base::FeatureList::IsEnabled(
          blink::features::kPrivateAggregationApiErrorReporting)) {
    is_pending_contributions_empty =
        receiver_context.pending_contributions_if_error_reporting_enabled
            ->IsEmpty();

    pending_contributions_wrapper =
        PrivateAggregationPendingContributions::Wrapper(
            std::move(receiver_context
                          .pending_contributions_if_error_reporting_enabled)
                .value());

    pending_contributions_wrapper->GetPendingContributions()
        .MarkContributionsFinalized(timeout_or_disconnect);
  } else {
    std::vector<blink::mojom::AggregatableReportHistogramContribution>
        contributions;

    std::map<ContributionMergeKey,
             blink::mojom::AggregatableReportHistogramContribution>&
        accepted_contributions =
            receiver_context.pending_contributions_if_error_reporting_disabled
                .accepted_contributions;

    RecordNumberOfContributionMergeKeysHistogram(
        accepted_contributions.size() +
            receiver_context.truncated_merge_keys.size(),
        receiver_context.caller_api,
        /*has_timeout=*/!!receiver_context.timeout_timer);

    contributions.reserve(accepted_contributions.size());
    for (auto& contribution_it : accepted_contributions) {
      contributions.push_back(std::move(contribution_it.second));
    }

    is_pending_contributions_empty = contributions.empty();

    pending_contributions_wrapper =
        PrivateAggregationPendingContributions::Wrapper(
            std::move(contributions));
  }

  if (is_pending_contributions_empty) {
    switch (receiver_context.null_report_behavior) {
      case NullReportBehavior::kDontSendReport:
        RecordPipeResultHistogram(PipeResult::kNoReportButNoError);
        return;

      case NullReportBehavior::kSendNullReport:
        // Null reports caused by no contributions don't have debug mode
        // enabled if `kPrivateAggregationApiErrorReporting` is disabled.
        if (!base::FeatureList::IsEnabled(
                blink::features::kPrivateAggregationApiErrorReporting)) {
          receiver_context.report_debug_details =
              blink::mojom::DebugModeDetails::New();
        }
        break;
    }
  }

  const base::Time now = base::Time::Now();

  // If the timeout hasn't been reached, use a modified report issued time.
  base::Time scheduled_report_time = now + remaining_timeout;

  // Add a tiny window to account for local processing time, the majority of
  // which we expect to be spent in `PrivateAggregationBudgeter`. Otherwise, the
  // report time could passively leak information about the previous budgeting
  // history. For context, see <https://crbug.com/324314568>.
  scheduled_report_time += kTimeForLocalProcessing;

  const bool use_reduced_delay =
      should_not_delay_reports_ || receiver_context.timeout_timer;

  if (!use_reduced_delay) {
    // Add a full delay to the report time. The full delay is picked uniformly
    // at random from the range [10 minutes, 1 hour).
    // TODO(alexmt): Consider making this configurable for easier testing.
    scheduled_report_time +=
        base::Minutes(10) + base::RandDouble() * base::Minutes(50);
  }

  const AggregatableReportRequest::DelayType delay_type =
      use_reduced_delay
          ? AggregatableReportRequest::DelayType::ScheduledWithReducedDelay
          : AggregatableReportRequest::DelayType::ScheduledWithFullDelay;

  ReportRequestGenerator report_request_generator = base::BindOnce(
      GenerateReportRequest, std::move(timeout_or_disconnect_timer),
      std::move(receiver_context.report_debug_details), scheduled_report_time,
      delay_type, /*report_id=*/base::Uuid::GenerateRandomV4(),
      reporting_origin, receiver_context.caller_api,
      std::move(receiver_context.context_id),
      std::move(receiver_context.aggregation_coordinator_origin),
      receiver_context.filtering_id_max_bytes,
      receiver_context.effective_max_contributions);

  // Note: `kReportSuccessButTruncatedDueToTooManyContributions` is never
  // recorded if `kPrivateAggregationApiErrorReporting` is enabled as truncation
  // does not occur until later.
  RecordPipeResultHistogram(
      !base::FeatureList::IsEnabled(
          blink::features::kPrivateAggregationApiErrorReporting) &&
              receiver_context.pending_contributions_if_error_reporting_disabled
                  .did_truncate_contributions
          ? PipeResult::kReportSuccessButTruncatedDueToTooManyContributions
          : PipeResult::kReportSuccess);

  std::optional<PrivateAggregationBudgetKey> budget_key =
      PrivateAggregationBudgetKey::Create(
          /*origin=*/reporting_origin, /*api_invocation_time=*/now,
          receiver_context.caller_api);

  // The origin should be potentially trustworthy.
  CHECK(budget_key.has_value());

  on_report_request_details_received_.Run(
      std::move(report_request_generator),
      std::move(*pending_contributions_wrapper), std::move(budget_key.value()),
      receiver_context.null_report_behavior);
}

}  // namespace content
