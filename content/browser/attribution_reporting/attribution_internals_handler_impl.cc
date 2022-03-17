// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_internals_handler_impl.h"

#include <iterator>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_info.h"
#include "content/browser/attribution_reporting/attribution_manager_provider.h"
#include "content/browser/attribution_reporting/attribution_observer_types.h"
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
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace content {

namespace {

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
      dedup_keys, source.filter_data().filter_values(), attributability);
}

void ForwardSourcesToWebUI(
    mojom::AttributionInternalsHandler::GetActiveSourcesCallback
        web_ui_callback,
    std::vector<StoredSource> active_sources) {
  std::vector<mojom::WebUIAttributionSourcePtr> web_ui_sources;
  web_ui_sources.reserve(active_sources.size());

  for (const StoredSource& source : active_sources) {
    Attributability attributability;
    if (source.attribution_logic() == StoredSource::AttributionLogic::kNever) {
      attributability = Attributability::kNoised;
    } else {
      switch (source.active_state()) {
        case StoredSource::ActiveState::kActive:
          attributability = Attributability::kAttributable;
          break;
        case StoredSource::ActiveState::kReachedEventLevelAttributionLimit:
          attributability = Attributability::kReachedEventLevelAttributionLimit;
          break;
        case StoredSource::ActiveState::kInactive:
          NOTREACHED();
          return;
      }
    }

    web_ui_sources.push_back(WebUIAttributionSource(
        source.common_info(), attributability, source.dedup_keys()));
  }

  std::move(web_ui_callback).Run(std::move(web_ui_sources));
}

mojom::WebUIAttributionReportPtr WebUIAttributionReport(
    const AttributionReport& report,
    bool is_debug_report,
    int http_response_code,
    mojom::WebUIAttributionReport::Status status) {
  struct Visitor {
    StoredSource::AttributionLogic attribution_logic;

    mojom::WebUIAttributionReportDataPtr operator()(
        const AttributionReport::EventLevelData& event_level_data) {
      return mojom::WebUIAttributionReportData::NewEventLevelData(
          mojom::WebUIAttributionReportEventLevelData::New(
              event_level_data.id, event_level_data.priority,
              attribution_logic ==
                  StoredSource::AttributionLogic::kTruthfully));
    }

    mojom::WebUIAttributionReportDataPtr operator()(
        const AttributionReport::AggregatableAttributionData&
            aggregatable_data) {
      std::vector<mojom::AttributionReportHistogramContributionPtr>
          contributions;
      base::ranges::transform(
          aggregatable_data.contributions, std::back_inserter(contributions),
          [](const auto& contribution) {
            return mojom::AttributionReportHistogramContribution::New(
                absl::Uint128High64(contribution.key()),
                absl::Uint128Low64(contribution.key()), contribution.value());
          });
      return mojom::WebUIAttributionReportData::NewAggregatableAttributionData(
          mojom::WebUIAttributionReportAggregatableAttributionData::New(
              aggregatable_data.id, std::move(contributions)));
    }
  };

  const AttributionInfo& attribution_info = report.attribution_info();

  mojom::WebUIAttributionReportDataPtr data = absl::visit(
      Visitor{.attribution_logic = attribution_info.source.attribution_logic()},
      report.data());
  return mojom::WebUIAttributionReport::New(
      attribution_info.source.common_info().ConversionDestination().Serialize(),
      report.ReportURL(is_debug_report),
      /*trigger_time=*/attribution_info.time.ToJsTime(),
      /*report_time=*/report.report_time().ToJsTime(),
      SerializeAttributionJson(report.ReportBody(), /*pretty_print=*/true),
      status, http_response_code, std::move(data));
}

void ForwardReportsToWebUI(
    mojom::AttributionInternalsHandler::GetReportsCallback web_ui_callback,
    std::vector<AttributionReport> pending_reports) {
  std::vector<mojom::WebUIAttributionReportPtr> web_ui_reports;
  web_ui_reports.reserve(pending_reports.size());
  for (const AttributionReport& report : pending_reports) {
    web_ui_reports.push_back(WebUIAttributionReport(
        report, /*is_debug_report=*/false, /*http_response_code=*/0,
        mojom::WebUIAttributionReport::Status::kPending));
  }

  std::move(web_ui_callback).Run(std::move(web_ui_reports));
}

}  // namespace

