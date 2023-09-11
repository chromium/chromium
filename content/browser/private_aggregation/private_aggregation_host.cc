// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/private_aggregation/private_aggregation_host.h"

#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/uuid.h"
#include "base/values.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/private_aggregation/private_aggregation_budget_key.h"
#include "content/browser/private_aggregation/private_aggregation_utils.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/private_aggregation/aggregatable_report.mojom.h"
#include "third_party/blink/public/mojom/private_aggregation/private_aggregation_host.mojom.h"
#include "url/origin.h"

namespace content {

namespace {

// Helper to add a random delay to reports being sent. This delay is picked
// uniformly at random from the range [10 minutes, 1 hour).
// TODO(alexmt): Consider making this configurable for easier testing.
base::Time GetScheduledReportTime(base::Time report_issued_time) {
  return report_issued_time + base::Minutes(10) +
         base::RandDouble() * base::Minutes(50);
}

void RecordPipeResultHistogram(PrivateAggregationHost::PipeResult result) {
  base::UmaHistogramEnumeration(
      "PrivacySandbox.PrivateAggregation.Host.PipeResult", result);
}

}  // namespace

struct PrivateAggregationHost::ReceiverContext {
  url::Origin worklet_origin;
  url::Origin top_frame_origin;
  PrivateAggregationBudgetKey::Api api_for_budgeting;
  absl::optional<std::string> context_id;

  // If contributions have been truncated, tracks this for triggering the right
  // histogram value.
  bool too_many_contributions = false;

  // Contributions passed to `ContributeToHistogram()` for this receiver.
  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      contributions;

  // The debug mode details to use if a non-null report is sent. Cannot be null.
  blink::mojom::DebugModeDetailsPtr report_debug_details =
      blink::mojom::DebugModeDetails::New();
};

PrivateAggregationHost::PrivateAggregationHost(
    base::RepeatingCallback<void(AggregatableReportRequest,
                                 PrivateAggregationBudgetKey)>
        on_report_request_received,
    BrowserContext* browser_context)
    : should_not_delay_reports_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kPrivateAggregationDeveloperMode)),
      on_report_request_received_(std::move(on_report_request_received)),
      browser_context_(*browser_context) {
  DCHECK(!on_report_request_received_.is_null());

  // `base::Unretained()` is safe as `receiver_set_` is owned by `this`.
  receiver_set_.set_disconnect_handler(base::BindRepeating(
      &PrivateAggregationHost::OnReceiverDisconnected, base::Unretained(this)));
}

PrivateAggregationHost::~PrivateAggregationHost() {
  if (pipe_duration_timers_.empty()) {
    return;
  }
  for (auto& [id, elapsed_timer] : pipe_duration_timers_) {
    base::UmaHistogramLongTimes(
        "PrivacySandbox.PrivateAggregation.Host.PipeOpenDurationOnShutdown",
        elapsed_timer.Elapsed());
  }
}

bool PrivateAggregationHost::BindNewReceiver(
    url::Origin worklet_origin,
    url::Origin top_frame_origin,
    PrivateAggregationBudgetKey::Api api_for_budgeting,
    absl::optional<std::string> context_id,
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

  mojo::ReceiverId id = receiver_set_.Add(
      this, std::move(pending_receiver),
      ReceiverContext{.worklet_origin = std::move(worklet_origin),
                      .top_frame_origin = std::move(top_frame_origin),
                      .api_for_budgeting = api_for_budgeting,
                      .context_id = std::move(context_id)});

  auto emplace_result = pipe_duration_timers_.emplace(id, base::ElapsedTimer());
  CHECK(emplace_result.second);  // The ID should not already be present.

  return true;
}

bool PrivateAggregationHost::IsDebugModeAllowed(
    const url::Origin& top_frame_origin,
    const url::Origin& reporting_origin) {
  if (!blink::features::kPrivateAggregationApiDebugModeEnabledAtAll.Get()) {
    return false;
  }

  if (!blink::features::kPrivateAggregationApiDebugModeSettingsCheckEnabled
           .Get()) {
    return true;
  }

  return GetContentClient()->browser()->IsPrivateAggregationDebugModeAllowed(
      &*browser_context_, top_frame_origin, reporting_origin);
}

void PrivateAggregationHost::ContributeToHistogram(
    std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
        contribution_ptrs) {
  const url::Origin& reporting_origin =
      receiver_set_.current_context().worklet_origin;
  DCHECK(network::IsOriginPotentiallyTrustworthy(reporting_origin));

  if (!GetContentClient()->browser()->IsPrivateAggregationAllowed(
          &*browser_context_, receiver_set_.current_context().top_frame_origin,
          reporting_origin)) {
    CloseCurrentPipe(PipeResult::kApiDisabledInSettings);
    return;
  }

  // Null pointers should fail mojo validation.
  DCHECK(base::ranges::none_of(
      contribution_ptrs,
      [](const blink::mojom::AggregatableReportHistogramContributionPtr&
             contribution_ptr) { return contribution_ptr.is_null(); }));

  if (base::ranges::any_of(
          contribution_ptrs,
          [](const blink::mojom::AggregatableReportHistogramContributionPtr&
                 contribution_ptr) { return contribution_ptr->value < 0; })) {
    mojo::ReportBadMessage("Negative value encountered");
    CloseCurrentPipe(PipeResult::kNegativeValue);
    return;
  }

  // TODO(alexmt): Consider eliding contributions with values of zero as well as
  // potentially merging contributions with the same bucket (although that
  // should probably be done after budgeting).

  bool too_many_contributions =
      contribution_ptrs.size() +
          receiver_set_.current_context().contributions.size() >
      kMaxNumberOfContributions;
  if (too_many_contributions) {
    receiver_set_.current_context().too_many_contributions = true;
    const int num_to_copy =
        kMaxNumberOfContributions -
        receiver_set_.current_context().contributions.size();
    CHECK_GE(num_to_copy, 0);
    contribution_ptrs.resize(num_to_copy);
  }
  base::ranges::transform(
      contribution_ptrs,
      std::back_inserter(receiver_set_.current_context().contributions),
      [](const blink::mojom::AggregatableReportHistogramContributionPtr&
             contribution_ptr) { return std::move(*contribution_ptr); });
}

