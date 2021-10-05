// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/conversion_manager_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
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
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"

namespace content {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ConversionReportSendOutcome {
  kSent = 0,
  kFailed = 1,
  kDropped = 2,
  kMaxValue = kDropped
};

const size_t kMaxSentReportsToStore = 100;

// The shared-task runner for all conversion storage operations. Note that
// different ConversionManagerImpl perform operations on the same task
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

void RecordCreateReportStatus(
    AttributionStorage::CreateReportResult::Status status) {
  base::UmaHistogramEnumeration("Conversions.CreateReportStatus", status);
}

// We measure this in order to be able to count reports that weren't
// successfully deleted, which can lead to duplicate reports.
void RecordDeleteEvent(ConversionManagerImpl::DeleteEvent event) {
  base::UmaHistogramEnumeration("Conversions.DeleteSentReportOperation", event);
}

ConversionReportSendOutcome ConvertToConversionReportSendOutcome(
    SentReportInfo::Status status) {
  switch (status) {
    case SentReportInfo::Status::kSent:
      return ConversionReportSendOutcome::kSent;
    case SentReportInfo::Status::kTransientFailure:
    case SentReportInfo::Status::kFailure:
      return ConversionReportSendOutcome::kFailed;
    case SentReportInfo::Status::kOffline:
    case SentReportInfo::Status::kRemovedFromQueue:
      // Offline reports and reports removed from the queue before being sent
      // should never record an outcome.
      NOTREACHED();
      return ConversionReportSendOutcome::kFailed;
    case SentReportInfo::Status::kDropped:
      return ConversionReportSendOutcome::kDropped;
  }
}

}  // namespace

const constexpr base::TimeDelta kConversionManagerQueueReportsInterval =
    base::Minutes(30);

ConversionManager* ConversionManagerProviderImpl::GetManager(
    WebContents* web_contents) const {
  return static_cast<StoragePartitionImpl*>(
             web_contents->GetBrowserContext()->GetDefaultStoragePartition())
      ->GetConversionManager();
}

// static
void ConversionManagerImpl::RunInMemoryForTesting() {
  AttributionStorageSql::RunInMemoryForTesting();
}

// static
std::unique_ptr<ConversionManagerImpl> ConversionManagerImpl::CreateForTesting(
    std::unique_ptr<AttributionReporter> reporter,
    std::unique_ptr<AttributionPolicy> policy,
    const base::Clock* clock,
    const base::FilePath& user_data_directory,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
    size_t max_sent_reports_to_store) {
  return base::WrapUnique<ConversionManagerImpl>(new ConversionManagerImpl(
      std::move(reporter), std::move(policy), clock, user_data_directory,
      std::move(special_storage_policy), max_sent_reports_to_store));
}

ConversionManagerImpl::ConversionManagerImpl(
    StoragePartitionImpl* storage_partition,
    const base::FilePath& user_data_directory,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy)
    : ConversionManagerImpl(
          std::make_unique<AttributionReporterImpl>(
              storage_partition,
              base::DefaultClock::GetInstance(),
              // |reporter_| is owned by |this|, so `base::Unretained()` is safe
              // as the reporter and callbacks will be deleted first.
              base::BindRepeating(&ConversionManagerImpl::OnReportSent,
                                  base::Unretained(this))),
          std::make_unique<AttributionPolicy>(
              base::CommandLine::ForCurrentProcess()->HasSwitch(
                  switches::kConversionsDebugMode)),
          base::DefaultClock::GetInstance(),
          user_data_directory,
          std::move(special_storage_policy),
          kMaxSentReportsToStore) {}

