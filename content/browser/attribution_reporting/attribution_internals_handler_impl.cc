// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_internals_handler_impl.h"

#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_source.h"
#include "content/browser/attribution_reporting/attribution_info.h"
#include "content/browser/attribution_reporting/attribution_manager_provider.h"
#include "content/browser/attribution_reporting/attribution_observer_types.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_reporting.pb.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/attribution_utils.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/send_result.h"
#include "content/browser/attribution_reporting/stored_source.h"
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
    ::attribution_internals::mojom::WebUISource::Attributability;

base::flat_map<std::string, attribution_internals::mojom::AggregatableKeyPtr>
Convert(const AttributionAggregatableSource& aggregatable_source) {
  const proto::AttributionAggregatableSource& proto =
      aggregatable_source.proto();

  base::flat_map<std::string, attribution_internals::mojom::AggregatableKeyPtr>
      map;
  for (const auto& [key_id, key] : proto.keys()) {
    // TODO(linnan): Replacing with 128-bit value string.
    map.emplace(key_id, attribution_internals::mojom::AggregatableKey::New(
                            key.high_bits(), key.low_bits()));
  }
  return map;
}

attribution_internals::mojom::WebUISourcePtr WebUISource(
    const CommonSourceInfo& source,
    Attributability attributability,
    const std::vector<uint64_t>& dedup_keys) {
  return attribution_internals::mojom::WebUISource::New(
      source.source_event_id(), source.impression_origin(),
      source.ConversionDestination().Serialize(), source.reporting_origin(),
      source.impression_time().ToJsTime(), source.expiry_time().ToJsTime(),
      source.source_type(), source.priority(),
      source.debug_key()
          ? attribution_internals::mojom::DebugKey::New(*source.debug_key())
          : nullptr,
      dedup_keys, source.filter_data().filter_values(),
      Convert(source.aggregatable_source()), attributability);
}

void ForwardSourcesToWebUI(
    attribution_internals::mojom::Handler::GetActiveSourcesCallback
        web_ui_callback,
    std::vector<StoredSource> active_sources) {
  std::vector<attribution_internals::mojom::WebUISourcePtr> web_ui_sources;
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

    web_ui_sources.push_back(WebUISource(source.common_info(), attributability,
                                         source.dedup_keys()));
  }

  std::move(web_ui_callback).Run(std::move(web_ui_sources));
}

attribution_internals::mojom::WebUIReportPtr WebUIReport(
    const AttributionReport& report,
    bool is_debug_report,
    int http_response_code,
    attribution_internals::mojom::WebUIReport::Status status) {
  struct Visitor {
    StoredSource::AttributionLogic attribution_logic;

    attribution_internals::mojom::WebUIReportDataPtr operator()(
        const AttributionReport::EventLevelData& event_level_data) {
      return attribution_internals::mojom::WebUIReportData::NewEventLevelData(
          attribution_internals::mojom::WebUIReportEventLevelData::New(
              event_level_data.id, event_level_data.priority,
              attribution_logic ==
                  StoredSource::AttributionLogic::kTruthfully));
    }

    attribution_internals::mojom::WebUIReportDataPtr operator()(
        const AttributionReport::AggregatableAttributionData&
            aggregatable_data) {
      std::vector<
          attribution_internals::mojom::AggregatableHistogramContributionPtr>
          contributions;
      base::ranges::transform(
          aggregatable_data.contributions, std::back_inserter(contributions),
          [](const auto& contribution) {
            return attribution_internals::mojom::
                AggregatableHistogramContribution::New(
                    attribution_internals::mojom::AggregatableKey::New(
                        absl::Uint128High64(contribution.key()),
                        absl::Uint128Low64(contribution.key())),
                    contribution.value());
          });
      return attribution_internals::mojom::WebUIReportData::
          NewAggregatableAttributionData(
              attribution_internals::mojom::
                  WebUIReportAggregatableAttributionData::New(
                      aggregatable_data.id, std::move(contributions)));
    }
  };

  const AttributionInfo& attribution_info = report.attribution_info();

  attribution_internals::mojom::WebUIReportDataPtr data = absl::visit(
      Visitor{.attribution_logic = attribution_info.source.attribution_logic()},
      report.data());
  return attribution_internals::mojom::WebUIReport::New(
      report.ReportURL(is_debug_report),
      /*trigger_time=*/attribution_info.time.ToJsTime(),
      /*report_time=*/report.report_time().ToJsTime(),
      SerializeAttributionJson(report.ReportBody(), /*pretty_print=*/true),
      status, http_response_code, std::move(data));
}