AttributionInternalsHandlerImpl::AttributionInternalsHandlerImpl(
    WebUI* web_ui,
    mojo::PendingReceiver<mojom::AttributionInternalsHandler> receiver)
    : web_ui_(web_ui),
      manager_provider_(AttributionManagerProvider::Default()),
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
    AttributionReport::ReportType report_type,
    mojom::AttributionInternalsHandler::GetReportsCallback callback) {
  if (AttributionManager* manager =
          manager_provider_->GetManager(web_ui_->GetWebContents())) {
    manager->GetPendingReportsForInternalUse(
        report_type,
        base::BindOnce(&ForwardReportsToWebUI, std::move(callback)));
  } else {
    std::move(callback).Run({});
  }
}

void AttributionInternalsHandlerImpl::SendEventLevelReports(
    const std::vector<AttributionReport::EventLevelData::Id>& ids,
    mojom::AttributionInternalsHandler::SendEventLevelReportsCallback
        callback) {
  SendReports(std::vector<AttributionReport::Id>(ids.begin(), ids.end()),
              std::move(callback));
}

void AttributionInternalsHandlerImpl::SendAggregatableAttributionReports(
    const std::vector<AttributionReport::AggregatableAttributionData::Id>& ids,
    mojom::AttributionInternalsHandler::
        SendAggregatableAttributionReportsCallback callback) {
  SendReports(std::vector<AttributionReport::Id>(ids.begin(), ids.end()),
              std::move(callback));
}

