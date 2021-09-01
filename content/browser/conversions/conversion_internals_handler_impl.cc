// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_internals_handler_impl.h"

#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/containers/circular_deque.h"
#include "base/time/time.h"
#include "content/browser/conversions/conversion_manager_impl.h"
#include "content/browser/conversions/conversion_report.h"
#include "content/browser/conversions/sent_report_info.h"
#include "content/browser/conversions/storable_impression.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"

namespace content {

namespace {

::mojom::SourceType SourceTypeToMojoType(StorableImpression::SourceType input) {
  switch (input) {
    case StorableImpression::SourceType::kNavigation:
      return ::mojom::SourceType::kNavigation;
    case StorableImpression::SourceType::kEvent:
      return ::mojom::SourceType::kEvent;
  }
}

void ForwardImpressionsToWebUI(
    ::mojom::ConversionInternalsHandler::GetActiveImpressionsCallback
        web_ui_callback,
    std::vector<StorableImpression> stored_impressions) {
  std::vector<::mojom::WebUIImpressionPtr> web_ui_impressions;
  web_ui_impressions.reserve(stored_impressions.size());

  for (const StorableImpression& impression : stored_impressions) {
    web_ui_impressions.push_back(::mojom::WebUIImpression::New(
        impression.impression_data(), impression.impression_origin(),
        impression.conversion_origin(), impression.reporting_origin(),
        impression.impression_time().ToJsTime(),
        impression.expiry_time().ToJsTime(),
        SourceTypeToMojoType(impression.source_type()), impression.priority(),
        impression.dedup_keys()));
  }

  std::move(web_ui_callback).Run(std::move(web_ui_impressions));
}

::mojom::WebUIConversionReportPtr WebUIConversionReport(
    const ConversionReport& report,
    int http_response_code) {
  return ::mojom::WebUIConversionReport::New(
      report.impression.conversion_origin(), report.ReportURL(),
      report.report_time.ToJsTime(), report.priority,
      report.ReportBody(/*pretty_print=*/true), http_response_code);
}

void ForwardReportsToWebUI(
    ::mojom::ConversionInternalsHandler::GetSentAndPendingReportsCallback
        web_ui_callback,
    std::vector<::mojom::WebUIConversionReportPtr> sent_reports,
    std::vector<ConversionReport> stored_reports) {
  std::vector<::mojom::WebUIConversionReportPtr> web_ui_reports;
  web_ui_reports.reserve(stored_reports.size());

  for (const ConversionReport& report : stored_reports) {
    web_ui_reports.push_back(
        WebUIConversionReport(report, /*http_response_code=*/0));
  }
  std::move(web_ui_callback)
      .Run(std::move(sent_reports), std::move(web_ui_reports));
}

}  // namespace

ConversionInternalsHandlerImpl::ConversionInternalsHandlerImpl(
    WebUI* web_ui,
    mojo::PendingReceiver<::mojom::ConversionInternalsHandler> receiver)
    : web_ui_(web_ui),
      manager_provider_(std::make_unique<ConversionManagerProviderImpl>()),
      receiver_(this, std::move(receiver)) {}

ConversionInternalsHandlerImpl::~ConversionInternalsHandlerImpl() = default;

void ConversionInternalsHandlerImpl::IsMeasurementEnabled(
    ::mojom::ConversionInternalsHandler::IsMeasurementEnabledCallback
        callback) {
  content::WebContents* contents = web_ui_->GetWebContents();
  bool measurement_enabled =
      manager_provider_->GetManager(contents) &&
      GetContentClient()->browser()->IsConversionMeasurementOperationAllowed(
          contents->GetBrowserContext(),
          ContentBrowserClient::ConversionMeasurementOperation::kAny,
          /*impression_origin=*/nullptr, /*conversion_origin=*/nullptr,
          /*reporting_origin=*/nullptr);
  bool debug_mode = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kConversionsDebugMode);
  std::move(callback).Run(measurement_enabled, debug_mode);
}

void ConversionInternalsHandlerImpl::GetActiveImpressions(
    ::mojom::ConversionInternalsHandler::GetActiveImpressionsCallback
        callback) {
  if (ConversionManager* manager =
          manager_provider_->GetManager(web_ui_->GetWebContents())) {
    manager->GetActiveImpressionsForWebUI(
        base::BindOnce(&ForwardImpressionsToWebUI, std::move(callback)));
  } else {
    std::move(callback).Run({});
  }
}

void ConversionInternalsHandlerImpl::GetSentAndPendingReports(
    ::mojom::ConversionInternalsHandler::GetSentAndPendingReportsCallback
        callback) {
  if (ConversionManager* manager =
          manager_provider_->GetManager(web_ui_->GetWebContents())) {
    const base::circular_deque<SentReportInfo>& sent_reports =
        manager->GetSentReportsForWebUI();
    std::vector<::mojom::WebUIConversionReportPtr> web_ui_sent_reports;
    web_ui_sent_reports.reserve(sent_reports.size());
    for (const SentReportInfo& info : sent_reports) {
      web_ui_sent_reports.push_back(
          WebUIConversionReport(info.report, info.http_response_code));
    }

    manager->GetPendingReportsForWebUI(
        base::BindOnce(&ForwardReportsToWebUI, std::move(callback),
                       std::move(web_ui_sent_reports)),
        base::Time::Max());
  } else {
    std::move(callback).Run({}, {});
  }
}

void ConversionInternalsHandlerImpl::SendPendingReports(
    ::mojom::ConversionInternalsHandler::SendPendingReportsCallback callback) {
  if (ConversionManager* manager =
          manager_provider_->GetManager(web_ui_->GetWebContents())) {
    manager->SendReportsForWebUI(std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

void ConversionInternalsHandlerImpl::ClearStorage(
    ::mojom::ConversionInternalsHandler::ClearStorageCallback callback) {
  if (ConversionManager* manager =
          manager_provider_->GetManager(web_ui_->GetWebContents())) {
    manager->ClearData(base::Time::Min(), base::Time::Max(),
                       base::NullCallback(), std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

void ConversionInternalsHandlerImpl::SetConversionManagerProviderForTesting(
    std::unique_ptr<ConversionManager::Provider> manager_provider) {
  manager_provider_ = std::move(manager_provider);
}

}  // namespace content
