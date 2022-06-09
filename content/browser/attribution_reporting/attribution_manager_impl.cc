// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_manager_impl.h"

#include <cmath>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/time/time.h"
#include "content/browser/aggregation_service/aggregation_service.h"
#include "content/browser/aggregation_service/aggregation_service_impl.h"
#include "content/browser/attribution_reporting/aggregatable_attribution_utils.h"
#include "content/browser/attribution_reporting/attribution_cookie_checker.h"
#include "content/browser/attribution_reporting/attribution_cookie_checker_impl.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager_impl.h"
#include "content/browser/attribution_reporting/attribution_info.h"
#include "content/browser/attribution_reporting/attribution_metrics.h"
#include "content/browser/attribution_reporting/attribution_observer.h"
#include "content/browser/attribution_reporting/attribution_observer_types.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_report_network_sender.h"
#include "content/browser/attribution_reporting/attribution_report_sender.h"
#include "content/browser/attribution_reporting/attribution_storage.h"
#include "content/browser/attribution_reporting/attribution_storage_delegate.h"
#include "content/browser/attribution_reporting/attribution_storage_delegate_impl.h"
#include "content/browser/attribution_reporting/attribution_storage_sql.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/attribution_utils.h"
#include "content/browser/attribution_reporting/send_result.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/attribution_reporting.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "url/gurl.h"

namespace content {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ConversionReportSendOutcome {
  kSent = 0,
  kFailed = 1,
  kDropped = 2,
  kFailedToAssemble = 3,
  kMaxValue = kFailedToAssemble,
};

// The shared-task runner for all attribution storage operations. Note that
// different AttributionManagerImpl perform operations on the same task
// runner. This prevents any potential races when a given context is destroyed
// and recreated for the same backing storage. This uses
// BLOCK_SHUTDOWN as some data deletion operations may be running when the
// browser is closed, and we want to ensure all data is deleted correctly.
base::LazyThreadPoolSequencedTaskRunner g_storage_task_runner =
    LAZY_THREAD_POOL_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::TaskPriority::BEST_EFFORT,
                         base::MayBlock(),
                         base::TaskShutdownBehavior::BLOCK_SHUTDOWN));

bool IsOriginSessionOnly(
    scoped_refptr<storage::SpecialStoragePolicy> storage_policy,
    const url::Origin& origin) {
  // TODO(johnidel): This conversion is unfortunate but necessary. Storage
  // partition clear data logic uses Origin keyed deletion, while the storage
  // policy uses GURLs. Ideally these would be coalesced.
  const GURL& url = origin.GetURL();
  if (storage_policy->IsStorageProtected(url))
    return false;

  if (storage_policy->IsStorageSessionOnly(url))
    return true;
  return false;
}

void RecordCreateReportStatus(CreateReportResult result) {
  static_assert(
      AttributionTrigger::EventLevelResult::kMaxValue ==
          AttributionTrigger::EventLevelResult::kNoMatchingConfigurations,
      "Bump version of Conversions.CreateReportStatus3 histogram.");
  base::UmaHistogramEnumeration("Conversions.CreateReportStatus3",
                                result.event_level_status());
  base::UmaHistogramEnumeration(
      "Conversions.AggregatableReport.CreateReportStatus2",
      result.aggregatable_status());
}

ConversionReportSendOutcome ConvertToConversionReportSendOutcome(
    SendResult::Status status) {
  switch (status) {
    case SendResult::Status::kSent:
      return ConversionReportSendOutcome::kSent;
    case SendResult::Status::kTransientFailure:
    case SendResult::Status::kFailure:
      return ConversionReportSendOutcome::kFailed;
    case SendResult::Status::kDropped:
      return ConversionReportSendOutcome::kDropped;
    case SendResult::Status::kFailedToAssemble:
      return ConversionReportSendOutcome::kFailedToAssemble;
  }
}

