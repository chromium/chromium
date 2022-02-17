// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_internals_handler_impl.h"

#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_info.h"
#include "content/browser/attribution_reporting/attribution_manager_impl.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/attribution_utils.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/send_result.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace content {

namespace {

using DeactivatedSource = ::content::AttributionStorage::DeactivatedSource;

using Attributability =
    ::content::mojom::WebUIAttributionSource::Attributability;

mojom::WebUIAttributionSourcePtr WebUIAttributionSource(
    const CommonSourceInfo& source,
    Attributability attributability,
    const std::vector<uint64_t>& dedup_keys) {
  return mojom::WebUIAttributionSource::New(
      source.source_event_id(), source.impression_origin(),
      source.ConversionDestination().Serialize(), source.reporting_origin(),
      source.impression_time().ToJsTime(), source.expiry_time().ToJsTime(),
      source.source_type(), source.priority(),
      source.debug_key() ? mojom::AttributionDebugKey::New(*source.debug_key())
                         : nullptr,
      dedup_keys, attributability);
}

void ForwardSourcesToWebUI(
    mojom::AttributionInternalsHandler::GetActiveSourcesCallback
        web_ui_callback,
    std::vector<StoredSource> active_sources) {
  std::vector<mojom::WebUIAttributionSourcePtr> web_ui_sources;
  web_ui_sources.reserve(active_sources.size());

  for (const StoredSource& source : active_sources) {
    auto attributability =
        source.attribution_logic() == StoredSource::AttributionLogic::kNever
            ? Attributability::kNoised
            : Attributability::kAttributable;

    web_ui_sources.push_back(WebUIAttributionSource(
        source.common_info(), attributability, source.dedup_keys()));
  }

  std::move(web_ui_callback).Run(std::move(web_ui_sources));
}

mojom::WebUIAttributionReportPtr WebUIAttributionReport(
    const AttributionReport& report,
    int http_response_code,
    mojom::WebUIAttributionReport::Status status) {
  const auto* data =
      absl::get_if<AttributionReport::EventLevelData>(&report.data());
  DCHECK(data);
  const AttributionInfo& attribution_info = report.attribution_info();
  return mojom::WebUIAttributionReport::New(
      data->id,
      attribution_info.source.common_info().ConversionDestination().Serialize(),
      report.ReportURL(),
      /*trigger_time=*/attribution_info.time.ToJsTime(),
      /*report_time=*/report.report_time().ToJsTime(), data->priority,
      SerializeAttributionJson(report.ReportBody(), /*pretty_print=*/true),
      /*attributed_truthfully=*/
      attribution_info.source.attribution_logic() ==
          StoredSource::AttributionLogic::kTruthfully,
      status, http_response_code);
}

void ForwardReportsToWebUI(
    mojom::AttributionInternalsHandler::GetReportsCallback web_ui_callback,
    std::vector<AttributionReport> pending_reports) {
  std::vector<mojom::WebUIAttributionReportPtr> web_ui_reports;
  web_ui_reports.reserve(pending_reports.size());
  for (const AttributionReport& report : pending_reports) {
    web_ui_reports.push_back(WebUIAttributionReport(
        report, /*http_response_code=*/0,
        mojom::WebUIAttributionReport::Status::kPending));
  }

  std::move(web_ui_callback).Run(std::move(web_ui_reports));
}

}  // namespace

AttributionInternalsHandlerImpl::AttributionInternalsHandlerImpl(
    WebUI* web_ui,
    mojo::PendingReceiver<mojom::AttributionInternalsHandler> receiver)
    : web_ui_(web_ui),
      manager_provider_(std::make_unique<AttributionManagerProviderImpl>()),
      receiver_(this, std::move(receiver)) {}

AttributionInternalsHandlerImpl::~AttributionInternalsHandlerImpl() = default;

