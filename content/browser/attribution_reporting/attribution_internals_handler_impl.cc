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
#include "content/browser/attribution_reporting/aggregatable_attribution_utils.h"
#include "content/browser/attribution_reporting/attribution_aggregation_keys.h"
#include "content/browser/attribution_reporting/attribution_info.h"
#include "content/browser/attribution_reporting/attribution_observer_types.h"
#include "content/browser/attribution_reporting/attribution_report.h"
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
#include "net/base/net_errors.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/abseil-cpp/absl/utility/utility.h"

namespace content {

namespace {

using Attributability =
    ::attribution_internals::mojom::WebUISource::Attributability;
using Empty = ::attribution_internals::mojom::Empty;
using ReportStatus = ::attribution_internals::mojom::ReportStatus;
using ReportStatusPtr = ::attribution_internals::mojom::ReportStatusPtr;

attribution_internals::mojom::DebugKeyPtr WebUIDebugKey(
    absl::optional<uint64_t> debug_key) {
  return debug_key ? attribution_internals::mojom::DebugKey::New(*debug_key)
                   : nullptr;
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
      WebUIDebugKey(source.debug_key()), dedup_keys,
      source.filter_data().filter_values(),
      base::MakeFlatMap<std::string, std::string>(
          source.aggregation_keys().keys(), {},
          [](const auto& key) {
            return std::make_pair(key.first,
                                  HexEncodeAggregationKey(key.second));
          }),
      attributability);
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
    ReportStatusPtr status) {
  struct Visitor {
    StoredSource::AttributionLogic attribution_logic;

    attribution_internals::mojom::WebUIReportDataPtr operator()(
        const AttributionReport::EventLevelData& event_level_data) {
      return attribution_internals::mojom::WebUIReportData::NewEventLevelData(
          attribution_internals::mojom::WebUIReportEventLevelData::New(
              event_level_data.priority,
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
                    HexEncodeAggregationKey(contribution.key()),
                    contribution.value());
          });
      return attribution_internals::mojom::WebUIReportData::
          NewAggregatableAttributionData(
              attribution_internals::mojom::
                  WebUIReportAggregatableAttributionData::New(
                      std::move(contributions)));
    }
  };

  const AttributionInfo& attribution_info = report.attribution_info();

  attribution_internals::mojom::WebUIReportDataPtr data = absl::visit(
      Visitor{.attribution_logic = attribution_info.source.attribution_logic()},
      report.data());
  return attribution_internals::mojom::WebUIReport::New(
      report.ReportId(), report.ReportURL(is_debug_report),
      /*trigger_time=*/attribution_info.time.ToJsTime(),
      /*report_time=*/report.report_time().ToJsTime(),
      SerializeAttributionJson(report.ReportBody(), /*pretty_print=*/true),
      std::move(status), std::move(data));
}

void ForwardReportsToWebUI(
    attribution_internals::mojom::Handler::GetReportsCallback web_ui_callback,
    std::vector<AttributionReport> pending_reports) {
  std::vector<attribution_internals::mojom::WebUIReportPtr> web_ui_reports;
  web_ui_reports.reserve(pending_reports.size());
  for (const AttributionReport& report : pending_reports) {
    web_ui_reports.push_back(
        WebUIReport(report, /*is_debug_report=*/false,
                    ReportStatus::NewPending(Empty::New())));
  }

  std::move(web_ui_callback).Run(std::move(web_ui_reports));
}

}  // namespace

AttributionInternalsHandlerImpl::AttributionInternalsHandlerImpl(
    WebUI* web_ui,
    mojo::PendingReceiver<attribution_internals::mojom::Handler> receiver)
    : web_ui_(web_ui), receiver_(this, std::move(receiver)) {}

AttributionInternalsHandlerImpl::~AttributionInternalsHandlerImpl() = default;

void AttributionInternalsHandlerImpl::IsAttributionReportingEnabled(
    attribution_internals::mojom::Handler::IsAttributionReportingEnabledCallback
        callback) {
  content::WebContents* contents = web_ui_->GetWebContents();
  bool attribution_reporting_enabled =
      AttributionManager::FromWebContents(contents) &&
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
          AttributionManager::FromWebContents(web_ui_->GetWebContents())) {
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
          AttributionManager::FromWebContents(web_ui_->GetWebContents())) {
    manager->GetPendingReportsForInternalUse(
        AttributionReport::ReportTypes{report_type},
        /*limit=*/1000,
        base::BindOnce(&ForwardReportsToWebUI, std::move(callback)));
  } else {
    std::move(callback).Run({});
  }
}

