// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/private_aggregation/private_aggregation_host.h"

#include <iterator>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/check.h"
#include "base/guid.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/private_aggregation/private_aggregation_budget_key.h"
#include "content/common/aggregatable_report.mojom.h"
#include "content/common/private_aggregation_host.mojom.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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
    : on_report_request_received_(std::move(on_report_request_received)),
      browser_context_(*browser_context) {
  DCHECK(!on_report_request_received_.is_null());
}

PrivateAggregationHost::~PrivateAggregationHost() = default;

bool PrivateAggregationHost::BindNewReceiver(
    url::Origin worklet_origin,
    url::Origin top_frame_origin,
    PrivateAggregationBudgetKey::Api api_for_budgeting,
    mojo::PendingReceiver<mojom::PrivateAggregationHost> pending_receiver) {
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
    std::vector<mojom::AggregatableReportHistogramContributionPtr>
        contribution_ptrs,
    mojom::AggregationServiceMode aggregation_mode) {
  // TODO(alexmt): Consider updating or making a FeatureParam.
  static constexpr char kFledgeReportingPath[] =
      "/.well-known/private-aggregation/report-fledge";
  static constexpr char kSharedStorageReportingPath[] =
      "/.well-known/private-aggregation/report-shared-storage";

  const url::Origin& reporting_origin =
      receiver_set_.current_context().worklet_origin;
  DCHECK(network::IsOriginPotentiallyTrustworthy(reporting_origin));

  if (!GetContentClient()->browser()->IsPrivateAggregationAllowed(
          &*browser_context_, receiver_set_.current_context().top_frame_origin,
          reporting_origin)) {
    return;
  }

  // Null pointers should fail mojo validation.
  DCHECK(base::ranges::none_of(
      contribution_ptrs,
      [](const mojom::AggregatableReportHistogramContributionPtr&
             contribution_ptr) { return contribution_ptr.is_null(); }));

  if (contribution_ptrs.size() > kMaxNumberOfContributions) {
    // TODO(crbug.com/1323324): Add histograms for monitoring failures here,
    // possibly broken out by failure reason.
    mojo::ReportBadMessage("Too many contributions");
    return;
  }

  std::vector<mojom::AggregatableReportHistogramContribution> contributions;
  contributions.reserve(contribution_ptrs.size());
  base::ranges::transform(
      contribution_ptrs, std::back_inserter(contributions),
      [](const mojom::AggregatableReportHistogramContributionPtr&
             contribution_ptr) { return std::move(*contribution_ptr); });

  AggregationServicePayloadContents payload_contents(
      AggregationServicePayloadContents::Operation::kHistogram,
      std::move(contributions), aggregation_mode);

  base::Time now = base::Time::Now();

  AggregatableReportSharedInfo shared_info(
      /*scheduled_report_time=*/GetScheduledReportTime(
          /*report_issued_time=*/now),
      /*report_id=*/base::GUID::GenerateRandomV4(), reporting_origin,
      AggregatableReportSharedInfo::DebugMode::kDisabled,
      /*additional_fields=*/base::Value::Dict(),
      /*api_version=*/kApiReportVersion,
      /*api_identifier=*/kApiIdentifier);

  std::string reporting_path;
  switch (receiver_set_.current_context().api_for_budgeting) {
    case PrivateAggregationBudgetKey::Api::kFledge:
      reporting_path = kFledgeReportingPath;
      break;
    case PrivateAggregationBudgetKey::Api::kSharedStorage:
      reporting_path = kSharedStorageReportingPath;
      break;
  }

  absl::optional<AggregatableReportRequest> report_request =
      AggregatableReportRequest::Create(std::move(payload_contents),
                                        std::move(shared_info),
                                        std::move(reporting_path));
  if (!report_request.has_value()) {
    // TODO(crbug.com/1323324): Add histograms for monitoring failures here,
    // possibly broken out by failure reason.
    mojo::ReportBadMessage("Invalid report request parameters");
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
}

}  // namespace content