void ForwardReportsToWebUI(
    attribution_internals::mojom::Handler::GetReportsCallback web_ui_callback,
    std::vector<AttributionReport> pending_reports) {
  std::vector<attribution_internals::mojom::WebUIReportPtr> web_ui_reports;
  web_ui_reports.reserve(pending_reports.size());
  for (const AttributionReport& report : pending_reports) {
    web_ui_reports.push_back(WebUIReport(
        report, /*is_debug_report=*/false, /*http_response_code=*/0,
        attribution_internals::mojom::WebUIReport::Status::kPending));
  }

  std::move(web_ui_callback).Run(std::move(web_ui_reports));
}

}  // namespace

AttributionInternalsHandlerImpl::AttributionInternalsHandlerImpl(
    WebUI* web_ui,
    mojo::PendingReceiver<attribution_internals::mojom::Handler> receiver)
    : web_ui_(web_ui),
      manager_provider_(AttributionManagerProvider::Default()),
      receiver_(this, std::move(receiver)) {}

AttributionInternalsHandlerImpl::~AttributionInternalsHandlerImpl() = default;

void AttributionInternalsHandlerImpl::IsAttributionReportingEnabled(
    attribution_internals::mojom::Handler::IsAttributionReportingEnabledCallback
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
    attribution_internals::mojom::Handler::GetActiveSourcesCallback callback) {
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
    attribution_internals::mojom::Handler::GetReportsCallback callback) {
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
    attribution_internals::mojom::Handler::SendEventLevelReportsCallback
        callback) {
  SendReports(std::vector<AttributionReport::Id>(ids.begin(), ids.end()),
              std::move(callback));
}

