// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_manager_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/time/default_clock.h"
#include "content/browser/attribution_reporting/attribution_policy.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_reporter_impl.h"
#include "content/browser/attribution_reporting/attribution_storage_delegate_impl.h"
#include "content/browser/attribution_reporting/attribution_storage_sql.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/storable_trigger.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"

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

void RecordCreateReportStatus(CreateReportResult::Status status) {
  base::UmaHistogramEnumeration("Conversions.CreateReportStatus", status);
}

// We measure this in order to be able to count reports that weren't
// successfully deleted, which can lead to duplicate reports.
void RecordDeleteEvent(AttributionManagerImpl::DeleteEvent event) {
  base::UmaHistogramEnumeration("Conversions.DeleteSentReportOperation", event);
}

ConversionReportSendOutcome ConvertToConversionReportSendOutcome(
    SentReport::Status status) {
  switch (status) {
    case SentReport::Status::kSent:
      return ConversionReportSendOutcome::kSent;
    case SentReport::Status::kTransientFailure:
    case SentReport::Status::kFailure:
      return ConversionReportSendOutcome::kFailed;
    case SentReport::Status::kOffline:
    case SentReport::Status::kRemovedFromQueue:
      // Offline reports and reports removed from the queue before being sent
      // should never record an outcome.
      NOTREACHED();
      return ConversionReportSendOutcome::kFailed;
    case SentReport::Status::kDropped:
      return ConversionReportSendOutcome::kDropped;
  }
}

}  // namespace

const constexpr base::TimeDelta kAttributionManagerQueueReportsInterval =
    base::Minutes(30);

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
std::unique_ptr<AttributionManagerImpl>
AttributionManagerImpl::CreateForTesting(
    std::unique_ptr<AttributionReporter> reporter,
    std::unique_ptr<AttributionPolicy> policy,
    const base::Clock* clock,
    const base::FilePath& user_data_directory,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy) {
  return base::WrapUnique<AttributionManagerImpl>(new AttributionManagerImpl(
      std::move(reporter), std::move(policy), clock, user_data_directory,
      std::move(special_storage_policy)));
}

AttributionManagerImpl::AttributionManagerImpl(
    StoragePartitionImpl* storage_partition,
    const base::FilePath& user_data_directory,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy)
    : AttributionManagerImpl(
          std::make_unique<AttributionReporterImpl>(
              storage_partition,
              base::DefaultClock::GetInstance(),
              // |reporter_| is owned by |this|, so `base::Unretained()` is safe
              // as the reporter and callbacks will be deleted first.
              base::BindRepeating(&AttributionManagerImpl::OnReportSent,
                                  base::Unretained(this))),
          std::make_unique<AttributionPolicy>(
              base::CommandLine::ForCurrentProcess()->HasSwitch(
                  switches::kConversionsDebugMode)),
          base::DefaultClock::GetInstance(),
          user_data_directory,
          std::move(special_storage_policy)) {}

AttributionManagerImpl::AttributionManagerImpl(
    std::unique_ptr<AttributionReporter> reporter,
    std::unique_ptr<AttributionPolicy> policy,
    const base::Clock* clock,
    const base::FilePath& user_data_directory,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy)
    : debug_mode_(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kConversionsDebugMode)),
      clock_(clock),
      reporter_(std::move(reporter)),
      attribution_storage_(base::SequenceBound<AttributionStorageSql>(
          g_storage_task_runner.Get(),
          user_data_directory,
          std::make_unique<AttributionStorageDelegateImpl>(debug_mode_),
          clock_)),
      attribution_policy_(std::move(policy)),
      special_storage_policy_(std::move(special_storage_policy)),
      weak_factory_(this) {
  // Once the database is loaded, get all reports that may have expired while
  // Chrome was not running and handle these specially. It is safe to post tasks
  // to the storage context as soon as it is created.
  GetAndHandleReports(
      base::BindOnce(&AttributionManagerImpl::OnGetReportsToSend,
                     weak_factory_.GetWeakPtr()),
      clock_->Now() + kAttributionManagerQueueReportsInterval);

  // Start a repeating timer that will fetch reports once every
  // |kAttributionManagerQueueReportsInterval| and add them to |reporter_|.
  get_and_queue_reports_timer_.Start(
      FROM_HERE, kAttributionManagerQueueReportsInterval, this,
      &AttributionManagerImpl::GetAndQueueReportsForNextInterval);
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
                std::move(session_only_origin_predicate));
}

void AttributionManagerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AttributionManagerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AttributionManagerImpl::HandleSource(StorableSource source) {
  // Process attributions in the order in which they were received, processing
  // the new one after any background attributions are flushed.
  GetContentClient()->browser()->FlushBackgroundAttributions(
      base::BindOnce(&AttributionManagerImpl::HandleSourceInternal,
                     weak_factory_.GetWeakPtr(), std::move(source)));
}

void AttributionManagerImpl::HandleSourceInternal(StorableSource source) {
  // Only retrieve deactivated sources if an observer is there to hear it.
  // Technically, an observer could be registered between the time the async
  // call is made and the time the response is received, but this is unlikely.
  int deactivated_source_return_limit = observers_.empty() ? 0 : 50;
  attribution_storage_.AsyncCall(&AttributionStorage::StoreSource)
      .WithArgs(std::move(source), deactivated_source_return_limit)
      .Then(base::BindOnce(
          [](base::WeakPtr<AttributionManagerImpl> manager,
             std::vector<AttributionStorage::DeactivatedSource>
                 deactivated_sources) {
            if (!manager)
              return;

            manager->NotifySourcesChanged();

            for (const auto& source : deactivated_sources) {
              manager->NotifySourceDeactivated(source);
            }
          },
          weak_factory_.GetWeakPtr()));
}

void AttributionManagerImpl::HandleTrigger(StorableTrigger trigger) {
  GetContentClient()->browser()->FlushBackgroundAttributions(
      base::BindOnce(&AttributionManagerImpl::HandleTriggerInternal,
                     weak_factory_.GetWeakPtr(), std::move(trigger)));
}

void AttributionManagerImpl::HandleTriggerInternal(StorableTrigger trigger) {
  attribution_storage_.AsyncCall(&AttributionStorage::MaybeCreateAndStoreReport)
      .WithArgs(std::move(trigger))
      .Then(base::BindOnce(&AttributionManagerImpl::OnReportStored,
                           weak_factory_.GetWeakPtr()));

  // If we are running in debug mode, we should also schedule a task to
  // gather and send any new reports.
  if (debug_mode_)
    GetAndQueueReportsForNextInterval();
}

void AttributionManagerImpl::OnReportStored(CreateReportResult result) {
  RecordCreateReportStatus(result.status());

  if (result.status() != CreateReportResult::Status::kInternalError) {
    // Sources are changed here because storing a report can cause sources to be
    // deleted or become associated with a dedup key.
    NotifySourcesChanged();
    NotifyReportsChanged();
  }

  if (!result.dropped_report().has_value())
    return;

  if (absl::optional<AttributionStorage::DeactivatedSource> source =
          result.GetDeactivatedSource()) {
    NotifySourceDeactivated(*source);
  }

  for (Observer& observer : observers_)
    observer.OnReportDropped(result);
}

void AttributionManagerImpl::GetActiveSourcesForWebUI(
    base::OnceCallback<void(std::vector<StorableSource>)> callback) {
  const int kMaxSources = 1000;
  attribution_storage_.AsyncCall(&AttributionStorage::GetActiveSources)
      .WithArgs(kMaxSources)
      .Then(std::move(callback));
}

void AttributionManagerImpl::GetPendingReportsForWebUI(
    base::OnceCallback<void(std::vector<AttributionReport>)> callback) {
  const int kMaxReports = 1000;
  GetAndHandleReports(std::move(callback),
                      /*max_report_time=*/base::Time::Max(), kMaxReports);
}

void AttributionManagerImpl::SendReportsForWebUI(base::OnceClosure done) {
  GetAndHandleReports(
      base::BindOnce(&AttributionManagerImpl::OnGetReportsToSendFromWebUI,
                     weak_factory_.GetWeakPtr(), std::move(done)),
      base::Time::Max());
}