void AttributionInternalsHandlerImpl::IsAttributionReportingEnabled(
    mojom::AttributionInternalsHandler::IsAttributionReportingEnabledCallback
        callback) {
  content::WebContents* contents = web_ui_->GetWebContents();
  bool attribution_reporting_enabled =
      manager_provider_->GetManager(contents) &&
      GetContentClient()->browser()->IsConversionMeasurementOperationAllowed(
          contents->GetBrowserContext(),
          ContentBrowserClient::ConversionMeasurementOperation::kAny,
          /*impression_origin=*/nullptr, /*conversion_origin=*/nullptr,
          /*reporting_origin=*/nullptr);
  bool debug_mode = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kConversionsDebugMode);
  std::move(callback).Run(attribution_reporting_enabled, debug_mode);
}

void AttributionInternalsHandlerImpl::GetActiveSources(
    mojom::AttributionInternalsHandler::GetActiveSourcesCallback callback) {
  if (AttributionManager* manager =
          manager_provider_->GetManager(web_ui_->GetWebContents())) {
    manager->GetActiveSourcesForWebUI(
        base::BindOnce(&ForwardSourcesToWebUI, std::move(callback)));
  } else {
    std::move(callback).Run({});
  }
}

void AttributionInternalsHandlerImpl::GetReports(
    mojom::AttributionInternalsHandler::GetReportsCallback callback) {
  if (AttributionManager* manager =
          manager_provider_->GetManager(web_ui_->GetWebContents())) {
    manager->GetPendingReportsForInternalUse(
        base::BindOnce(&ForwardReportsToWebUI, std::move(callback)));
  } else {
    std::move(callback).Run({});
  }
}