void AttributionInternalsHandlerImpl::SendAggregatableAttributionReports(
    const std::vector<AttributionReport::AggregatableAttributionData::Id>& ids,
    attribution_internals::mojom::Handler::
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
    attribution_internals::mojom::Handler::ClearStorageCallback callback) {
  if (AttributionManager* manager =
          manager_provider_->GetManager(web_ui_->GetWebContents())) {
    manager->ClearData(base::Time::Min(), base::Time::Max(),
                       base::NullCallback(), std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

void AttributionInternalsHandlerImpl::AddObserver(
    mojo::PendingRemote<attribution_internals::mojom::Observer> observer,
    attribution_internals::mojom::Handler::AddObserverCallback callback) {
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

  auto source =
      WebUISource(deactivated_source.source.common_info(), attributability,
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

  auto web_ui_source =
      WebUISource(source.common_info(), attributability, /*dedup_keys=*/{});

  for (auto& observer : observers_) {
    observer->OnSourceRejectedOrDeactivated(web_ui_source.Clone());
  }
}

void AttributionInternalsHandlerImpl::OnReportSent(
    const AttributionReport& report,
    bool is_debug_report,
    const SendResult& info) {
  attribution_internals::mojom::WebUIReport::Status status;
  switch (info.status) {
    case SendResult::Status::kSent:
      status = attribution_internals::mojom::WebUIReport::Status::kSent;
      break;
    case SendResult::Status::kDropped:
      status = attribution_internals::mojom::WebUIReport::Status::
          kProhibitedByBrowserPolicy;
      break;
    case SendResult::Status::kFailure:
    case SendResult::Status::kTransientFailure:
      status = attribution_internals::mojom::WebUIReport::Status::kNetworkError;
      break;
    case SendResult::Status::kFailedToAssemble:
      status =
          attribution_internals::mojom::WebUIReport::Status::kFailedToAssemble;
      break;
  }

  auto web_report =
      WebUIReport(report, is_debug_report, info.http_response_code, status);

  for (auto& observer : observers_) {
    observer->OnReportSent(web_report.Clone());
  }
}

namespace {

absl::optional<attribution_internals::mojom::WebUIReport::Status>
GetDroppedReportStatus(AttributionTrigger::EventLevelResult status) {
  switch (status) {
    case AttributionTrigger::EventLevelResult::kSuccessDroppedLowerPriority:
    case AttributionTrigger::EventLevelResult::kPriorityTooLow:
      return attribution_internals::mojom::WebUIReport::Status::
          kDroppedDueToLowPriority;
    case AttributionTrigger::EventLevelResult::kDroppedForNoise:
      return attribution_internals::mojom::WebUIReport::Status::
          kDroppedForNoise;
    case AttributionTrigger::EventLevelResult::kExcessiveAttributions:
      return attribution_internals::mojom::WebUIReport::Status::
          kDroppedDueToExcessiveAttributions;
    case AttributionTrigger::EventLevelResult::kExcessiveReportingOrigins:
      return attribution_internals::mojom::WebUIReport::Status::
          kDroppedDueToExcessiveReportingOrigins;
    case AttributionTrigger::EventLevelResult::kDeduplicated:
      return attribution_internals::mojom::WebUIReport::Status::kDeduplicated;
    case AttributionTrigger::EventLevelResult::
        kNoCapacityForConversionDestination:
      return attribution_internals::mojom::WebUIReport::Status::
          kNoReportCapacityForDestinationSite;
    case AttributionTrigger::EventLevelResult::kNoMatchingSourceFilterData:
      return attribution_internals::mojom::WebUIReport::Status::
          kNoMatchingSourceFilterData;
    case AttributionTrigger::EventLevelResult::kInternalError:
      return attribution_internals::mojom::WebUIReport::Status::kInternalError;
    case AttributionTrigger::EventLevelResult::kSuccess:
    case AttributionTrigger::EventLevelResult::kNoMatchingImpressions:
      // TODO(apaseltiner): Surface `kNoMatchingImpressions` in internals UI.
      return absl::nullopt;
  }
}

absl::optional<attribution_internals::mojom::WebUIReport::Status>
GetDroppedReportStatus(AttributionTrigger::AggregatableResult status) {
  switch (status) {
    case AttributionTrigger::AggregatableResult::kExcessiveAttributions:
      return attribution_internals::mojom::WebUIReport::Status::
          kDroppedDueToExcessiveAttributions;
    case AttributionTrigger::AggregatableResult::kExcessiveReportingOrigins:
      return attribution_internals::mojom::WebUIReport::Status::
          kDroppedDueToExcessiveReportingOrigins;
    case AttributionTrigger::AggregatableResult::
        kNoCapacityForConversionDestination:
      return attribution_internals::mojom::WebUIReport::Status::
          kNoReportCapacityForDestinationSite;
    case AttributionTrigger::AggregatableResult::kNoMatchingSourceFilterData:
      return attribution_internals::mojom::WebUIReport::Status::
          kNoMatchingSourceFilterData;
    case AttributionTrigger::AggregatableResult::kInsufficientBudget:
      return attribution_internals::mojom::WebUIReport::Status::
          kInsufficientAggregatableBudget;
    case AttributionTrigger::AggregatableResult::kInternalError:
      return attribution_internals::mojom::WebUIReport::Status::kInternalError;
    case AttributionTrigger::AggregatableResult::kSuccess:
    case AttributionTrigger::AggregatableResult::kNoHistograms:
    case AttributionTrigger::AggregatableResult::kNoMatchingImpressions:
    case AttributionTrigger::AggregatableResult::kNotRegistered:
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

      absl::optional<attribution_internals::mojom::WebUIReport::Status>
      operator()(const AttributionReport::EventLevelData&) {
        return GetDroppedReportStatus(result.event_level_status());
      }

      absl::optional<attribution_internals::mojom::WebUIReport::Status>
      operator()(const AttributionReport::AggregatableAttributionData&) {
        return GetDroppedReportStatus(result.aggregatable_status());
      }
    };

    absl::optional<attribution_internals::mojom::WebUIReport::Status> status =
        absl::visit(Visitor{.result = result}, dropped_report.data());
    DCHECK(status.has_value());

    auto report = WebUIReport(dropped_report, /*is_debug_report=*/false,
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
