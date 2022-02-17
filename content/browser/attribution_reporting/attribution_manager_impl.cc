// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_manager_impl.h"

#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_cookie_checker.h"
#include "content/browser/attribution_reporting/attribution_cookie_checker_impl.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager_impl.h"
#include "content/browser/attribution_reporting/attribution_info.h"
#include "content/browser/attribution_reporting/attribution_network_sender.h"
#include "content/browser/attribution_reporting/attribution_network_sender_impl.h"
#include "content/browser/attribution_reporting/attribution_policy.h"
#include "content/browser/attribution_reporting/attribution_report.h"
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
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace content {

namespace {

using CreateReportResult = ::content::AttributionStorage::CreateReportResult;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ConversionReportSendOutcome {
  kSent = 0,
  kFailed = 1,
  kDropped = 2,
  kMaxValue = kDropped
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

void RecordCreateReportStatus(AttributionTrigger::Result status) {
  base::UmaHistogramEnumeration("Conversions.CreateReportStatus", status);
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
  }
}

// Called when |report| is to be sent over network, for logging metrics.
void LogMetricsOnReportSend(const AttributionReport& report, base::Time now) {
  // Use a large time range to capture users that might not open the browser for
  // a long time while a conversion report is pending. Revisit this range if it
  // is non-ideal for real world data.
  const AttributionInfo& attribution_info = report.attribution_info();
  base::Time original_report_time = ComputeReportTime(
      attribution_info.source.common_info(), attribution_info.time);
  base::TimeDelta time_since_original_report_time = now - original_report_time;
  base::UmaHistogramCustomTimes(
      "Conversions.ExtraReportDelay2", time_since_original_report_time,
      base::Seconds(1), base::Days(24), /*buckets=*/100);

  base::TimeDelta time_from_conversion_to_report_send =
      report.report_time() - attribution_info.time;
  UMA_HISTOGRAM_COUNTS_1000("Conversions.TimeFromConversionToReportSend",
                            time_from_conversion_to_report_send.InHours());
}

bool IsOffline() {
  return content::GetNetworkConnectionTracker()->IsOffline();
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

}  // namespace

AttributionManager* AttributionManagerProviderImpl::GetManager(
    WebContents* web_contents) const {
  return static_cast<StoragePartitionImpl*>(
             web_contents->GetBrowserContext()->GetDefaultStoragePartition())
      ->GetAttributionManager();
}

// static
void AttributionManagerImpl::RunInMemoryForTesting() {
  AttributionStorageSql::RunInMemoryForTesting();
}

// static
AttributionManagerImpl::IsReportAllowedCallback
AttributionManagerImpl::DefaultIsReportAllowedCallback(
    BrowserContext* browser_context) {
  return base::BindRepeating(
      [](BrowserContext* browser_context, const AttributionReport& report) {
        const CommonSourceInfo& common_info =
            report.attribution_info().source.common_info();
        return GetContentClient()
            ->browser()
            ->IsConversionMeasurementOperationAllowed(
                browser_context,
                ContentBrowserClient::ConversionMeasurementOperation::kReport,
                &common_info.impression_origin(),
                &common_info.conversion_origin(),
                &common_info.reporting_origin());
      },
      browser_context);
}

// static
std::unique_ptr<AttributionManagerImpl>
AttributionManagerImpl::CreateForTesting(
    IsReportAllowedCallback is_report_allowed_callback,
    const base::FilePath& user_data_directory,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
    std::unique_ptr<AttributionStorageDelegate> storage_delegate,
    std::unique_ptr<AttributionCookieChecker> cookie_checker,
    std::unique_ptr<AttributionNetworkSender> network_sender) {
  return absl::WrapUnique(new AttributionManagerImpl(
      std::move(is_report_allowed_callback), user_data_directory,
      std::move(special_storage_policy), std::move(storage_delegate),
      std::move(cookie_checker), std::move(network_sender),
      /*data_host_manager=*/nullptr));
}

AttributionManagerImpl::AttributionManagerImpl(
    StoragePartitionImpl* storage_partition,
    const base::FilePath& user_data_directory,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy)
    : AttributionManagerImpl(
          DefaultIsReportAllowedCallback(storage_partition->browser_context()),
          user_data_directory,
          std::move(special_storage_policy),
          MakeStorageDelegate(),
          std::make_unique<AttributionCookieCheckerImpl>(storage_partition),
          std::make_unique<AttributionNetworkSenderImpl>(storage_partition),
          std::make_unique<AttributionDataHostManagerImpl>(
              storage_partition->browser_context(),
              this)) {}

AttributionManagerImpl::AttributionManagerImpl(
    IsReportAllowedCallback is_report_allowed_callback,
    const base::FilePath& user_data_directory,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
    std::unique_ptr<AttributionStorageDelegate> storage_delegate,
    std::unique_ptr<AttributionCookieChecker> cookie_checker,
    std::unique_ptr<AttributionNetworkSender> network_sender,
    std::unique_ptr<AttributionDataHostManager> data_host_manager)
    : is_report_allowed_callback_(std::move(is_report_allowed_callback)),
      attribution_storage_(base::SequenceBound<AttributionStorageSql>(
          g_storage_task_runner.Get(),
          user_data_directory,
          std::move(storage_delegate))),
      data_host_manager_(std::move(data_host_manager)),
      special_storage_policy_(std::move(special_storage_policy)),
      cookie_checker_(std::move(cookie_checker)),
      network_sender_(std::move(network_sender)),
      weak_factory_(this) {
  DCHECK(is_report_allowed_callback_);
  DCHECK(cookie_checker_);
  DCHECK(network_sender_);

  content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);

  OnConnectionChanged(network::mojom::ConnectionType::CONNECTION_UNKNOWN);
}

AttributionManagerImpl::~AttributionManagerImpl() {
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);

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
                std::move(session_only_origin_predicate));
}

void AttributionManagerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AttributionManagerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

AttributionDataHostManager* AttributionManagerImpl::GetDataHostManager() {
  return data_host_manager_.get();
}

void AttributionManagerImpl::HandleSource(StorableSource source) {
  // Process attributions in the order in which they were received, processing
  // the new one after any background attributions are flushed.
  GetContentClient()->browser()->FlushBackgroundAttributions(
      base::BindOnce(&AttributionManagerImpl::MaybeEnqueueEvent,
                     weak_factory_.GetWeakPtr(), std::move(source)));
}

void AttributionManagerImpl::StoreSource(StorableSource source) {
  // Only retrieve deactivated sources if an observer is there to hear it.
  // Technically, an observer could be registered between the time the async
  // call is made and the time the response is received, but this is unlikely.
  int deactivated_source_return_limit = observers_.empty() ? 0 : 50;
  attribution_storage_.AsyncCall(&AttributionStorage::StoreSource)
      .WithArgs(source, deactivated_source_return_limit)
      .Then(base::BindOnce(
          [](base::WeakPtr<AttributionManagerImpl> manager,
             StorableSource source,
             AttributionStorage::StoreSourceResult result) {
            // TODO(apaseltiner): Consider logging UMA based on `result` to help
            // understand how often this fails due to privacy limits, etc.
            //
            // TODO(apaseltiner): Show rejected sources in internals UI.

            if (!manager)
              return;

            for (Observer& observer : manager->observers_)
              observer.OnSourceHandled(source, result.status);

            if (result.min_fake_report_time.has_value()) {
              manager->UpdateGetReportsToSendTimer(
                  *result.min_fake_report_time);
            }

            manager->NotifySourcesChanged();

            for (const auto& deactivated_source : result.deactivated_sources) {
              manager->NotifySourceDeactivated(deactivated_source);
            }
          },
          weak_factory_.GetWeakPtr(), std::move(source)));
}

void AttributionManagerImpl::HandleTrigger(AttributionTrigger trigger) {
  GetContentClient()->browser()->FlushBackgroundAttributions(
      base::BindOnce(&AttributionManagerImpl::MaybeEnqueueEvent,
                     weak_factory_.GetWeakPtr(), std::move(trigger)));
}

void AttributionManagerImpl::StoreTrigger(AttributionTrigger trigger) {
  attribution_storage_.AsyncCall(&AttributionStorage::MaybeCreateAndStoreReport)
      .WithArgs(std::move(trigger))
      .Then(base::BindOnce(&AttributionManagerImpl::OnReportStored,
                           weak_factory_.GetWeakPtr()));
}

void AttributionManagerImpl::MaybeEnqueueEvent(SourceOrTrigger event) {
  const size_t size_before_push = pending_events_.size();

  // Avoid unbounded memory growth with adversarial input.
  // TODO(apaseltiner): Consider logging UMA / surfacing DevTools issue, since
  // this will cause data loss.
  constexpr size_t kMaxPendingEvents = 1000;
  if (size_before_push == kMaxPendingEvents)
    return;

  pending_events_.push_back(std::move(event));

  // Only process the new event if it is the only one in the queue. Otherwise,
  // there's already an async cookie-check in progress.
  if (size_before_push == 0)
    ProcessEvents();
}