void RecordAssembleAggregatableReportStatus(
    AssembleAggregatableReportStatus status) {
  base::UmaHistogramEnumeration(
      "Conversions.AggregatableReport.AssembleReportStatus", status);
}

// Called when |report| is to be sent over network for event-level reports or
// to be assembled for aggregatable reports, for logging metrics.
void LogMetricsOnReportSend(const AttributionReport& report, base::Time now) {
  switch (report.GetReportType()) {
    case AttributionReport::ReportType::kEventLevel: {
      // Use a large time range to capture users that might not open the browser
      // for a long time while a conversion report is pending. Revisit this
      // range if it is non-ideal for real world data.
      const AttributionInfo& attribution_info = report.attribution_info();
      base::Time original_report_time = ComputeReportTime(
          attribution_info.source.common_info(), attribution_info.time);
      base::TimeDelta time_since_original_report_time =
          now - original_report_time;
      base::UmaHistogramCustomTimes(
          "Conversions.ExtraReportDelay2", time_since_original_report_time,
          base::Seconds(1), base::Days(24), /*buckets=*/100);

      base::TimeDelta time_from_conversion_to_report_send =
          report.report_time() - attribution_info.time;
      UMA_HISTOGRAM_COUNTS_1000("Conversions.TimeFromConversionToReportSend",
                                time_from_conversion_to_report_send.InHours());
      break;
    }
    case AttributionReport::ReportType::kAggregatableAttribution: {
      base::TimeDelta time_from_conversion_to_report_assembly =
          report.report_time() - report.attribution_info().time;
      UMA_HISTOGRAM_COUNTS_1000(
          "Conversions.AggregatableReport.TimeFromTriggerToReportAssembly",
          time_from_conversion_to_report_assembly.InMinutes());
      break;
    }
  }
}

// Called when |report| is sent, failed or dropped, for logging metrics.
void LogMetricsOnReportCompleted(const AttributionReport& report,
                                 SendResult::Status status) {
  switch (report.GetReportType()) {
    case AttributionReport::ReportType::kEventLevel:
      base::UmaHistogramEnumeration(
          "Conversions.ReportSendOutcome3",
          ConvertToConversionReportSendOutcome(status));
      break;
    case AttributionReport::ReportType::kAggregatableAttribution:
      base::UmaHistogramEnumeration(
          "Conversions.AggregatableReport.ReportSendOutcome2",
          ConvertToConversionReportSendOutcome(status));
      break;
  }
}

std::unique_ptr<AttributionStorageDelegate> MakeStorageDelegate() {
  bool debug_mode = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kConversionsDebugMode);

  if (debug_mode) {
    return std::make_unique<AttributionStorageDelegateImpl>(
        AttributionNoiseMode::kNone, AttributionDelayMode::kNone);
  }

  return std::make_unique<AttributionStorageDelegateImpl>(
      AttributionNoiseMode::kDefault, AttributionDelayMode::kDefault);
}

bool IsOperationAllowed(
    StoragePartitionImpl* storage_partition,
    ContentBrowserClient::ConversionMeasurementOperation operation,
    const url::Origin* source_origin,
    const url::Origin* destination_origin,
    const url::Origin* reporting_origin) {
  DCHECK(storage_partition);
  return GetContentClient()->browser()->IsConversionMeasurementOperationAllowed(
      storage_partition->browser_context(), operation, source_origin,
      destination_origin, reporting_origin);
}

}  // namespace

absl::optional<base::TimeDelta> GetFailedReportDelay(int failed_send_attempts) {
  DCHECK_GT(failed_send_attempts, 0);

  const int kMaxFailedSendAttempts = 2;
  const base::TimeDelta kInitialReportDelay = base::Minutes(5);
  const int kDelayFactor = 3;

  if (failed_send_attempts > kMaxFailedSendAttempts)
    return absl::nullopt;

  return kInitialReportDelay * std::pow(kDelayFactor, failed_send_attempts - 1);
}