AggregatableReportRequest PrivateAggregationHost::GenerateReportRequest(
    std::vector<blink::mojom::AggregatableReportHistogramContribution>
        contributions,
    blink::mojom::AggregationServiceMode aggregation_mode,
    blink::mojom::DebugModeDetailsPtr debug_mode_details,
    base::Time scheduled_report_time,
    base::Uuid report_id,
    const url::Origin& reporting_origin,
    PrivateAggregationBudgetKey::Api api_for_budgeting,
    absl::optional<std::string> context_id) {
  AggregationServicePayloadContents payload_contents(
      AggregationServicePayloadContents::Operation::kHistogram,
      std::move(contributions), aggregation_mode,
      /*aggregation_coordinator_origin=*/absl::nullopt);

  CHECK(debug_mode_details);
  AggregatableReportSharedInfo shared_info(
      scheduled_report_time, std::move(report_id), reporting_origin,
      debug_mode_details->is_enabled
          ? AggregatableReportSharedInfo::DebugMode::kEnabled
          : AggregatableReportSharedInfo::DebugMode::kDisabled,
      /*additional_fields=*/base::Value::Dict(),
      /*api_version=*/kApiReportVersion,
      /*api_identifier=*/
      private_aggregation::GetApiIdentifier(api_for_budgeting));

  std::string reporting_path = private_aggregation::GetReportingPath(
      api_for_budgeting,
      /*is_immediate_debug_report=*/false);

  absl::optional<uint64_t> debug_key;
  if (!debug_mode_details->debug_key.is_null()) {
    CHECK(debug_mode_details->is_enabled);
    debug_key = debug_mode_details->debug_key->value;
  }

  base::flat_map<std::string, std::string> additional_fields;
  if (context_id.has_value()) {
    additional_fields["context_id"] = context_id.value();
  }

  absl::optional<AggregatableReportRequest> report_request =
      AggregatableReportRequest::Create(
          std::move(payload_contents), std::move(shared_info),
          std::move(reporting_path), debug_key, std::move(additional_fields));

  // All failure cases should've been handled by earlier validation code.
  CHECK(report_request.has_value());

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
  RecordPipeResultHistogram(pipe_result);

  mojo::ReceiverId current_receiver = receiver_set_.current_receiver();
  receiver_set_.Remove(current_receiver);
  pipe_duration_timers_.erase(current_receiver);
}

void PrivateAggregationHost::OnReceiverDisconnected() {
  pipe_duration_timers_.erase(receiver_set_.current_receiver());

  ReceiverContext& current_context = receiver_set_.current_context();
  const url::Origin& reporting_origin = current_context.worklet_origin;
  DCHECK(network::IsOriginPotentiallyTrustworthy(reporting_origin));

  if (!GetContentClient()->browser()->IsPrivateAggregationAllowed(
          &*browser_context_, current_context.top_frame_origin,
          reporting_origin)) {
    // No need to remove the pipe from `receiver_set_` as it's already
    // disconnected.
    RecordPipeResultHistogram(PipeResult::kApiDisabledInSettings);
    return;
  }

  if (current_context.report_debug_details->is_enabled &&
      !IsDebugModeAllowed(current_context.top_frame_origin, reporting_origin)) {
    current_context.report_debug_details =
        blink::mojom::DebugModeDetails::New();
  }

  if (current_context.contributions.empty()) {
    if (!current_context.context_id.has_value()) {
      RecordPipeResultHistogram(PipeResult::kNoReportButNoError);
      return;
    }

    // Null reports never have debug mode enabled.
    // TODO(crbug.com/1466668): Consider permitting this.
    current_context.report_debug_details =
        blink::mojom::DebugModeDetails::New();

    current_context.contributions.emplace_back(
        /*bucket=*/0, /*value=*/0);
  }

  base::Time now = base::Time::Now();

  // TODO(alexmt): Move report generation to the manager.
  AggregatableReportRequest report_request = GenerateReportRequest(
      std::move(current_context.contributions),
      // TODO(alexmt): Consider allowing this to be set.
      blink::mojom::AggregationServiceMode::kDefault,
      std::move(current_context.report_debug_details),
      /*scheduled_report_time=*/
      should_not_delay_reports_ ? now
                                : GetScheduledReportTime(
                                      /*report_issued_time=*/now),
      /*report_id=*/base::Uuid::GenerateRandomV4(), reporting_origin,
      current_context.api_for_budgeting, current_context.context_id);

  RecordPipeResultHistogram(
      current_context.too_many_contributions
          ? PipeResult::kReportSuccessButTruncatedDueToTooManyContributions
          : PipeResult::kReportSuccess);

  absl::optional<PrivateAggregationBudgetKey> budget_key =
      PrivateAggregationBudgetKey::Create(
          /*origin=*/reporting_origin, /*api_invocation_time=*/now,
          /*api=*/current_context.api_for_budgeting);

  // The origin should be potentially trustworthy.
  DCHECK(budget_key.has_value());

  on_report_request_received_.Run(std::move(report_request),
                                  std::move(budget_key.value()));
}

}  // namespace content