const AttributionPolicy& AttributionManagerImpl::GetAttributionPolicy() const {
  return *attribution_policy_;
}

void AttributionManagerImpl::ClearData(
    base::Time delete_begin,
    base::Time delete_end,
    base::RepeatingCallback<bool(const url::Origin&)> filter,
    base::OnceClosure done) {
  reporter_->RemoveAllReportsFromQueue();
  attribution_storage_.AsyncCall(&AttributionStorage::ClearData)
      .WithArgs(delete_begin, delete_end, std::move(filter))
      .Then(base::BindOnce(
          [](base::OnceClosure done,
             base::WeakPtr<AttributionManagerImpl> manager) {
            if (manager) {
              manager->GetAndQueueReportsForNextInterval();
              manager->NotifySourcesChanged();
              manager->NotifyReportsChanged();
            }
            std::move(done).Run();
          },
          std::move(done), weak_factory_.GetWeakPtr()));
}

void AttributionManagerImpl::GetAndHandleReports(
    ReportsHandlerFunc handler_function,
    base::Time max_report_time,
    int limit) {
  attribution_storage_.AsyncCall(&AttributionStorage::GetAttributionsToReport)
      .WithArgs(max_report_time, limit)
      .Then(std::move(handler_function));
}

void AttributionManagerImpl::GetAndQueueReportsForNextInterval() {
  // Get all the reports that will be reported in the next interval and them to
  // the |reporter_|.
  GetAndHandleReports(
      base::BindOnce(&AttributionManagerImpl::OnGetReportsToSend,
                     weak_factory_.GetWeakPtr()),
      clock_->Now() + kAttributionManagerQueueReportsInterval);
}

void AttributionManagerImpl::OnGetReportsToSend(
    std::vector<AttributionReport> reports) {
  RemoveAlreadyQueuedReports(reports);

  if (reports.empty())
    return;

  // Add delay to all reports that expired while the browser was not able to
  // send reports (or not running) so they are not temporally joinable.
  base::Time current_time = clock_->Now();
  for (AttributionReport& report : reports) {
    if (report.report_time >= current_time)
      continue;

    report.report_time =
        attribution_policy_->GetReportTimeForReportPastSendTime(current_time);
  }

  AddReportsToReporter(std::move(reports));
}

void AttributionManagerImpl::OnGetReportsToSendFromWebUI(
    base::OnceClosure done,
    std::vector<AttributionReport> reports) {
  RemoveAlreadyQueuedReports(reports);

  // If there's already a send-all in progress, ignore this request.
  if (reports.empty() || !send_reports_for_web_ui_callback_.is_null()) {
    std::move(done).Run();
    return;
  }

  std::vector<AttributionReport::Id> report_ids;
  report_ids.reserve(reports.size());
  base::Time now = clock_->Now();
  // All reports should be sent immediately.
  for (AttributionReport& report : reports) {
    report.report_time = now;
    DCHECK(report.conversion_id.has_value());
    report_ids.push_back(*report.conversion_id);
  }

  pending_report_ids_for_internals_ui_ =
      base::flat_set<AttributionReport::Id>(std::move(report_ids));
  send_reports_for_web_ui_callback_ = std::move(done);

  AddReportsToReporter(std::move(reports));
}

void AttributionManagerImpl::RemoveAlreadyQueuedReports(
    std::vector<AttributionReport>& reports) const {
  reports.erase(base::ranges::remove_if(
                    reports,
                    [&](const AttributionReport& report) {
                      DCHECK(report.conversion_id.has_value());
                      return queued_reports_.contains(*report.conversion_id);
                    }),
                reports.end());
}

void AttributionManagerImpl::AddReportsToReporter(
    std::vector<AttributionReport> reports) {
  DCHECK(base::ranges::none_of(reports, [&](const AttributionReport& report) {
    DCHECK(report.conversion_id.has_value());
    return queued_reports_.contains(*report.conversion_id);
  }));

  // This is more efficient than calling `flat_set::insert()` repeatedly.
  std::vector<AttributionReport::Id> queued_reports =
      std::move(queued_reports_).extract();
  queued_reports.reserve(queued_reports.size() + reports.size());
  for (const auto& report : reports) {
    queued_reports.push_back(*report.conversion_id);
  }
  queued_reports_ =
      base::flat_set<AttributionReport::Id>(std::move(queued_reports));

  reporter_->AddReportsToQueue(std::move(reports));
}

