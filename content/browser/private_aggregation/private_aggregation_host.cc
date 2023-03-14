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
#include "base/functional/callback.h"
#include "base/guid.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
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
}

PrivateAggregationHost::~PrivateAggregationHost() = default;

bool PrivateAggregationHost::BindNewReceiver(
    url::Origin worklet_origin,
    url::Origin top_frame_origin,
    PrivateAggregationBudgetKey::Api api_for_budgeting,
    mojo::PendingReceiver<blink::mojom::PrivateAggregationHost>
        pending_receiver) {
  if (!network::IsOriginPotentiallyTrustworthy(worklet_origin)) {
    // Let the pending receiver be destroyed as it goes out of scope so none of
    // its requests are processed.
    return false;
  }
  receiver_set_.Add(
      this, std::move(pending_receiver),
      ReceiverContext{.worklet_origin = std::move(worklet_origin),
                      .top_frame_origin = std::move(top_frame_origin),
                      .api_for_budgeting = api_for_budgeting});
  return true;
}

void PrivateAggregationHost::SendHistogramReport(
    std::vector<blink::mojom::AggregatableReportHistogramContributionPtr>
        contribution_ptrs,
    blink::mojom::AggregationServiceMode aggregation_mode,
    blink::mojom::DebugModeDetailsPtr debug_mode_details) {
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

  AggregationServicePayloadContents payload_contents(
      AggregationServicePayloadContents::Operation::kHistogram,
      std::move(contributions), aggregation_mode,
      ::aggregation_service::mojom::AggregationCoordinator::kDefault);

  base::Time now = base::Time::Now();

  AggregatableReportSharedInfo shared_info(
      /*scheduled_report_time=*/should_not_delay_reports_
          ? now
          : GetScheduledReportTime(
                /*report_issued_time=*/now),
      /*report_id=*/base::GUID::GenerateRandomV4(), reporting_origin,
      debug_mode_details->is_enabled
          ? AggregatableReportSharedInfo::DebugMode::kEnabled
          : AggregatableReportSharedInfo::DebugMode::kDisabled,
      /*additional_fields=*/base::Value::Dict(),
      /*api_version=*/kApiReportVersion,
      /*api_identifier=*/
      private_aggregation::GetApiIdentifier(
          receiver_set_.current_context().api_for_budgeting));

  std::string reporting_path = private_aggregation::GetReportingPath(
      receiver_set_.current_context().api_for_budgeting,
      /*is_immediate_debug_report=*/false);

  absl::optional<uint64_t> debug_key;
  if (!debug_mode_details->debug_key.is_null()) {
    if (!debug_mode_details->is_enabled) {
      mojo::ReportBadMessage("Debug key present but debug mode is not enabled");
      RecordSendHistogramReportResultHistogram(
          SendHistogramReportResult::kDebugKeyPresentWithoutDebugMode);
      return;
    }
    debug_key = debug_mode_details->debug_key->value;
  }

  absl::optional<AggregatableReportRequest> report_request =
      AggregatableReportRequest::Create(std::move(payload_contents),
                                        std::move(shared_info),
                                        std::move(reporting_path), debug_key);
  if (!report_request.has_value()) {
    mojo::ReportBadMessage("Invalid report request parameters");
    RecordSendHistogramReportResultHistogram(
        SendHistogramReportResult::kReportRequestCreationFailed);
    return;
  }

  absl::optional<PrivateAggregationBudgetKey> budget_key =
      PrivateAggregationBudgetKey::Create(
          /*origin=*/reporting_origin, /*api_invocation_time=*/now,
          /*api=*/receiver_set_.current_context().api_for_budgeting);

  // The origin should be potentially trustworthy.
  DCHECK(budget_key.has_value());

  on_report_request_received_.Run(std::move(report_request.value()),
                                  std::move(budget_key.value()));

  RecordSendHistogramReportResultHistogram(
      too_many_contributions ? SendHistogramReportResult::
                                   kSuccessButTruncatedDueToTooManyContributions
                             : SendHistogramReportResult::kSuccess);
}

}  // namespace content