// static
void AttributionManagerImpl::RunInMemoryForTesting() {
  AttributionStorageSql::RunInMemoryForTesting();
}

bool AttributionManagerImpl::IsReportAllowed(
    const AttributionReport& report) const {
  const CommonSourceInfo& common_info =
      report.attribution_info().source.common_info();
  return IsOperationAllowed(
      storage_partition_.get(),
      ContentBrowserClient::ConversionMeasurementOperation::kReport,
      &common_info.impression_origin(), &common_info.conversion_origin(),
      &common_info.reporting_origin());
}

// static
std::unique_ptr<AttributionManagerImpl>
AttributionManagerImpl::CreateForTesting(
    const base::FilePath& user_data_directory,
    size_t max_pending_events,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
    std::unique_ptr<AttributionStorageDelegate> storage_delegate,
    std::unique_ptr<AttributionCookieChecker> cookie_checker,
    std::unique_ptr<AttributionReportSender> report_sender,
    StoragePartitionImpl* storage_partition) {
  return absl::WrapUnique(new AttributionManagerImpl(
      storage_partition, user_data_directory, max_pending_events,
      std::move(special_storage_policy), std::move(storage_delegate),
      std::move(cookie_checker), std::move(report_sender),
      /*data_host_manager=*/nullptr));
}

AttributionManagerImpl::AttributionManagerImpl(
    StoragePartitionImpl* storage_partition,
    const base::FilePath& user_data_directory,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy)
    : AttributionManagerImpl(
          storage_partition,
          user_data_directory,
          /*max_pending_events=*/1000,
          std::move(special_storage_policy),
          MakeStorageDelegate(),
          std::make_unique<AttributionCookieCheckerImpl>(storage_partition),
          std::make_unique<AttributionReportNetworkSender>(storage_partition),
          std::make_unique<AttributionDataHostManagerImpl>(this)) {}

AttributionManagerImpl::AttributionManagerImpl(
    StoragePartitionImpl* storage_partition,
    const base::FilePath& user_data_directory,
    size_t max_pending_events,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
    std::unique_ptr<AttributionStorageDelegate> storage_delegate,
    std::unique_ptr<AttributionCookieChecker> cookie_checker,
    std::unique_ptr<AttributionReportSender> report_sender,
    std::unique_ptr<AttributionDataHostManager> data_host_manager)
    : storage_partition_(storage_partition),
      max_pending_events_(max_pending_events),
      attribution_storage_(base::SequenceBound<AttributionStorageSql>(
          g_storage_task_runner.Get(),
          user_data_directory,
          std::move(storage_delegate))),
      scheduler_(base::BindRepeating(&AttributionManagerImpl::GetReportsToSend,
                                     base::Unretained(this)),
                 attribution_storage_),
      data_host_manager_(std::move(data_host_manager)),
      special_storage_policy_(std::move(special_storage_policy)),
      cookie_checker_(std::move(cookie_checker)),
      report_sender_(std::move(report_sender)) {
  DCHECK(storage_partition_);
  DCHECK_GT(max_pending_events_, 0u);
  DCHECK(cookie_checker_);
  DCHECK(report_sender_);
}

AttributionManagerImpl::~AttributionManagerImpl() {
  // Browser contexts are not required to have a special storage policy.
  if (!special_storage_policy_ ||
      !special_storage_policy_->HasSessionOnlyOrigins()) {
    return;
  }

  // Delete stored data for all session only origins given by
  // |special_storage_policy|.
  base::RepeatingCallback<bool(const url::Origin&)>
      session_only_origin_predicate = base::BindRepeating(
          &IsOriginSessionOnly, std::move(special_storage_policy_));
  attribution_storage_.AsyncCall(&AttributionStorage::ClearData)
      .WithArgs(base::Time::Min(), base::Time::Max(),
                std::move(session_only_origin_predicate),
                /*delete_rate_limit_data=*/true);
}

