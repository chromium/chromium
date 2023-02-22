// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregation_service_internals_handler_impl.h"

#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service.h"
#include "content/browser/aggregation_service/aggregation_service_internals.mojom.h"
#include "content/browser/aggregation_service/aggregation_service_storage.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

namespace content {

namespace {

AggregationService* GetAggregationService(WebContents* web_contents) {
  return AggregationService::GetService(web_contents->GetBrowserContext());
}

aggregation_service_internals::mojom::WebUIAggregatableReportPtr
CreateWebUIAggregatableReport(
    const AggregatableReportRequest& request,
    absl::optional<AggregationServiceStorage::RequestId> id,
    absl::optional<base::Time> actual_report_time,
    aggregation_service_internals::mojom::ReportStatus status,
    const absl::optional<AggregatableReport>& report) {
  std::vector<aggregation_service_internals::mojom::
                  AggregatableHistogramContributionPtr>
      contributions;
  base::ranges::transform(request.payload_contents().contributions,
                          std::back_inserter(contributions),
                          [](const auto& contribution) {
                            return aggregation_service_internals::mojom::
                                AggregatableHistogramContribution::New(
                                    contribution.bucket, contribution.value);
                          });

  base::Value::Dict report_body;
  if (report.has_value()) {
    report_body = report->GetAsJson();
  } else {
    report_body = AggregatableReport(
                      /*payloads=*/{}, request.shared_info().SerializeAsJson(),
                      request.debug_key())
                      .GetAsJson();

    constexpr char kAggregationServicePayloadsKey[] =
        "aggregation_service_payloads";
    DCHECK(!report_body.Find(kAggregationServicePayloadsKey));
    report_body.Set(kAggregationServicePayloadsKey,
                    "Not generated prior to send");
  }

  std::string output_json;
  bool success = base::JSONWriter::WriteWithOptions(
      report_body, base::JSONWriter::OPTIONS_PRETTY_PRINT, &output_json);
  DCHECK(success);

  base::Time report_time =
      actual_report_time.value_or(request.shared_info().scheduled_report_time);

  return aggregation_service_internals::mojom::WebUIAggregatableReport::New(
      id, report_time.ToJsTime(), request.shared_info().api_identifier,
      request.shared_info().api_version, request.GetReportingUrl(),
      std::move(contributions), status, output_json);
}

void ForwardReportsToWebUI(
    aggregation_service_internals::mojom::Handler::GetReportsCallback
        web_ui_callback,
    std::vector<AggregationServiceStorage::RequestAndId> requests_and_ids) {
  std::vector<aggregation_service_internals::mojom::WebUIAggregatableReportPtr>
      web_ui_reports;
  web_ui_reports.reserve(requests_and_ids.size());
  for (const AggregationServiceStorage::RequestAndId& request_and_id :
       requests_and_ids) {
    web_ui_reports.push_back(CreateWebUIAggregatableReport(
        request_and_id.request, request_and_id.id,
        /*actual_report_time=*/absl::nullopt,
        aggregation_service_internals::mojom::ReportStatus::kPending,
        /*report=*/absl::nullopt));
  }

  std::move(web_ui_callback).Run(std::move(web_ui_reports));
}

}  // namespace

AggregationServiceInternalsHandlerImpl::AggregationServiceInternalsHandlerImpl(
    WebUI* web_ui,
    mojo::PendingRemote<aggregation_service_internals::mojom::Observer>
        observer,
    mojo::PendingReceiver<aggregation_service_internals::mojom::Handler>
        handler)
    : web_ui_(web_ui),
      observer_(std::move(observer)),
      handler_(this, std::move(handler)) {
  DCHECK(web_ui);
  if (AggregationService* aggregation_service =
          GetAggregationService(web_ui_->GetWebContents())) {
    aggregation_service_observer_.Observe(aggregation_service);
    // `base::Unretained()` is safe because the observer is owned by `this`
    // and the callback will only be called while `observer_` is still alive.
    observer_.set_disconnect_handler(base::BindOnce(
        &AggregationServiceInternalsHandlerImpl::OnObserverDisconnected,
        base::Unretained(this)));
  }
}

AggregationServiceInternalsHandlerImpl::
    ~AggregationServiceInternalsHandlerImpl() = default;

void AggregationServiceInternalsHandlerImpl::GetReports(
    aggregation_service_internals::mojom::Handler::GetReportsCallback
        callback) {
  if (AggregationService* aggregation_service =
          GetAggregationService(web_ui_->GetWebContents())) {
    aggregation_service->GetPendingReportRequestsForWebUI(
        base::BindOnce(&ForwardReportsToWebUI, std::move(callback)));
  } else {
    std::move(callback).Run({});
  }
}

void AggregationServiceInternalsHandlerImpl::SendReports(
    const std::vector<AggregationServiceStorage::RequestId>& ids,
    aggregation_service_internals::mojom::Handler::SendReportsCallback
        callback) {
  if (AggregationService* aggregation_service =
          GetAggregationService(web_ui_->GetWebContents())) {
    aggregation_service->SendReportsForWebUI(ids, std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

void AggregationServiceInternalsHandlerImpl::ClearStorage(
    aggregation_service_internals::mojom::Handler::ClearStorageCallback
        callback) {
  if (AggregationService* aggregation_service =
          GetAggregationService(web_ui_->GetWebContents())) {
    aggregation_service->ClearData(/*delete_begin=*/base::Time::Min(),
                                   /*delete_end=*/base::Time::Max(),
                                   /*filter=*/base::NullCallback(),
                                   std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

void AggregationServiceInternalsHandlerImpl::OnRequestStorageModified() {
  observer_->OnRequestStorageModified();
}

void AggregationServiceInternalsHandlerImpl::OnReportHandled(
    const AggregatableReportRequest& request,
    absl::optional<AggregationServiceStorage::RequestId> id,
    const absl::optional<AggregatableReport>& report,
    base::Time actual_report_time,
    AggregationServiceObserver::ReportStatus status) {
  aggregation_service_internals::mojom::ReportStatus web_report_status;
  switch (status) {
    case AggregationServiceObserver::ReportStatus::kSent:
      web_report_status =
          aggregation_service_internals::mojom::ReportStatus::kSent;
      break;
    case AggregationServiceObserver::ReportStatus::kFailedToAssemble:
      web_report_status =
          aggregation_service_internals::mojom::ReportStatus::kFailedToAssemble;
      break;
    case AggregationServiceObserver::ReportStatus::kFailedToSend:
      web_report_status =
          aggregation_service_internals::mojom::ReportStatus::kFailedToSend;
      break;
  }

  observer_->OnReportHandled(CreateWebUIAggregatableReport(
      request, id, actual_report_time, web_report_status, report));
}

void AggregationServiceInternalsHandlerImpl::OnObserverDisconnected() {
  aggregation_service_observer_.Reset();
}

}  // namespace content