void AttributionInternalsHandlerImpl::SendReports(
    const std::vector<AttributionReport::Id>& ids,
    attribution_internals::mojom::Handler::SendReportsCallback callback) {
  if (AttributionManager* manager =
          AttributionManager::FromWebContents(web_ui_->GetWebContents())) {
    manager->SendReportsForWebUI(ids, std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

void AttributionInternalsHandlerImpl::ClearStorage(
    attribution_internals::mojom::Handler::ClearStorageCallback callback) {
  if (AttributionManager* manager =
          AttributionManager::FromWebContents(web_ui_->GetWebContents())) {
    manager->ClearData(base::Time::Min(), base::Time::Max(),
                       base::NullCallback(),
                       /*delete_rate_limit_data=*/true, std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

void AttributionInternalsHandlerImpl::AddObserver(
    mojo::PendingRemote<attribution_internals::mojom::Observer> observer,
    attribution_internals::mojom::Handler::AddObserverCallback callback) {
  if (AttributionManager* manager =
          AttributionManager::FromWebContents(web_ui_->GetWebContents())) {
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
    const StoredSource& deactivated_source) {
  auto source = WebUISource(deactivated_source.common_info(),
                            Attributability::kReplacedByNewerSource,
                            deactivated_source.dedup_keys());

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
    case StorableSource::Result::kProhibitedByBrowserPolicy:
      attributability = Attributability::kProhibitedByBrowserPolicy;
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
  ReportStatusPtr status;
  switch (info.status) {
    case SendResult::Status::kSent:
      status = ReportStatus::NewSent(info.http_response_code);
      break;
    case SendResult::Status::kDropped:
      status = ReportStatus::NewProhibitedByBrowserPolicy(Empty::New());
      break;
    case SendResult::Status::kFailure:
    case SendResult::Status::kTransientFailure:
      status = ReportStatus::NewNetworkError(
          net::ErrorToShortString(info.network_error));
      break;
    case SendResult::Status::kFailedToAssemble:
      status = ReportStatus::NewFailedToAssemble(Empty::New());
      break;
  }

  auto web_report = WebUIReport(report, is_debug_report, std::move(status));

  for (auto& observer : observers_) {
    observer->OnReportSent(web_report.Clone());
  }
}

namespace {

using AggregatableStatus = ::content::AttributionTrigger::AggregatableResult;
using EventLevelStatus = ::content::AttributionTrigger::EventLevelResult;
using WebUITriggerStatus = ::attribution_internals::mojom::WebUITrigger::Status;

WebUITriggerStatus GetWebUITriggerStatus(EventLevelStatus status) {
  switch (status) {
    case EventLevelStatus::kSuccess:
    case EventLevelStatus::kSuccessDroppedLowerPriority:
      return WebUITriggerStatus::kSuccess;
    case EventLevelStatus::kInternalError:
      return WebUITriggerStatus::kInternalError;
    case EventLevelStatus::kNoCapacityForConversionDestination:
      return WebUITriggerStatus::kNoReportCapacityForDestinationSite;
    case EventLevelStatus::kNoMatchingImpressions:
      return WebUITriggerStatus::kNoMatchingSources;
    case EventLevelStatus::kDeduplicated:
      return WebUITriggerStatus::kDeduplicated;
    case EventLevelStatus::kExcessiveAttributions:
      return WebUITriggerStatus::kExcessiveAttributions;
    case EventLevelStatus::kPriorityTooLow:
      return WebUITriggerStatus::kLowPriority;
    case EventLevelStatus::kDroppedForNoise:
      return WebUITriggerStatus::kNoised;
    case EventLevelStatus::kExcessiveReportingOrigins:
      return WebUITriggerStatus::kExcessiveReportingOrigins;
    case EventLevelStatus::kNoMatchingSourceFilterData:
      return WebUITriggerStatus::kNoMatchingSourceFilterData;
    case EventLevelStatus::kProhibitedByBrowserPolicy:
      return WebUITriggerStatus::kProhibitedByBrowserPolicy;
    case EventLevelStatus::kNoMatchingConfigurations:
      return WebUITriggerStatus::kNoMatchingConfigurations;
  }
}

WebUITriggerStatus GetWebUITriggerStatus(AggregatableStatus status) {
  switch (status) {
    case AggregatableStatus::kSuccess:
      return WebUITriggerStatus::kSuccess;
    case AggregatableStatus::kInternalError:
      return WebUITriggerStatus::kInternalError;
    case AggregatableStatus::kNoCapacityForConversionDestination:
      return WebUITriggerStatus::kNoReportCapacityForDestinationSite;
    case AggregatableStatus::kNoMatchingImpressions:
      return WebUITriggerStatus::kNoMatchingSources;
    case AggregatableStatus::kExcessiveAttributions:
      return WebUITriggerStatus::kExcessiveAttributions;
    case AggregatableStatus::kExcessiveReportingOrigins:
      return WebUITriggerStatus::kExcessiveReportingOrigins;
    case AggregatableStatus::kNoHistograms:
      return WebUITriggerStatus::kNoHistograms;
    case AggregatableStatus::kInsufficientBudget:
      return WebUITriggerStatus::kInsufficientBudget;
    case AggregatableStatus::kNoMatchingSourceFilterData:
      return WebUITriggerStatus::kNoMatchingSourceFilterData;
    case AggregatableStatus::kNotRegistered:
      return WebUITriggerStatus::kNotRegistered;
    case AggregatableStatus::kProhibitedByBrowserPolicy:
      return WebUITriggerStatus::kProhibitedByBrowserPolicy;
  }
}

}  // namespace

void AttributionInternalsHandlerImpl::OnTriggerHandled(
    const AttributionTrigger& trigger,
    const CreateReportResult& result) {
  auto web_ui_trigger = attribution_internals::mojom::WebUITrigger::New();
  web_ui_trigger->trigger_time = result.trigger_time().ToJsTime();
  web_ui_trigger->destination_origin = trigger.destination_origin();
  web_ui_trigger->reporting_origin = trigger.reporting_origin();
  web_ui_trigger->filters = trigger.filters().filter_values();
  web_ui_trigger->not_filters = trigger.not_filters().filter_values();
  web_ui_trigger->debug_key = WebUIDebugKey(trigger.debug_key());
  web_ui_trigger->event_level_status =
      GetWebUITriggerStatus(result.event_level_status());
  web_ui_trigger->aggregatable_status =
      GetWebUITriggerStatus(result.aggregatable_status());

  for (const auto& event_trigger : trigger.event_triggers()) {
    web_ui_trigger->event_triggers.emplace_back(
        absl::in_place,
        /*data=*/event_trigger.data,
        /*priority=*/event_trigger.priority,
        /*deduplication_key=*/event_trigger.dedup_key
            ? attribution_internals::mojom::DedupKey::New(
                  *event_trigger.dedup_key)
            : nullptr,
        /*filters=*/event_trigger.filters.filter_values(),
        /*not_filters=*/event_trigger.not_filters.filter_values());
  }

  for (const auto& aggregatable_trigger_data :
       trigger.aggregatable_trigger_data()) {
    web_ui_trigger->aggregatable_triggers.emplace_back(
        absl::in_place,
        /*key_piece=*/
        HexEncodeAggregationKey(aggregatable_trigger_data.key_piece()),
        /*source_keys=*/
        std::vector<std::string>(
            aggregatable_trigger_data.source_keys().begin(),
            aggregatable_trigger_data.source_keys().end()),
        /*filters=*/aggregatable_trigger_data.filters().filter_values(),
        /*not_filters=*/
        aggregatable_trigger_data.not_filters().filter_values());
  }

  web_ui_trigger->aggregatable_values = trigger.aggregatable_values().values();

  for (auto& observer : observers_) {
    observer->OnTriggerHandled(web_ui_trigger.Clone());
  }

  if (const absl::optional<AttributionReport>& report =
          result.replaced_event_level_report()) {
    DCHECK_EQ(
        result.event_level_status(),
        AttributionTrigger::EventLevelResult::kSuccessDroppedLowerPriority);
    DCHECK(result.new_event_level_report().has_value());

    auto web_ui_report =
        WebUIReport(*report, /*is_debug_report=*/false,
                    ReportStatus::NewReplacedByHigherPriorityReport(
                        result.new_event_level_report()
                            ->external_report_id()
                            .AsLowercaseString()));

    for (auto& observer : observers_) {
      observer->OnReportDropped(web_ui_report.Clone());
    }
  }
}

}  // namespace content