void AttributionManagerImpl::AddObserver(AttributionObserver* observer) {
  observers_.AddObserver(observer);
}

void AttributionManagerImpl::RemoveObserver(AttributionObserver* observer) {
  observers_.RemoveObserver(observer);
}

AttributionDataHostManager* AttributionManagerImpl::GetDataHostManager() {
  return data_host_manager_.get();
}

void AttributionManagerImpl::HandleSource(StorableSource source) {
  MaybeEnqueueEvent(std::move(source));
}

void AttributionManagerImpl::StoreSource(StorableSource source) {
  // Only retrieve deactivated sources if an observer is there to hear it.
  // Technically, an observer could be registered between the time the async
  // call is made and the time the response is received, but this is unlikely.
  int deactivated_source_return_limit = observers_.empty() ? 0 : 50;
  attribution_storage_.AsyncCall(&AttributionStorage::StoreSource)
      .WithArgs(source, deactivated_source_return_limit)
      .Then(base::BindOnce(&AttributionManagerImpl::OnSourceStored,
                           weak_factory_.GetWeakPtr(), std::move(source)));
}

void AttributionManagerImpl::OnSourceStored(
    StorableSource source,
    AttributionStorage::StoreSourceResult result) {
  // TODO(apaseltiner): Consider logging UMA based on `result` to help
  // understand how often this fails due to privacy limits, etc.

  for (auto& observer : observers_)
    observer.OnSourceHandled(source, result.status);

  scheduler_.ScheduleSend(result.min_fake_report_time);

  NotifySourcesChanged();

  for (const auto& deactivated_source : result.deactivated_sources) {
    NotifySourceDeactivated(deactivated_source);
  }
}

void AttributionManagerImpl::HandleTrigger(AttributionTrigger trigger) {
  MaybeEnqueueEvent(std::move(trigger));
}

void AttributionManagerImpl::StoreTrigger(AttributionTrigger trigger) {
  attribution_storage_.AsyncCall(&AttributionStorage::MaybeCreateAndStoreReport)
      .WithArgs(trigger)
      .Then(base::BindOnce(&AttributionManagerImpl::OnReportStored,
                           weak_factory_.GetWeakPtr(), std::move(trigger)));
}

void AttributionManagerImpl::MaybeEnqueueEvent(SourceOrTrigger event) {
  const size_t size_before_push = pending_events_.size();

  // Avoid unbounded memory growth with adversarial input.
  bool allowed = size_before_push < max_pending_events_;
  base::UmaHistogramBoolean("Conversions.EnqueueEventAllowed", allowed);
  if (!allowed)
    return;

  pending_events_.push_back(std::move(event));

  // Only process the new event if it is the only one in the queue. Otherwise,
  // there's already an async cookie-check in progress.
  if (size_before_push == 0)
    ProcessEvents();
}

void AttributionManagerImpl::ProcessEvents() {
  struct DebugCookieOriginGetter {
    const url::Origin* operator()(const StorableSource& source) const {
      return source.common_info().debug_key().has_value()
                 ? &source.common_info().reporting_origin()
                 : nullptr;
    }

    const url::Origin* operator()(const AttributionTrigger& trigger) const {
      return trigger.debug_key().has_value() ? &trigger.reporting_origin()
                                             : nullptr;
    }
  };

  // Process as many events not requiring a cookie check (synchronously) as
  // possible. Once reaching the first to require a cookie check, start the
  // async check and stop processing further events.
  while (!pending_events_.empty()) {
    const url::Origin* cookie_origin =
        absl::visit(DebugCookieOriginGetter(), pending_events_.front());
    if (cookie_origin) {
      cookie_checker_->IsDebugCookieSet(
          *cookie_origin,
          base::BindOnce(
              [](base::WeakPtr<AttributionManagerImpl> manager,
                 bool is_debug_cookie_set) {
                if (manager) {
                  manager->ProcessNextEvent(is_debug_cookie_set);
                  manager->ProcessEvents();
                }
              },
              weak_factory_.GetWeakPtr()));
      return;
    }

    ProcessNextEvent(/*is_debug_cookie_set=*/false);
  }
}