ConversionManagerImpl::ConversionManagerImpl(
    std::unique_ptr<AttributionReporter> reporter,
    std::unique_ptr<AttributionPolicy> policy,
    const base::Clock* clock,
    const base::FilePath& user_data_directory,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
    size_t max_sent_reports_to_store)
    : debug_mode_(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kConversionsDebugMode)),
      clock_(clock),
      reporter_(std::move(reporter)),
      attribution_storage_(base::SequenceBound<AttributionStorageSql>(
          g_storage_task_runner.Get(),
          user_data_directory,
          std::make_unique<AttributionStorageDelegateImpl>(debug_mode_),
          clock_)),
      session_storage_(max_sent_reports_to_store),
      attribution_policy_(std::move(policy)),
      special_storage_policy_(std::move(special_storage_policy)),
      weak_factory_(this) {
  // Once the database is loaded, get all reports that may have expired while
  // Chrome was not running and handle these specially. It is safe to post tasks
  // to the storage context as soon as it is created.
  GetAndHandleReports(base::BindOnce(&ConversionManagerImpl::QueueReports,
                                     weak_factory_.GetWeakPtr()),
                      clock_->Now() + kConversionManagerQueueReportsInterval);

  // Start a repeating timer that will fetch reports once every
  // |kConversionManagerQueueReportsInterval| and add them to |reporter_|.
  get_and_queue_reports_timer_.Start(
      FROM_HERE, kConversionManagerQueueReportsInterval, this,
      &ConversionManagerImpl::GetAndQueueReportsForNextInterval);
}

