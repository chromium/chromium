// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_internals_handler_impl.h"

#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/time/time.h"
#include "content/browser/conversions/conversion_manager_impl.h"
#include "content/browser/conversions/conversion_report.h"
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
        impression.expiry_time().ToJsTime()));
  }

  std::move(web_ui_callback).Run(std::move(web_ui_impressions));
}

void ForwardReportsToWebUI(
    ::mojom::ConversionInternalsHandler::GetPendingReportsCallback
        web_ui_callback,
    std::vector<ConversionReport> stored_reports) {
  std::vector<::mojom::WebUIConversionReportPtr> web_ui_reports;
  web_ui_reports.reserve(stored_reports.size());

  for (const ConversionReport& report : stored_reports) {
    web_ui_reports.push_back(::mojom::WebUIConversionReport::New(
        report.impression.impression_data(), report.conversion_data,
        report.impression.conversion_origin(),
        report.impression.reporting_origin(), report.report_time.ToJsTime(),
        report.attribution_credit));
  }
  std::move(web_ui_callback).Run(std::move(web_ui_reports));
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
      GetContentClient()->browser()->IsConversionMeasurementAllowed(
          contents->GetBrowserContext());
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

void ConversionInternalsHandlerImpl::GetPendingReports(
    ::mojom::ConversionInternalsHandler::GetPendingReportsCallback callback) {
  if (ConversionManager* manager =
          manager_provider_->GetManager(web_ui_->GetWebContents())) {
    manager->GetReportsForWebUI(
        base::BindOnce(&ForwardReportsToWebUI, std::move(callback)),
        base::Time::Max());
  } else {
    std::move(callback).Run({});
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