void AttributionManagerImpl::ProcessNextEvent(bool is_debug_cookie_set) {
  DCHECK(!pending_events_.empty());

  struct EventStorer {
    raw_ptr<AttributionManagerImpl> manager;
    bool is_debug_cookie_set;

    void operator()(StorableSource source) {
      CommonSourceInfo& common_info = source.common_info();

      bool allowed = IsOperationAllowed(
          manager->storage_partition_.get(),
          ContentBrowserClient::ConversionMeasurementOperation::kImpression,
          &common_info.impression_origin(),
          /*destination_origin=*/nullptr, &common_info.reporting_origin());
      RecordRegisterImpressionAllowed(allowed);
      if (!allowed) {
        manager->OnSourceStored(
            std::move(source),
            AttributionStorage::StoreSourceResult(
                StorableSource::Result::kProhibitedByBrowserPolicy));
        return;
      }

      if (!is_debug_cookie_set)
        common_info.ClearDebugKey();

      manager->StoreSource(std::move(source));
    }

    void operator()(AttributionTrigger trigger) {
      bool allowed = IsOperationAllowed(
          manager->storage_partition_.get(),
          ContentBrowserClient::ConversionMeasurementOperation::kConversion,
          /*source_origin=*/nullptr, &trigger.destination_origin(),
          &trigger.reporting_origin());
      RecordRegisterConversionAllowed(allowed);
      if (!allowed) {
        manager->OnReportStored(
            std::move(trigger),
            CreateReportResult(/*trigger_time=*/base::Time::Now(),
                               AttributionTrigger::EventLevelResult::
                                   kProhibitedByBrowserPolicy,
                               AttributionTrigger::AggregatableResult::
                                   kProhibitedByBrowserPolicy));
        return;
      }

      if (!is_debug_cookie_set)
        trigger.ClearDebugKey();

      manager->StoreTrigger(std::move(trigger));
    }
  };

  SourceOrTrigger event = std::move(pending_events_.front());
  pending_events_.pop_front();

  absl::visit(
      EventStorer{.manager = this, .is_debug_cookie_set = is_debug_cookie_set},
      std::move(event));
}

void AttributionManagerImpl::OnReportStored(const AttributionTrigger trigger,
                                            CreateReportResult result) {
  RecordCreateReportStatus(result);

  absl::optional<base::Time> min_new_report_time;

  if (auto& report = result.new_event_level_report()) {
    min_new_report_time = report->report_time();
    MaybeSendDebugReport(std::move(*report));
  }

  if (auto& report = result.new_aggregatable_report()) {
    min_new_report_time = AttributionReport::MinReportTime(
        min_new_report_time, report->report_time());
    MaybeSendDebugReport(std::move(*report));
  }

  scheduler_.ScheduleSend(min_new_report_time);

  if (result.event_level_status() !=
      AttributionTrigger::EventLevelResult::kInternalError) {
    // Sources are changed here because storing an event-level report can
    // cause sources to reach event-level attribution limit or become
    // associated with a dedup key.
    NotifySourcesChanged();
    NotifyReportsChanged(AttributionReport::ReportType::kEventLevel);
  }

  if (result.aggregatable_status() ==
      AttributionTrigger::AggregatableResult::kSuccess) {
    NotifyReportsChanged(
        AttributionReport::ReportType::kAggregatableAttribution);
  }

  for (auto& observer : observers_)
    observer.OnTriggerHandled(trigger, result);
}