void AttributionManagerImpl::MaybeEnqueueEventForTesting(
    SourceOrTrigger event) {
  MaybeEnqueueEvent(std::move(event));
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
      if (!is_debug_cookie_set)
        source.common_info().ClearDebugKey();

      manager->StoreSource(std::move(source));
    }

    void operator()(AttributionTrigger trigger) {
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

void AttributionManagerImpl::OnReportStored(CreateReportResult result) {
  RecordCreateReportStatus(result.status());

  UpdateGetReportsToSendTimer(result.report_time());

  if (result.status() != AttributionTrigger::Result::kInternalError) {
    // Sources are changed here because storing a report can cause sources to be
    // deleted or become associated with a dedup key.
    NotifySourcesChanged();
    NotifyReportsChanged();
  }

  if (absl::optional<AttributionStorage::DeactivatedSource> source =
          result.GetDeactivatedSource()) {
    NotifySourceDeactivated(*source);
  }

  for (Observer& observer : observers_)
    observer.OnTriggerHandled(result);
}

void AttributionManagerImpl::GetActiveSourcesForWebUI(
    base::OnceCallback<void(std::vector<StoredSource>)> callback) {
  const int kMaxSources = 1000;
  attribution_storage_.AsyncCall(&AttributionStorage::GetActiveSources)
      .WithArgs(kMaxSources)
      .Then(std::move(callback));
}

void AttributionManagerImpl::GetPendingReportsForInternalUse(
    base::OnceCallback<void(std::vector<AttributionReport>)> callback) {
  GetAndHandleReports(std::move(callback),
                      /*max_report_time=*/base::Time::Max(), /*limit=*/1000);
}

void AttributionManagerImpl::SendReportsForWebUI(
    const std::vector<AttributionReport::EventLevelData::Id>& ids,
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
    base::OnceClosure done) {
  attribution_storage_.AsyncCall(&AttributionStorage::ClearData)
      .WithArgs(delete_begin, delete_end, std::move(filter))
      .Then(base::BindOnce(
          [](base::OnceClosure done,
             base::WeakPtr<AttributionManagerImpl> manager) {
            std::move(done).Run();

            if (manager) {
              manager->StartGetReportsToSendTimer();
              manager->NotifySourcesChanged();
              manager->NotifyReportsChanged();
            }
          },
          std::move(done), weak_factory_.GetWeakPtr()));
}

void AttributionManagerImpl::OnConnectionChanged(
    network::mojom::ConnectionType connection_type) {
  if (IsOffline()) {
    get_reports_to_send_timer_.Stop();
  } else {
    DCHECK(!get_reports_to_send_timer_.IsRunning());

    // Add delay to all reports that should have been sent while the browser was
    // offline so they are not temporally joinable. We do this in storage to
    // avoid pulling an unbounded number of reports into memory, only to
    // immediately issue async storage calls to modify their report times.
    attribution_storage_
        .AsyncCall(&AttributionStorage::AdjustOfflineReportTimes)
        .Then(
            base::BindOnce(&AttributionManagerImpl::UpdateGetReportsToSendTimer,
                           weak_factory_.GetWeakPtr()));
  }
}

void AttributionManagerImpl::GetAndHandleReports(
    ReportsHandlerFunc handler_function,
    base::Time max_report_time,
    int limit) {
  attribution_storage_.AsyncCall(&AttributionStorage::GetAttributionsToReport)
      .WithArgs(max_report_time, limit)
      .Then(std::move(handler_function));
}

void AttributionManagerImpl::UpdateGetReportsToSendTimer(
    absl::optional<base::Time> time) {
  if (!time.has_value() || IsOffline())
    return;

  if (!get_reports_to_send_timer_.IsRunning() ||
      *time < get_reports_to_send_timer_.desired_run_time()) {
    get_reports_to_send_timer_.Start(FROM_HERE, *time, this,
                                     &AttributionManagerImpl::GetReportsToSend);
  }
}

void AttributionManagerImpl::StartGetReportsToSendTimer() {
  if (IsOffline())
    return;

  attribution_storage_.AsyncCall(&AttributionStorage::GetNextReportTime)
      .WithArgs(base::Time::Now())
      .Then(base::BindOnce(&AttributionManagerImpl::UpdateGetReportsToSendTimer,
                           weak_factory_.GetWeakPtr()));
}

void AttributionManagerImpl::GetReportsToSend() {
  DCHECK(!IsOffline());

  // We only get the next report time strictly after now, because if we are
  // sending a report now but haven't finished doing so and it is still present
  // in storage, storage will return the report time for the same report.
  // Deduplication via `reports_being_sent_` will ensure that the report isn't
  // sent twice, but it will result in wasted processing.
  //
  // TODO(apaseltiner): Consider limiting the number of reports being sent at
  // once, to avoid pulling an arbitrary number of reports into memory.
  GetAndHandleReports(
      base::BindOnce(&AttributionManagerImpl::OnGetReportsToSend,
                     weak_factory_.GetWeakPtr()),
      /*max_report_time=*/base::Time::Now(), /*limit=*/-1);
}

void AttributionManagerImpl::OnGetReportsToSend(
    std::vector<AttributionReport> reports) {
  if (reports.empty() || IsOffline())
    return;

  SendReports(std::move(reports), /*log_metrics=*/true, base::DoNothing());
  StartGetReportsToSendTimer();
}

void AttributionManagerImpl::OnGetReportsToSendFromWebUI(
    base::OnceClosure done,
    std::vector<AttributionReport> reports) {
  if (reports.empty() || IsOffline()) {
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
    DCHECK(report.ReportId().has_value());
    DCHECK_LE(report.report_time(), now);

    DCHECK(absl::holds_alternative<AttributionReport::EventLevelData::Id>(
        *report.ReportId()));
    bool inserted =
        reports_being_sent_
            .emplace(absl::get<AttributionReport::EventLevelData::Id>(
                *report.ReportId()))
            .second;
    if (!inserted) {
      done.Run();
      continue;
    }

    if (!is_report_allowed_callback_.Run(report)) {
      // If measurement is disallowed, just drop the report on the floor. We
      // need to make sure we forward that the report was "sent" to ensure it is
      // deleted from storage, etc. This simulates sending the report through a
      // null channel.
      OnReportSent(done, std::move(report),
                   SendResult(SendResult::Status::kDropped,
                              /*http_response_code=*/0));
      continue;
    }

    if (log_metrics)
      LogMetricsOnReportSend(report, now);

    network_sender_->SendReport(
        std::move(report), base::BindOnce(&AttributionManagerImpl::OnReportSent,
                                          weak_factory_.GetWeakPtr(), done));
  }
}

void AttributionManagerImpl::MarkReportCompleted(
    AttributionReport::EventLevelData::Id report_id) {
  size_t num_removed = reports_being_sent_.erase(report_id);
  DCHECK_EQ(num_removed, 1u);
}

void AttributionManagerImpl::OnReportSent(base::OnceClosure done,
                                          AttributionReport report,
                                          SendResult info) {
  DCHECK(report.ReportId().has_value());

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

  DCHECK(absl::holds_alternative<AttributionReport::EventLevelData::Id>(
      *report.ReportId()));
  const auto report_id =
      absl::get<AttributionReport::EventLevelData::Id>(*report.ReportId());

  if (should_retry) {
    // After updating the report's failure count and new report time in the DB,
    // add it directly to the queue so that the retry is attempted as the new
    // report time is reached, rather than wait for the next DB-polling to
    // occur.
    attribution_storage_
        .AsyncCall(&AttributionStorage::UpdateReportForSendFailure)
        .WithArgs(report_id, report.report_time())
        .Then(base::BindOnce(
            [](base::OnceClosure done,
               base::WeakPtr<AttributionManagerImpl> manager,
               AttributionReport::EventLevelData::Id report_id,
               base::Time new_report_time, bool success) {
              std::move(done).Run();

              if (manager && success) {
                manager->MarkReportCompleted(report_id);
                manager->UpdateGetReportsToSendTimer(new_report_time);
                manager->NotifyReportsChanged();
              }
            },
            std::move(done), weak_factory_.GetWeakPtr(), report_id,
            report.report_time()));
  } else {
    attribution_storage_.AsyncCall(&AttributionStorage::DeleteReport)
        .WithArgs(report_id)
        .Then(base::BindOnce(
            [](base::OnceClosure done,
               base::WeakPtr<AttributionManagerImpl> manager,
               AttributionReport::EventLevelData::Id report_id, bool success) {
              std::move(done).Run();

              if (manager && success) {
                manager->MarkReportCompleted(report_id);
                manager->NotifyReportsChanged();
              }
            },
            std::move(done), weak_factory_.GetWeakPtr(), report_id));

    base::UmaHistogramEnumeration(
        "Conversions.ReportSendOutcome",
        ConvertToConversionReportSendOutcome(info.status));
  }

  // TODO(apaseltiner): Consider surfacing retry attempts in internals UI.
  if (info.status != SendResult::Status::kSent &&
      info.status != SendResult::Status::kFailure &&
      info.status != SendResult::Status::kDropped) {
    return;
  }

  for (Observer& observer : observers_)
    observer.OnReportSent(report, info);
}

void AttributionManagerImpl::NotifySourcesChanged() {
  for (Observer& observer : observers_)
    observer.OnSourcesChanged();
}

void AttributionManagerImpl::NotifyReportsChanged() {
  for (Observer& observer : observers_)
    observer.OnReportsChanged();
}

void AttributionManagerImpl::NotifySourceDeactivated(
    const AttributionStorage::DeactivatedSource& source) {
  for (Observer& observer : observers_)
    observer.OnSourceDeactivated(source);
}

}  // namespace content