void AttributionInternalsHandlerImpl::SendReports(
    const std::vector<AttributionReport::EventLevelData::Id>& ids,
    mojom::AttributionInternalsHandler::SendReportsCallback callback) {
  if (AttributionManager* manager =
          manager_provider_->GetManager(web_ui_->GetWebContents())) {
    manager->SendReportsForWebUI(ids, std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

void AttributionInternalsHandlerImpl::ClearStorage(
    mojom::AttributionInternalsHandler::ClearStorageCallback callback) {
  if (AttributionManager* manager =
          manager_provider_->GetManager(web_ui_->GetWebContents())) {
    manager->ClearData(base::Time::Min(), base::Time::Max(),
                       base::NullCallback(), std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

void AttributionInternalsHandlerImpl::AddObserver(
    mojo::PendingRemote<mojom::AttributionInternalsObserver> observer,
    mojom::AttributionInternalsHandler::AddObserverCallback callback) {
  if (AttributionManager* manager =
          manager_provider_->GetManager(web_ui_->GetWebContents())) {
    observers_.Add(std::move(observer));

    if (!manager_observation_.IsObservingSource(manager))
      manager_observation_.Observe(manager);

    std::move(callback).Run(true);
  } else {
    std::move(callback).Run(false);
  }
}

void AttributionInternalsHandlerImpl::OnSourcesChanged() {
  for (auto& observer : observers_)
    observer->OnSourcesChanged();
}

void AttributionInternalsHandlerImpl::OnReportsChanged() {
  for (auto& observer : observers_)
    observer->OnReportsChanged();
}

void AttributionInternalsHandlerImpl::OnSourceDeactivated(
    const AttributionStorage::DeactivatedSource& deactivated_source) {
  Attributability attributability;
  switch (deactivated_source.reason) {
    case DeactivatedSource::Reason::kReplacedByNewerSource:
      attributability = Attributability::kReplacedByNewerSource;
      break;
    case DeactivatedSource::Reason::kReachedAttributionLimit:
      attributability = Attributability::kReachedAttributionLimit;
      break;
  }

  auto source = WebUIAttributionSource(deactivated_source.source.common_info(),
                                       attributability,
                                       deactivated_source.source.dedup_keys());

  for (auto& observer : observers_) {
    observer->OnSourceRejectedOrDeactivated(source.Clone());
  }
}

void AttributionInternalsHandlerImpl::OnSourceHandled(
    const StorableSource& source,
    StorableSource::Result result) {
  Attributability attributability;
  switch (result) {
    case StorableSource::Result::kSuccess:
      return;
    case StorableSource::Result::kInternalError:
      attributability = Attributability::kInternalError;
      break;
    case StorableSource::Result::kInsufficientSourceCapacity:
      attributability = Attributability::kInsufficientSourceCapacity;
      break;
    case StorableSource::Result::kInsufficientUniqueDestinationCapacity:
      attributability = Attributability::kInsufficientUniqueDestinationCapacity;
      break;
    case StorableSource::Result::kExcessiveReportingOrigins:
      attributability = Attributability::kExcessiveReportingOrigins;
      break;
  }

  auto web_ui_source = WebUIAttributionSource(
      source.common_info(), attributability, /*dedup_keys=*/{});

  for (auto& observer : observers_) {
    observer->OnSourceRejectedOrDeactivated(web_ui_source.Clone());
  }
}

void AttributionInternalsHandlerImpl::OnReportSent(
    const AttributionReport& report,
    const SendResult& info) {
  mojom::WebUIAttributionReport::Status status;
  switch (info.status) {
    case SendResult::Status::kSent:
      status = mojom::WebUIAttributionReport::Status::kSent;
      break;
    case SendResult::Status::kDropped:
      status =
          mojom::WebUIAttributionReport::Status::kProhibitedByBrowserPolicy;
      break;
    case SendResult::Status::kFailure:
      status = mojom::WebUIAttributionReport::Status::kNetworkError;
      break;
    case SendResult::Status::kTransientFailure:
      NOTREACHED();
      return;
  }

  auto web_report =
      WebUIAttributionReport(report, info.http_response_code, status);

  for (auto& observer : observers_) {
    observer->OnReportSent(web_report.Clone());
  }
}

void AttributionInternalsHandlerImpl::OnTriggerHandled(
    const AttributionStorage::CreateReportResult& result) {
  mojom::WebUIAttributionReport::Status status;
  switch (result.status()) {
    case AttributionTrigger::Result::kSuccessDroppedLowerPriority:
    case AttributionTrigger::Result::kPriorityTooLow:
      status = mojom::WebUIAttributionReport::Status::kDroppedDueToLowPriority;
      break;
    case AttributionTrigger::Result::kDroppedForNoise:
      status = mojom::WebUIAttributionReport::Status::kDroppedForNoise;
      break;
    case AttributionTrigger::Result::kExcessiveAttributions:
      status = mojom::WebUIAttributionReport::Status::
          kDroppedDueToExcessiveAttributions;
      break;
    case AttributionTrigger::Result::kExcessiveReportingOrigins:
      status = mojom::WebUIAttributionReport::Status::
          kDroppedDueToExcessiveReportingOrigins;
      break;
    case AttributionTrigger::Result::kDeduplicated:
      status = mojom::WebUIAttributionReport::Status::kDeduplicated;
      break;
    case AttributionTrigger::Result::kNoCapacityForConversionDestination:
      status = mojom::WebUIAttributionReport::Status::
          kNoReportCapacityForDestinationSite;
      break;
    case AttributionTrigger::Result::kInternalError:
      // `kInternalError` doesn't always have a dropped report.
      if (!result.dropped_report().has_value())
        return;

      status = mojom::WebUIAttributionReport::Status::kInternalError;
      break;
    case AttributionTrigger::Result::kSuccess:
    case AttributionTrigger::Result::kNoMatchingImpressions:
      // TODO(apaseltiner): Surface `kNoMatchingImpressions` in internals UI.
      return;
  }

  DCHECK(result.dropped_report().has_value());
  auto report = WebUIAttributionReport(*result.dropped_report(),
                                       /*http_response_code=*/0, status);

  for (auto& observer : observers_) {
    observer->OnReportDropped(report.Clone());
  }
}

void AttributionInternalsHandlerImpl::SetAttributionManagerProviderForTesting(
    std::unique_ptr<AttributionManager::Provider> manager_provider) {
  DCHECK(manager_provider);

  manager_observation_.Reset();
  manager_provider_ = std::move(manager_provider);

  if (AttributionManager* manager =
          manager_provider_->GetManager(web_ui_->GetWebContents())) {
    manager_observation_.Observe(manager);
  }
}

}  // namespace content