void AttributionManagerImpl::MaybeSendDebugReport(AttributionReport&& report) {
  const AttributionInfo& attribution_info = report.attribution_info();
  if (!attribution_info.debug_key ||
      !attribution_info.source.common_info().debug_key() ||
      !IsReportAllowed(report)) {
    return;
  }

  // We don't delete from storage for debug reports.
  PrepareToSendReport(std::move(report), /*is_debug_report=*/true,
                      base::BindOnce(&AttributionManagerImpl::NotifyReportSent,
                                     weak_factory_.GetWeakPtr(),
                                     /*is_debug_report=*/true));
}

void AttributionManagerImpl::GetActiveSourcesForWebUI(
    base::OnceCallback<void(std::vector<StoredSource>)> callback) {
  const int kMaxSources = 1000;
  attribution_storage_.AsyncCall(&AttributionStorage::GetActiveSources)
      .WithArgs(kMaxSources)
      .Then(std::move(callback));
}

void AttributionManagerImpl::GetPendingReportsForInternalUse(
    AttributionReport::ReportTypes report_types,
    int limit,
    base::OnceCallback<void(std::vector<AttributionReport>)> callback) {
  attribution_storage_.AsyncCall(&AttributionStorage::GetAttributionReports)
      .WithArgs(
          /*max_report_time=*/base::Time::Max(), limit, std::move(report_types))
      .Then(std::move(callback));
}

void AttributionManagerImpl::SendReportsForWebUI(
    const std::vector<AttributionReport::Id>& ids,
    base::OnceClosure done) {
  attribution_storage_.AsyncCall(&AttributionStorage::GetReports)
      .WithArgs(ids)
      .Then(base::BindOnce(&AttributionManagerImpl::OnGetReportsToSendFromWebUI,
                           weak_factory_.GetWeakPtr(), std::move(done)));
}

void AttributionManagerImpl::ClearData(
    base::Time delete_begin,
    base::Time delete_end,
    base::RepeatingCallback<bool(const url::Origin&)> filter,
    bool delete_rate_limit_data,
    base::OnceClosure done) {
  attribution_storage_.AsyncCall(&AttributionStorage::ClearData)
      .WithArgs(delete_begin, delete_end, std::move(filter),
                delete_rate_limit_data)
      .Then(base::BindOnce(
          [](base::OnceClosure done,
             base::WeakPtr<AttributionManagerImpl> manager) {
            std::move(done).Run();

            if (manager) {
              manager->scheduler_.Refresh();
              manager->NotifySourcesChanged();
              manager->NotifyReportsChanged(
                  AttributionReport::ReportType::kEventLevel);
              manager->NotifyReportsChanged(
                  AttributionReport::ReportType::kAggregatableAttribution);
            }
          },
          std::move(done), weak_factory_.GetWeakPtr()));
}

void AttributionManagerImpl::GetReportsToSend() {
  // We only get the next report time strictly after now, because if we are
  // sending a report now but haven't finished doing so and it is still present
  // in storage, storage will return the report time for the same report.
  // Deduplication via `reports_being_sent_` will ensure that the report isn't
  // sent twice, but it will result in wasted processing.
  //
  // TODO(apaseltiner): Consider limiting the number of reports being sent at
  // once, to avoid pulling an arbitrary number of reports into memory.
  attribution_storage_.AsyncCall(&AttributionStorage::GetAttributionReports)
      .WithArgs(/*max_report_time=*/base::Time::Now(), /*limit=*/-1,
                AttributionReport::ReportTypes{
                    AttributionReport::ReportType::kEventLevel,
                    AttributionReport::ReportType::kAggregatableAttribution})
      .Then(base::BindOnce(&AttributionManagerImpl::OnGetReportsToSend,
                           weak_factory_.GetWeakPtr()));
}

void AttributionManagerImpl::OnGetReportsToSend(
    std::vector<AttributionReport> reports) {
  if (reports.empty())
    return;

  SendReports(std::move(reports), /*log_metrics=*/true, base::DoNothing());
  scheduler_.Refresh();
}

