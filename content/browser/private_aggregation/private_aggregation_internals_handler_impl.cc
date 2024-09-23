// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/private_aggregation/private_aggregation_internals_handler_impl.h"

#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service.h"
#include "content/browser/aggregation_service/aggregation_service_storage.h"
#include "content/browser/private_aggregation/private_aggregation_internals.mojom.h"
#include "content/browser/private_aggregation/private_aggregation_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

namespace content {

namespace {

AggregationService* GetAggregationService(WebContents* web_contents) {
  CHECK(web_contents);
  return AggregationService::GetService(web_contents->GetBrowserContext());
}

PrivateAggregationManager* GetPrivateAggregationManager(
    WebContents* web_contents) {
  CHECK(web_contents);
  BrowserContext* browser_context = web_contents->GetBrowserContext();
  CHECK(browser_context);
  return PrivateAggregationManager::GetManager(*browser_context);
}

private_aggregation_internals::mojom::WebUIAggregatableReportPtr
CreateWebUIAggregatableReport(
    const AggregatableReportRequest& request,
    std::optional<AggregationServiceStorage::RequestId> id,
    std::optional<base::Time> actual_report_time,
    private_aggregation_internals::mojom::ReportStatus status,
    const std::optional<AggregatableReport>& report) {
  std::vector<private_aggregation_internals::mojom::
                  AggregatableHistogramContributionPtr>
      contributions;
  base::ranges::transform(request.payload_contents().contributions,
                          std::back_inserter(contributions),
                          [](const auto& contribution) {
                            return private_aggregation_internals::mojom::
                                AggregatableHistogramContribution::New(
                                    contribution.bucket, contribution.value);
                          });

  base::Value::Dict report_body;
  if (report.has_value()) {
    report_body = report->GetAsJson();
  } else {
    report_body = AggregatableReport(
                      /*payloads=*/{}, request.shared_info().SerializeAsJson(),
                      request.debug_key(), request.additional_fields(),
                      request.payload_contents().aggregation_coordinator_origin)
                      .GetAsJson();

    constexpr char kAggregationServicePayloadsKey[] =
        "aggregation_service_payloads";
    CHECK(!report_body.Find(kAggregationServicePayloadsKey));
    report_body.Set(kAggregationServicePayloadsKey,
                    "Not generated prior to send");
  }

  std::string output_json;
  bool success = base::JSONWriter::WriteWithOptions(
      report_body, base::JSONWriter::OPTIONS_PRETTY_PRINT, &output_json);
  CHECK(success);

  base::Time report_time =
      actual_report_time.value_or(request.shared_info().scheduled_report_time);

  return private_aggregation_internals::mojom::WebUIAggregatableReport::New(
      id, report_time.InMillisecondsFSinceUnixEpoch(),
      request.shared_info().api_identifier, request.shared_info().api_version,
      request.GetReportingUrl(), std::move(contributions), status, output_json);
}

void ForwardReportsToWebUI(
    private_aggregation_internals::mojom::Handler::GetReportsCallback
        web_ui_callback,
    std::vector<AggregationServiceStorage::RequestAndId> requests_and_ids) {
  std::vector<private_aggregation_internals::mojom::WebUIAggregatableReportPtr>
      web_ui_reports;
  web_ui_reports.reserve(requests_and_ids.size());
  for (const AggregationServiceStorage::RequestAndId& request_and_id :
       requests_and_ids) {
    web_ui_reports.push_back(CreateWebUIAggregatableReport(
        request_and_id.request, request_and_id.id,
        /*actual_report_time=*/std::nullopt,
        private_aggregation_internals::mojom::ReportStatus::kPending,
        /*report=*/std::nullopt));
  }

  std::move(web_ui_callback).Run(std::move(web_ui_reports));
}

}  // namespace

PrivateAggregationInternalsHandlerImpl::PrivateAggregationInternalsHandlerImpl(
    WebUI* web_ui,
    mojo::PendingRemote<private_aggregation_internals::mojom::Observer>
        observer,
    mojo::PendingReceiver<private_aggregation_internals::mojom::Handler>
        handler)
    : web_ui_(web_ui),
      observer_(std::move(observer)),
      handler_(this, std::move(handler)) {
  CHECK(web_ui);
  if (AggregationService* aggregation_service =
          GetAggregationService(web_ui_->GetWebContents())) {
    aggregation_service_observer_.Observe(aggregation_service);
    // `base::Unretained()` is safe because the observer is owned by `this`
    // and the callback will only be called while `observer_` is still alive.
    observer_.set_disconnect_handler(base::BindOnce(
        &PrivateAggregationInternalsHandlerImpl::OnObserverDisconnected,
        base::Unretained(this)));
  }
}

PrivateAggregationInternalsHandlerImpl::
    ~PrivateAggregationInternalsHandlerImpl() = default;

void PrivateAggregationInternalsHandlerImpl::GetReports(
    private_aggregation_internals::mojom::Handler::GetReportsCallback
        callback) {
  if (AggregationService* aggregation_service =
          GetAggregationService(web_ui_->GetWebContents())) {
    aggregation_service->GetPendingReportRequestsForWebUI(
        base::BindOnce(&ForwardReportsToWebUI, std::move(callback)));
  } else {
    std::move(callback).Run({});
  }
}

void PrivateAggregationInternalsHandlerImpl::SendReports(
    const std::vector<AggregationServiceStorage::RequestId>& ids,
    private_aggregation_internals::mojom::Handler::SendReportsCallback
        callback) {
  if (AggregationService* aggregation_service =
          GetAggregationService(web_ui_->GetWebContents())) {
    aggregation_service->SendReportsForWebUI(ids, std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

void PrivateAggregationInternalsHandlerImpl::ClearStorage(
    private_aggregation_internals::mojom::Handler::ClearStorageCallback
        callback) {
  // Only run `callback` after we've cleared the aggregation service data *and*
  // the private aggregation budget data.
  auto barrier = base::BarrierClosure(/*num_closures=*/2, std::move(callback));

  if (AggregationService* aggregation_service =
          GetAggregationService(web_ui_->GetWebContents())) {
    aggregation_service->ClearData(/*delete_begin=*/base::Time::Min(),
                                   /*delete_end=*/base::Time::Max(),
                                   /*filter=*/base::NullCallback(), barrier);
  } else {
    barrier.Run();
  }

  if (PrivateAggregationManager* private_aggregation_manager =
          GetPrivateAggregationManager(web_ui_->GetWebContents())) {
    private_aggregation_manager->ClearBudgetData(
        /*delete_begin=*/base::Time::Min(),
        /*delete_end=*/base::Time::Max(),
        /*filter=*/base::NullCallback(), std::move(barrier));
  } else {
    std::move(barrier).Run();
  }
}

void PrivateAggregationInternalsHandlerImpl::OnRequestStorageModified() {
  observer_->OnRequestStorageModified();
}

void PrivateAggregationInternalsHandlerImpl::OnReportHandled(
    const AggregatableReportRequest& request,
    std::optional<AggregationServiceStorage::RequestId> id,
    const std::optional<AggregatableReport>& report,
    base::Time actual_report_time,
    AggregationServiceObserver::ReportStatus status) {
  private_aggregation_internals::mojom::ReportStatus web_report_status;
  switch (status) {
    case AggregationServiceObserver::ReportStatus::kSent:
      web_report_status =
          private_aggregation_internals::mojom::ReportStatus::kSent;
      break;
    case AggregationServiceObserver::ReportStatus::kFailedToAssemble:
      web_report_status =
          private_aggregation_internals::mojom::ReportStatus::kFailedToAssemble;
      break;
    case AggregationServiceObserver::ReportStatus::kFailedToSend:
      web_report_status =
          private_aggregation_internals::mojom::ReportStatus::kFailedToSend;
      break;
  }

  observer_->OnReportHandled(CreateWebUIAggregatableReport(
      request, id, actual_report_time, web_report_status, report));
}

void PrivateAggregationInternalsHandlerImpl::OnObserverDisconnected() {
  aggregation_service_observer_.Reset();
}

}  // namespace content