ConversionManagerImpl::~ConversionManagerImpl() {
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

void ConversionManagerImpl::HandleImpression(StorableSource impression) {
  // Add the impression to storage.
  attribution_storage_.AsyncCall(&AttributionStorage::StoreImpression)
      .WithArgs(std::move(impression));
}

void ConversionManagerImpl::HandleConversion(StorableTrigger conversion) {
  attribution_storage_
      .AsyncCall(&AttributionStorage::MaybeCreateAndStoreConversionReport)
      .WithArgs(std::move(conversion))
      .Then(base::BindOnce(&ConversionManagerImpl::OnReportStored,
                           weak_factory_.GetWeakPtr()));

  // If we are running in debug mode, we should also schedule a task to
  // gather and send any new reports.
  if (debug_mode_)
    GetAndQueueReportsForNextInterval();
}

void ConversionManagerImpl::OnReportStored(
    AttributionStorage::CreateReportResult result) {
  RecordCreateReportStatus(result.status());
  if (!result.dropped_report().has_value())
    return;

  session_storage_.AddDroppedReport(std::move(result));
}

void ConversionManagerImpl::GetActiveImpressionsForWebUI(
    base::OnceCallback<void(std::vector<StorableSource>)> callback) {
  const int kMaxImpressions = 1000;
  attribution_storage_.AsyncCall(&AttributionStorage::GetActiveImpressions)
      .WithArgs(kMaxImpressions)
      .Then(std::move(callback));
}

void ConversionManagerImpl::GetPendingReportsForWebUI(
    base::OnceCallback<void(std::vector<AttributionReport>)> callback,
    base::Time max_report_time) {
  const int kMaxReports = 1000;
  GetAndHandleReports(std::move(callback), max_report_time, kMaxReports);
}

const AttributionSessionStorage& ConversionManagerImpl::GetSessionStorage()
    const {
  return session_storage_;
}

void ConversionManagerImpl::SendReportsForWebUI(base::OnceClosure done) {
  GetAndHandleReports(
      base::BindOnce(&ConversionManagerImpl::HandleReportsSentFromWebUI,
                     weak_factory_.GetWeakPtr(), std::move(done)),
      base::Time::Max());
}

const AttributionPolicy& ConversionManagerImpl::GetAttributionPolicy() const {
  return *attribution_policy_;
}

void ConversionManagerImpl::ClearData(
    base::Time delete_begin,
    base::Time delete_end,
    base::RepeatingCallback<bool(const url::Origin&)> filter,
    base::OnceClosure done) {
  session_storage_.Reset();
  reporter_->RemoveAllReportsFromQueue();
  attribution_storage_.AsyncCall(&AttributionStorage::ClearData)
      .WithArgs(delete_begin, delete_end, std::move(filter))
      .Then(base::BindOnce(
          [](base::OnceClosure done,
             base::WeakPtr<ConversionManagerImpl> manager) {
            std::move(done).Run();
            if (manager)
              manager->GetAndQueueReportsForNextInterval();
          },
          std::move(done), weak_factory_.GetWeakPtr()));
}

void ConversionManagerImpl::GetAndHandleReports(
    ReportsHandlerFunc handler_function,
    base::Time max_report_time,
    int limit) {
  attribution_storage_.AsyncCall(&AttributionStorage::GetConversionsToReport)
      .WithArgs(max_report_time, limit)
      .Then(std::move(handler_function));
}

void ConversionManagerImpl::GetAndQueueReportsForNextInterval() {
  // Get all the reports that will be reported in the next interval and them to
  // the |reporter_|.
  GetAndHandleReports(base::BindOnce(&ConversionManagerImpl::QueueReports,
                                     weak_factory_.GetWeakPtr()),
                      clock_->Now() + kConversionManagerQueueReportsInterval);
}

void ConversionManagerImpl::QueueReports(
    std::vector<AttributionReport> reports) {
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

  reporter_->AddReportsToQueue(std::move(reports));
}

void ConversionManagerImpl::HandleReportsSentFromWebUI(
    base::OnceClosure done,
    std::vector<AttributionReport> reports) {
  // If there's already a send-all in progress, ignore this request.
  if (reports.empty() || !send_reports_for_web_ui_callback_.is_null()) {
    std::move(done).Run();
    return;
  }

  std::vector<AttributionReport::Id> conversion_ids;
  conversion_ids.reserve(reports.size());
  base::Time now = clock_->Now();
  // All reports should be sent immediately.
  for (AttributionReport& report : reports) {
    report.report_time = now;
    DCHECK(report.conversion_id.has_value());
    conversion_ids.push_back(*report.conversion_id);
  }

  // Reports may already be in the process of sending, but they will be
  // deduplicated by `AttributionReporterImpl::AddReportsToQueue()`. In that
  // case, the callback will be invoked as a result of the in-process reports.
  pending_conversion_ids_for_internals_ui_ =
      base::flat_set<AttributionReport::Id>(std::move(conversion_ids));
  send_reports_for_web_ui_callback_ = std::move(done);

  reporter_->AddReportsToQueue(std::move(reports));
}

void ConversionManagerImpl::OnReportSent(SentReportInfo info) {
  DCHECK(info.report.conversion_id.has_value());

  // If there was a transient failure, and another attempt is allowed,
  // update the report's DB state to reflect that. Otherwise, delete the report
  // from storage if it wasn't skipped due to the browser being offline.

  bool should_retry = false;
  if (info.status == SentReportInfo::Status::kTransientFailure) {
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
            [](base::WeakPtr<ConversionManagerImpl> manager,
               AttributionReport report, bool success) {
              if (!manager || !success)
                return;
              manager->reporter_->AddReportsToQueue({std::move(report)});
            },
            weak_factory_.GetWeakPtr(), info.report));
  } else if (info.status != SentReportInfo::Status::kOffline &&
             info.status != SentReportInfo::Status::kRemovedFromQueue) {
    RecordDeleteEvent(DeleteEvent::kStarted);
    attribution_storage_.AsyncCall(&AttributionStorage::DeleteConversion)
        .WithArgs(*info.report.conversion_id)
        .Then(base::BindOnce([](bool succeeded) {
          RecordDeleteEvent(succeeded ? DeleteEvent::kSucceeded
                                      : DeleteEvent::kFailed);
        }));

    base::UmaHistogramEnumeration(
        "Conversion.ReportSendOutcome",
        ConvertToConversionReportSendOutcome(info.status));
  }

  DCHECK_EQ(send_reports_for_web_ui_callback_.is_null(),
            pending_conversion_ids_for_internals_ui_.empty());

  // If there's a `SendReportsForWebUI()` callback waiting on this report's
  // conversion ID, remove the ID from the wait-set; if it was the last such ID,
  // run the callback.
  if (!send_reports_for_web_ui_callback_.is_null() &&
      pending_conversion_ids_for_internals_ui_.erase(
          *info.report.conversion_id) > 0 &&
      pending_conversion_ids_for_internals_ui_.empty()) {
    std::move(send_reports_for_web_ui_callback_).Run();
  }

  // TODO(apaseltiner): Consider surfacing retry attempts in internals UI.
  if (info.status != SentReportInfo::Status::kSent &&
      info.status != SentReportInfo::Status::kFailure)
    return;

  session_storage_.AddSentReport(std::move(info));
}

}  // namespace content