void AttributionManagerImpl::OnGetReportsToSendFromWebUI(
    base::OnceClosure done,
    std::vector<AttributionReport> reports) {
  if (reports.empty()) {
    std::move(done).Run();
    return;
  }

  base::Time now = base::Time::Now();
  for (AttributionReport& report : reports) {
    report.set_report_time(now);
  }

  auto barrier = base::BarrierClosure(reports.size(), std::move(done));
  SendReports(std::move(reports), /*log_metrics=*/false, std::move(barrier));
}

void AttributionManagerImpl::SendReports(std::vector<AttributionReport> reports,
                                         bool log_metrics,
                                         base::RepeatingClosure done) {
  const base::Time now = base::Time::Now();
  for (AttributionReport& report : reports) {
    DCHECK_LE(report.report_time(), now);

    bool inserted = reports_being_sent_.emplace(report.ReportId()).second;
    if (!inserted) {
      done.Run();
      continue;
    }

    if (!IsReportAllowed(report)) {
      // If measurement is disallowed, just drop the report on the floor. We
      // need to make sure we forward that the report was "sent" to ensure it is
      // deleted from storage, etc. This simulates sending the report through a
      // null channel.
      OnReportSent(done, std::move(report),
                   SendResult(SendResult::Status::kDropped));
      continue;
    }

    if (log_metrics)
      LogMetricsOnReportSend(report, now);

    PrepareToSendReport(std::move(report), /*is_debug_report=*/false,
                        base::BindOnce(&AttributionManagerImpl::OnReportSent,
                                       weak_factory_.GetWeakPtr(), done));
  }
}

void AttributionManagerImpl::MarkReportCompleted(
    AttributionReport::Id report_id) {
  size_t num_removed = reports_being_sent_.erase(report_id);
  DCHECK_EQ(num_removed, 1u);
}

void AttributionManagerImpl::PrepareToSendReport(AttributionReport report,
                                                 bool is_debug_report,
                                                 ReportSentCallback callback) {
  switch (report.GetReportType()) {
    case AttributionReport::ReportType::kEventLevel:
      report_sender_->SendReport(std::move(report), is_debug_report,
                                 std::move(callback));
      break;
    case AttributionReport::ReportType::kAggregatableAttribution:
      AssembleAggregatableReport(std::move(report), is_debug_report,
                                 std::move(callback));
      break;
  }
}

void AttributionManagerImpl::OnReportSent(base::OnceClosure done,
                                          AttributionReport report,
                                          SendResult info) {
  // If there was a transient failure, and another attempt is allowed,
  // update the report's DB state to reflect that. Otherwise, delete the report
  // from storage if it wasn't skipped due to the browser being offline.

  bool should_retry = false;
  if (info.status == SendResult::Status::kTransientFailure) {
    report.set_failed_send_attempts(report.failed_send_attempts() + 1);
    const absl::optional<base::TimeDelta> delay =
        GetFailedReportDelay(report.failed_send_attempts());
    if (delay.has_value()) {
      should_retry = true;
      report.set_report_time(base::Time::Now() + *delay);
    }
  }

  if (should_retry) {
    // After updating the report's failure count and new report time in the DB,
    // add it directly to the queue so that the retry is attempted as the new
    // report time is reached, rather than wait for the next DB-polling to
    // occur.
    attribution_storage_
        .AsyncCall(&AttributionStorage::UpdateReportForSendFailure)
        .WithArgs(report.ReportId(), report.report_time())
        .Then(base::BindOnce(
            [](base::OnceClosure done,
               base::WeakPtr<AttributionManagerImpl> manager,
               AttributionReport::Id report_id, base::Time new_report_time,
               bool success) {
              std::move(done).Run();

              if (manager && success) {
                manager->MarkReportCompleted(report_id);
                manager->scheduler_.ScheduleSend(new_report_time);
                manager->NotifyReportsChanged(
                    AttributionReport::GetReportType(report_id));
              }
            },
            std::move(done), weak_factory_.GetWeakPtr(), report.ReportId(),
            report.report_time()));

    // TODO(apaseltiner): Consider surfacing retry attempts in internals UI.

    return;
  }

  attribution_storage_.AsyncCall(&AttributionStorage::DeleteReport)
      .WithArgs(report.ReportId())
      .Then(base::BindOnce(
          [](base::OnceClosure done,
             base::WeakPtr<AttributionManagerImpl> manager,
             AttributionReport::Id report_id, bool success) {
            std::move(done).Run();

            if (manager && success) {
              manager->MarkReportCompleted(report_id);
              manager->NotifyReportsChanged(
                  AttributionReport::GetReportType(report_id));
            }
          },
          std::move(done), weak_factory_.GetWeakPtr(), report.ReportId()));

  LogMetricsOnReportCompleted(report, info.status);

  NotifyReportSent(/*is_debug_report=*/false, std::move(report), info);
}

