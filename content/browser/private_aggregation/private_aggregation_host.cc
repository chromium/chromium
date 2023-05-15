// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/private_aggregation/private_aggregation_host.h"

#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "base/values.h"
#include "components/aggregation_service/aggregation_service.mojom.h"
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

void RecordSendHistogramReportResultHistogram(
    PrivateAggregationHost::SendHistogramReportResult result) {
  base::UmaHistogramEnumeration(
      "PrivacySandbox.PrivateAggregation.Host.SendHistogramReportResult",
      result);
}

}  // namespace

struct PrivateAggregationHost::ReceiverContext {
  url::Origin worklet_origin;
  url::Origin top_frame_origin;
  PrivateAggregationBudgetKey::Api api_for_budgeting;
  absl::optional<std::string> context_id;

  // Whether `SendHistogramReport()` has been called for this receiver.
  bool has_received_request = false;

  // If non-null, the debug mode details to use if a null report is sent.
  blink::mojom::DebugModeDetailsPtr null_report_debug_details;
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

PrivateAggregationHost::~PrivateAggregationHost() = default;

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

  receiver_set_.Add(
      this, std::move(pending_receiver),
      ReceiverContext{.worklet_origin = std::move(worklet_origin),
                      .top_frame_origin = std::move(top_frame_origin),
                      .api_for_budgeting = api_for_budgeting,
                      .context_id = std::move(context_id)});
  return true;
}

void PrivateAggregationHost::SendHistogramReport(
    std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
        contribution_ptrs,
    blink::mojom::AggregationServiceMode aggregation_mode,
    blink::mojom::DebugModeDetailsPtr debug_mode_details) {
  if (receiver_set_.current_context().has_received_request &&
      receiver_set_.current_context().context_id.has_value()) {
    mojo::ReportBadMessage("Pipe with context ID reused");
    RecordSendHistogramReportResultHistogram(
        SendHistogramReportResult::kPipeWithContextIdReused);
    return;
  }
  receiver_set_.current_context().has_received_request = true;

  const url::Origin& reporting_origin =
      receiver_set_.current_context().worklet_origin;
  DCHECK(network::IsOriginPotentiallyTrustworthy(reporting_origin));

  if (!GetContentClient()->browser()->IsPrivateAggregationAllowed(
          &*browser_context_, receiver_set_.current_context().top_frame_origin,
          reporting_origin)) {
    RecordSendHistogramReportResultHistogram(
        SendHistogramReportResult::kApiDisabledInSettings);
    return;
  }

  // Null pointers should fail mojo validation.
  DCHECK(base::ranges::none_of(
      contribution_ptrs,
      [](const blink::mojom::AggregatableReportHistogramContributionPtr&
             contribution_ptr) { return contribution_ptr.is_null(); }));
  DCHECK(!debug_mode_details.is_null());

  if (base::ranges::any_of(
          contribution_ptrs,
          [](const blink::mojom::AggregatableReportHistogramContributionPtr&
                 contribution_ptr) { return contribution_ptr->value < 0; })) {
    mojo::ReportBadMessage("Negative value encountered");
    RecordSendHistogramReportResultHistogram(
        SendHistogramReportResult::kNegativeValue);
    return;
  }

  // TODO(alexmt): Consider eliding contributions with values of zero as well as
  // potentially merging contributions with the same bucket (although that
  // should probably be done after budgeting).

  bool too_many_contributions =
      contribution_ptrs.size() > kMaxNumberOfContributions;
  if (too_many_contributions) {
    contribution_ptrs.resize(kMaxNumberOfContributions);
  }

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      contributions;
  contributions.reserve(contribution_ptrs.size());
  base::ranges::transform(
      contribution_ptrs, std::back_inserter(contributions),
      [](const blink::mojom::AggregatableReportHistogramContributionPtr&
             contribution_ptr) { return std::move(*contribution_ptr); });

  base::Time now = base::Time::Now();

  absl::optional<AggregatableReportRequest> report_request =
      GenerateReportRequest(
          std::move(contributions), aggregation_mode,
          std::move(debug_mode_details),
          /*scheduled_report_time=*/
          should_not_delay_reports_ ? now
                                    : GetScheduledReportTime(
                                          /*report_issued_time=*/now),
          /*report_id=*/base::Uuid::GenerateRandomV4(), reporting_origin,
          receiver_set_.current_context().api_for_budgeting,
          receiver_set_.current_context().context_id);

  if (!report_request.has_value()) {
    return;
  }

  RecordSendHistogramReportResultHistogram(
      too_many_contributions ? SendHistogramReportResult::
                                   kSuccessButTruncatedDueToTooManyContributions
                             : SendHistogramReportResult::kSuccess);

  absl::optional<PrivateAggregationBudgetKey> budget_key =
      PrivateAggregationBudgetKey::Create(
          /*origin=*/reporting_origin, /*api_invocation_time=*/now,
          /*api=*/receiver_set_.current_context().api_for_budgeting);

  // The origin should be potentially trustworthy.
  DCHECK(budget_key.has_value());

  on_report_request_received_.Run(std::move(report_request.value()),
                                  std::move(budget_key.value()));
}

absl::optional<AggregatableReportRequest>
PrivateAggregationHost::GenerateReportRequest(
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
      ::aggregation_service::mojom::AggregationCoordinator::kDefault);

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
    if (!debug_mode_details->is_enabled) {
      mojo::ReportBadMessage("Debug key present but debug mode is not enabled");
      RecordSendHistogramReportResultHistogram(
          SendHistogramReportResult::kDebugKeyPresentWithoutDebugMode);
      return absl::nullopt;
    }
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

  if (!report_request.has_value()) {
    mojo::ReportBadMessage("Invalid report request parameters");
    RecordSendHistogramReportResultHistogram(
        SendHistogramReportResult::kReportRequestCreationFailed);
    return absl::nullopt;
  }

  return report_request;
}

void PrivateAggregationHost::SetDebugModeDetailsOnNullReport(
    blink::mojom::DebugModeDetailsPtr debug_mode_details) {
  if (receiver_set_.current_context().null_report_debug_details) {
    mojo::ReportBadMessage(
        "SetDebugModeDetailsOnNullReport() called multiple times");
    return;
  }

  receiver_set_.current_context().null_report_debug_details =
      std::move(debug_mode_details);
}

void PrivateAggregationHost::OnReceiverDisconnected() {
  if (receiver_set_.current_context().context_id.has_value() &&
      !receiver_set_.current_context().has_received_request) {
    // Send a null report,
    // TODO(alexmt): Consider switching to an empty vector.
    std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
        contributions;
    contributions.push_back(
        blink::mojom::AggregatableReportHistogramContribution::New(
            /*bucket=*/0, /*value=*/0));

    blink::mojom::DebugModeDetailsPtr null_report_debug_details =
        std::move(receiver_set_.current_context().null_report_debug_details);

    if (null_report_debug_details.is_null()) {
      // While we expected SetDebugModeDetailsOnNullReport() to be called, we
      // still want to send a null report.
      null_report_debug_details = blink::mojom::DebugModeDetails::New();
    }

    SendHistogramReport(std::move(contributions),
                        blink::mojom::AggregationServiceMode::kDefault,
                        std::move(null_report_debug_details));
  }
}

}  // namespace content