void AttributionInternalsHandlerImpl::SendReports(
    const std::vector<AttributionReport::Id> ids,
    base::OnceClosure callback) {
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

void AttributionInternalsHandlerImpl::OnReportsChanged(
    AttributionReport::ReportType report_type) {
  for (auto& observer : observers_)
    observer->OnReportsChanged(report_type);
}

void AttributionInternalsHandlerImpl::OnSourceDeactivated(
    const DeactivatedSource& deactivated_source) {
  Attributability attributability;
  switch (deactivated_source.reason) {
    case DeactivatedSource::Reason::kReplacedByNewerSource:
      attributability = Attributability::kReplacedByNewerSource;
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
    bool is_debug_report,
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
    case SendResult::Status::kTransientFailure:
      status = mojom::WebUIAttributionReport::Status::kNetworkError;
      break;
    case SendResult::Status::kFailedToAssemble:
      status = mojom::WebUIAttributionReport::Status::kFailedToAssemble;
      break;
  }

  auto web_report = WebUIAttributionReport(report, is_debug_report,
                                           info.http_response_code, status);

  for (auto& observer : observers_) {
    observer->OnReportSent(web_report.Clone());
  }
}

namespace {

absl::optional<mojom::WebUIAttributionReport::Status> GetDroppedReportStatus(
    AttributionTrigger::EventLevelResult status) {
  switch (status) {
    case AttributionTrigger::EventLevelResult::kSuccessDroppedLowerPriority:
    case AttributionTrigger::EventLevelResult::kPriorityTooLow:
      return mojom::WebUIAttributionReport::Status::kDroppedDueToLowPriority;
    case AttributionTrigger::EventLevelResult::kDroppedForNoise:
      return mojom::WebUIAttributionReport::Status::kDroppedForNoise;
    case AttributionTrigger::EventLevelResult::kExcessiveAttributions:
      return mojom::WebUIAttributionReport::Status::
          kDroppedDueToExcessiveAttributions;
    case AttributionTrigger::EventLevelResult::kExcessiveReportingOrigins:
      return mojom::WebUIAttributionReport::Status::
          kDroppedDueToExcessiveReportingOrigins;
    case AttributionTrigger::EventLevelResult::kDeduplicated:
      return mojom::WebUIAttributionReport::Status::kDeduplicated;
    case AttributionTrigger::EventLevelResult::
        kNoCapacityForConversionDestination:
      return mojom::WebUIAttributionReport::Status::
          kNoReportCapacityForDestinationSite;
    case AttributionTrigger::EventLevelResult::kNoMatchingSourceFilterData:
      return mojom::WebUIAttributionReport::Status::kNoMatchingSourceFilterData;
    case AttributionTrigger::EventLevelResult::kInternalError:
      return mojom::WebUIAttributionReport::Status::kInternalError;
    case AttributionTrigger::EventLevelResult::kSuccess:
    case AttributionTrigger::EventLevelResult::kNoMatchingImpressions:
      // TODO(apaseltiner): Surface `kNoMatchingImpressions` in internals UI.
      return absl::nullopt;
  }
}

absl::optional<mojom::WebUIAttributionReport::Status> GetDroppedReportStatus(
    AttributionTrigger::AggregatableResult status) {
  switch (status) {
    case AttributionTrigger::AggregatableResult::kExcessiveAttributions:
      return mojom::WebUIAttributionReport::Status::
          kDroppedDueToExcessiveAttributions;
    case AttributionTrigger::AggregatableResult::kExcessiveReportingOrigins:
      return mojom::WebUIAttributionReport::Status::
          kDroppedDueToExcessiveReportingOrigins;
    case AttributionTrigger::AggregatableResult::
        kNoCapacityForConversionDestination:
      return mojom::WebUIAttributionReport::Status::
          kNoReportCapacityForDestinationSite;
    case AttributionTrigger::AggregatableResult::kNoMatchingSourceFilterData:
      return mojom::WebUIAttributionReport::Status::kNoMatchingSourceFilterData;
    case AttributionTrigger::AggregatableResult::kInsufficientBudget:
      return mojom::WebUIAttributionReport::Status::
          kInsufficientAggregatableBudget;
    case AttributionTrigger::AggregatableResult::kInternalError:
      return mojom::WebUIAttributionReport::Status::kInternalError;
    case AttributionTrigger::AggregatableResult::kSuccess:
    case AttributionTrigger::AggregatableResult::kNoHistograms:
    case AttributionTrigger::AggregatableResult::kNoMatchingImpressions:
      // TODO(apaseltiner): Surface `kNoMatchingImpressions` in internals UI.
      // TODO(linnan): Surface `kNoHistograms` in internals UI.
      return absl::nullopt;
  }
}

}  // namespace

void AttributionInternalsHandlerImpl::OnTriggerHandled(
    const CreateReportResult& result) {
  for (const AttributionReport& dropped_report : result.dropped_reports()) {
    struct Visitor {
      const CreateReportResult& result;

      absl::optional<mojom::WebUIAttributionReport::Status> operator()(
          const AttributionReport::EventLevelData&) {
        return GetDroppedReportStatus(result.event_level_status());
      }

      absl::optional<mojom::WebUIAttributionReport::Status> operator()(
          const AttributionReport::AggregatableAttributionData&) {
        return GetDroppedReportStatus(result.aggregatable_status());
      }
    };

    absl::optional<mojom::WebUIAttributionReport::Status> status =
        absl::visit(Visitor{.result = result}, dropped_report.data());
    DCHECK(status.has_value());

    auto report =
        WebUIAttributionReport(dropped_report, /*is_debug_report=*/false,
                               /*http_response_code=*/0, *status);

    for (auto& observer : observers_) {
      observer->OnReportDropped(report.Clone());
    }
  }
}

void AttributionInternalsHandlerImpl::SetAttributionManagerProviderForTesting(
    std::unique_ptr<AttributionManagerProvider> manager_provider) {
  DCHECK(manager_provider);

  manager_observation_.Reset();
  manager_provider_ = std::move(manager_provider);

  if (AttributionManager* manager =
          manager_provider_->GetManager(web_ui_->GetWebContents())) {
    manager_observation_.Observe(manager);
  }
}

}  // namespace content