void AttributionManagerImpl::NotifyReportSent(bool is_debug_report,
                                              AttributionReport report,
                                              SendResult info) {
  for (auto& observer : observers_)
    observer.OnReportSent(report, /*is_debug_report=*/is_debug_report, info);
}

void AttributionManagerImpl::AssembleAggregatableReport(
    AttributionReport report,
    bool is_debug_report,
    ReportSentCallback callback) {
  AggregationService* aggregation_service =
      storage_partition_->GetAggregationService();
  if (!aggregation_service) {
    RecordAssembleAggregatableReportStatus(
        AssembleAggregatableReportStatus::kAggregationServiceUnavailable);
    std::move(callback).Run(std::move(report),
                            SendResult(SendResult::Status::kFailedToAssemble));
    return;
  }

  absl::optional<AggregatableReportRequest> request =
      CreateAggregatableReportRequest(report);
  if (!request.has_value()) {
    RecordAssembleAggregatableReportStatus(
        AssembleAggregatableReportStatus::kCreateRequestFailed);
    std::move(callback).Run(std::move(report),
                            SendResult(SendResult::Status::kFailedToAssemble));
    return;
  }

  aggregation_service->AssembleReport(
      std::move(*request),
      base::BindOnce(&AttributionManagerImpl::OnAggregatableReportAssembled,
                     weak_factory_.GetWeakPtr(), std::move(report),
                     is_debug_report, std::move(callback)));
}

void AttributionManagerImpl::OnAggregatableReportAssembled(
    AttributionReport report,
    bool is_debug_report,
    ReportSentCallback callback,
    absl::optional<AggregatableReport> assembled_report,
    AggregationService::AssemblyStatus) {
  if (!assembled_report.has_value()) {
    RecordAssembleAggregatableReportStatus(
        AssembleAggregatableReportStatus::kAssembleReportFailed);
    std::move(callback).Run(std::move(report),
                            SendResult(SendResult::Status::kFailedToAssemble));
    return;
  }

  auto* data = absl::get_if<AttributionReport::AggregatableAttributionData>(
      &report.data());
  DCHECK(data);
  data->assembled_report = std::move(assembled_report);
  RecordAssembleAggregatableReportStatus(
      AssembleAggregatableReportStatus::kSuccess);

  report_sender_->SendReport(std::move(report), is_debug_report,
                             std::move(callback));
}

void AttributionManagerImpl::NotifySourcesChanged() {
  for (auto& observer : observers_)
    observer.OnSourcesChanged();
}

void AttributionManagerImpl::NotifyReportsChanged(
    AttributionReport::ReportType report_type) {
  for (auto& observer : observers_)
    observer.OnReportsChanged(report_type);
}

void AttributionManagerImpl::NotifySourceDeactivated(
    const StoredSource& source) {
  for (auto& observer : observers_)
    observer.OnSourceDeactivated(source);
}

}  // namespace content