void AttributionManagerImpl::OnReportSent(SentReport info) {
  DCHECK(info.report.conversion_id.has_value());

  // If there was a transient failure, and another attempt is allowed,
  // update the report's DB state to reflect that. Otherwise, delete the report
  // from storage if it wasn't skipped due to the browser being offline.

  bool should_retry = false;
  if (info.status == SentReport::Status::kTransientFailure) {
    info.report.failed_send_attempts++;
    const absl::optional<base::TimeDelta> delay =
        attribution_policy_->GetFailedReportDelay(
            info.report.failed_send_attempts);
    if (delay.has_value()) {
      should_retry = true;
      info.report.report_time += *delay;
    }
  }

  if (should_retry) {
    // After updating the report's failure count and new report time in the DB,
    // add it directly to the queue so that the retry is attempted as the new
    // report time is reached, rather than wait for the next DB-polling to
    // occur.
    attribution_storage_
        .AsyncCall(&AttributionStorage::UpdateReportForSendFailure)
        .WithArgs(*info.report.conversion_id, info.report.report_time)
        .Then(base::BindOnce(
            [](base::WeakPtr<AttributionManagerImpl> manager,
               AttributionReport report, bool success) {
              if (!manager || !success)
                return;
              // We add the report directly back to the reporter instead of
              // using `AddReportsToReporter()` to avoid having to remove and
              // then immediately re-add the report ID to the set.
              manager->reporter_->AddReportsToQueue({std::move(report)});

              manager->NotifyReportsChanged();
            },
            weak_factory_.GetWeakPtr(), info.report));
  } else if (info.status == SentReport::Status::kOffline ||
             info.status == SentReport::Status::kRemovedFromQueue) {
    // Remove the ID from the set so that subsequent attempts will not be
    // deduplicated.
    size_t num_removed = queued_reports_.erase(info.report.conversion_id);
    DCHECK_EQ(num_removed, 1u);
  } else {
    RecordDeleteEvent(DeleteEvent::kStarted);
    attribution_storage_.AsyncCall(&AttributionStorage::DeleteReport)
        .WithArgs(*info.report.conversion_id)
        .Then(base::BindOnce(
            [](base::WeakPtr<AttributionManagerImpl> manager,
               AttributionReport::Id report_id, bool succeeded) {
              RecordDeleteEvent(succeeded ? DeleteEvent::kSucceeded
                                          : DeleteEvent::kFailed);

              if (manager && succeeded) {
                // Only remove the ID from the set once deletion has succeeded
                // in order to avoid duplicates.
                size_t num_removed = manager->queued_reports_.erase(report_id);
                DCHECK_EQ(num_removed, 1u);

                manager->NotifyReportsChanged();
              }
            },
            weak_factory_.GetWeakPtr(), *info.report.conversion_id));

    base::UmaHistogramEnumeration(
        "Conversion.ReportSendOutcome",
        ConvertToConversionReportSendOutcome(info.status));
  }

  DCHECK_EQ(send_reports_for_web_ui_callback_.is_null(),
            pending_report_ids_for_internals_ui_.empty());

  // If there's a `SendReportsForWebUI()` callback waiting on this report's
  // ID, remove the ID from the wait-set; if it was the last such ID,
  // run the callback.
  if (!send_reports_for_web_ui_callback_.is_null() &&
      pending_report_ids_for_internals_ui_.erase(*info.report.conversion_id) >
          0 &&
      pending_report_ids_for_internals_ui_.empty()) {
    std::move(send_reports_for_web_ui_callback_).Run();
  }

  // TODO(apaseltiner): Consider surfacing retry attempts in internals UI.
  if (info.status != SentReport::Status::kSent &&
      info.status != SentReport::Status::kFailure &&
      info.status != SentReport::Status::kDropped) {
    return;
  }

  for (Observer& observer : observers_)
    observer.OnReportSent(info);
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
