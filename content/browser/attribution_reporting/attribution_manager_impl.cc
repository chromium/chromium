// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_manager_impl.h"

#include <cmath>
#include <set>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/overloaded.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task/updateable_sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "content/browser/aggregation_service/aggregation_service.h"
#include "content/browser/aggregation_service/aggregation_service_impl.h"
#include "content/browser/aggregation_service/report_scheduler_timer.h"
#include "content/browser/attribution_reporting/aggregatable_attribution_utils.h"
#include "content/browser/attribution_reporting/attribution_cookie_checker.h"
#include "content/browser/attribution_reporting/attribution_cookie_checker_impl.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager_impl.h"
#include "content/browser/attribution_reporting/attribution_debug_report.h"
#include "content/browser/attribution_reporting/attribution_features.h"
#include "content/browser/attribution_reporting/attribution_info.h"
#include "content/browser/attribution_reporting/attribution_observer.h"
#include "content/browser/attribution_reporting/attribution_os_level_manager.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_report_network_sender.h"
#include "content/browser/attribution_reporting/attribution_report_sender.h"
#include "content/browser/attribution_reporting/attribution_reporting.mojom.h"
#include "content/browser/attribution_reporting/attribution_storage.h"
#include "content/browser/attribution_reporting/attribution_storage_delegate.h"
#include "content/browser/attribution_reporting/attribution_storage_delegate_impl.h"
#include "content/browser/attribution_reporting/attribution_storage_sql.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/create_report_result.h"
#include "content/browser/attribution_reporting/os_registration.h"
#include "content/browser/attribution_reporting/send_result.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/store_source_result.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/browser/browsing_data/browsing_data_filter_builder_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/attribution_data_model.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/cpp/attribution_utils.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/attribution.mojom.h"
#include "services/network/public/mojom/network_change_manager.mojom-forward.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/attribution_reporting/attribution_os_level_manager_android.h"
#endif

namespace content {

namespace {

using ScopedUseInMemoryStorageForTesting =
    ::content::AttributionManagerImpl::ScopedUseInMemoryStorageForTesting;

using ::attribution_reporting::mojom::OsRegistrationResult;
using ::attribution_reporting::mojom::RegistrationType;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ConversionReportSendOutcome {
  kSent = 0,
  kFailed = 1,
  kDropped = 2,
  kFailedToAssemble = 3,
  kMaxValue = kFailedToAssemble,
};

// This class consolidates logic regarding when to schedule the browser to send
// attribution reports. It talks directly to the `AttributionStorage` to help
// make these decisions.
//
// While the class does not make large changes to the underlying database, it
// is responsible for notifying the `AttributionStorage` when the browser comes
// back online, which mutates report times for some scheduled reports.
//
// TODO(apaseltiner): Consider making this class an observer to allow it to
// manage when to schedule things.
class AttributionReportScheduler : public ReportSchedulerTimer::Delegate {
 public:
  AttributionReportScheduler(
      base::RepeatingClosure send_reports,
      base::RepeatingClosure on_reporting_paused_cb,
      base::SequenceBound<AttributionStorage>& attribution_storage)
      : send_reports_(std::move(send_reports)),
        on_reporting_paused_cb_(std::move(on_reporting_paused_cb)),
        attribution_storage_(attribution_storage) {}
  ~AttributionReportScheduler() override = default;

  AttributionReportScheduler(const AttributionReportScheduler&) = delete;
  AttributionReportScheduler& operator=(const AttributionReportScheduler&) =
      delete;
  AttributionReportScheduler(AttributionReportScheduler&&) = delete;
  AttributionReportScheduler& operator=(AttributionReportScheduler&&) = delete;

 private:
  // ReportSchedulerTimer::Delegate:
  void GetNextReportTime(
      base::OnceCallback<void(absl::optional<base::Time>)> callback,
      base::Time now) override {
    attribution_storage_->AsyncCall(&AttributionStorage::GetNextReportTime)
        .WithArgs(now)
        .Then(std::move(callback));
  }
  void OnReportingTimeReached(base::Time now) override { send_reports_.Run(); }
  void AdjustOfflineReportTimes(
      base::OnceCallback<void(absl::optional<base::Time>)> maybe_set_timer_cb)
      override {
    // Add delay to all reports that should have been sent while the browser was
    // offline so they are not temporally joinable. We do this in storage to
    // avoid pulling an unbounded number of reports into memory, only to
    // immediately issue async storage calls to modify their report times.
    attribution_storage_
        ->AsyncCall(&AttributionStorage::AdjustOfflineReportTimes)
        .Then(std::move(maybe_set_timer_cb));
  }

  void OnReportingPaused() override { on_reporting_paused_cb_.Run(); }

  base::RepeatingClosure send_reports_;
  base::RepeatingClosure on_reporting_paused_cb_;
  const raw_ref<base::SequenceBound<AttributionStorage>> attribution_storage_;
};

bool IsStorageKeySessionOnly(
    scoped_refptr<storage::SpecialStoragePolicy> storage_policy,
    const blink::StorageKey& storage_key) {
  // TODO(johnidel): This conversion is unfortunate but necessary. Storage
  // partition clear data logic uses storage key keyed deletion, while the
  // storage policy uses GURLs. Ideally these would be coalesced.
  const GURL& url = storage_key.origin().GetURL();
  if (storage_policy->IsStorageProtected(url)) {
    return false;
  }

  if (storage_policy->IsStorageSessionOnly(url)) {
    return true;
  }
  return false;
}

void RecordStoreSourceStatus(StoreSourceResult result) {
  static_assert(StorableSource::Result::kMaxValue ==
                    StorableSource::Result::kEventReportWindowsInvalidStartTime,
                "Bump version of Conversions.SourceStoredStatus6 histogram.");
  base::UmaHistogramEnumeration("Conversions.SourceStoredStatus6",
                                result.status);
}

void RecordCreateReportStatus(CreateReportResult result) {
  static_assert(
      AttributionTrigger::EventLevelResult::kMaxValue ==
          AttributionTrigger::EventLevelResult::kReportWindowNotStarted,
      "Bump version of Conversions.CreateReportStatus8 histogram.");
  base::UmaHistogramEnumeration("Conversions.CreateReportStatus8",
                                result.event_level_status());
  static_assert(
      AttributionTrigger::AggregatableResult::kMaxValue ==
          AttributionTrigger::AggregatableResult::kExcessiveReports,
      "Bump version of Conversions.AggregatableReport.CreateReportStatus4 "
      "histogram.");
  base::UmaHistogramEnumeration(
      "Conversions.AggregatableReport.CreateReportStatus4",
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
    case SendResult::Status::kAssemblyFailure:
    case SendResult::Status::kTransientAssemblyFailure:
      return ConversionReportSendOutcome::kFailedToAssemble;
  }
}

void RecordNetworkConnectionTypeOnFailure(
    AttributionReport::Type report_type,
    network::mojom::ConnectionType connection_type) {
  switch (report_type) {
    case AttributionReport::Type::kEventLevel:
      base::UmaHistogramEnumeration(
          "Conversions.EventLevelReport.NetworkConnectionTypeOnFailure",
          connection_type);
      break;
    case AttributionReport::Type::kAggregatableAttribution:
      base::UmaHistogramEnumeration(
          "Conversions.AggregatableReport.NetworkConnectionTypeOnFailure",
          connection_type);
      break;
    case AttributionReport::Type::kNullAggregatable:
      break;
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
    case AttributionReport::Type::kEventLevel: {
      // Use a large time range to capture users that might not open the browser
      // for a long time while a conversion report is pending. Revisit this
      // range if it is non-ideal for real world data.
      const AttributionInfo& attribution_info = report.attribution_info();
      base::TimeDelta time_since_original_report_time =
          now - report.initial_report_time();
      base::UmaHistogramCustomTimes(
          "Conversions.ExtraReportDelay2", time_since_original_report_time,
          base::Seconds(1), base::Days(24), /*buckets=*/100);

      base::TimeDelta time_from_conversion_to_report_send =
          report.report_time() - attribution_info.time;
      UMA_HISTOGRAM_COUNTS_1000("Conversions.TimeFromConversionToReportSend",
                                time_from_conversion_to_report_send.InHours());

      UMA_HISTOGRAM_CUSTOM_TIMES("Conversions.SchedulerReportDelay",
                                 now - report.report_time(), base::Seconds(1),
                                 base::Days(1), 50);
      break;
    }
    case AttributionReport::Type::kAggregatableAttribution: {
      base::TimeDelta time_from_conversion_to_report_assembly =
          report.report_time() - report.attribution_info().time;
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "Conversions.AggregatableReport.TimeFromTriggerToReportAssembly2",
          time_from_conversion_to_report_assembly, base::Minutes(1),
          base::Days(24), 50);

      UMA_HISTOGRAM_CUSTOM_TIMES(
          "Conversions.AggregatableReport.ExtraReportDelay",
          now - report.initial_report_time(), base::Seconds(1), base::Days(24),
          50);

      UMA_HISTOGRAM_CUSTOM_TIMES(
          "Conversions.AggregatableReport.SchedulerReportDelay",
          now - report.report_time(), base::Seconds(1), base::Days(1), 50);
      break;
    }
    case AttributionReport::Type::kNullAggregatable:
      break;
  }
}

// Called when |report| is sent, failed or dropped, for logging metrics.
void LogMetricsOnReportCompleted(const AttributionReport& report,
                                 SendResult::Status status) {
  switch (report.GetReportType()) {
    case AttributionReport::Type::kEventLevel:
      base::UmaHistogramEnumeration(
          "Conversions.ReportSendOutcome3",
          ConvertToConversionReportSendOutcome(status));
      break;
    case AttributionReport::Type::kAggregatableAttribution:
      base::UmaHistogramEnumeration(
          "Conversions.AggregatableReport.ReportSendOutcome2",
          ConvertToConversionReportSendOutcome(status));
      break;
    case AttributionReport::Type::kNullAggregatable:
      break;
  }
}

// Called when `report` is sent successfully.
void LogMetricsOnReportSent(const AttributionReport& report) {
  base::Time now = base::Time::Now();
  base::TimeDelta time_from_conversion_to_report_sent =
      now - report.attribution_info().time;
  base::TimeDelta time_since_original_report_time =
      now - report.initial_report_time();

  switch (report.GetReportType()) {
    case AttributionReport::Type::kEventLevel:
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "Conversions.ExtraReportDelayForSuccessfulSend",
          time_since_original_report_time, base::Seconds(1), base::Days(24),
          /*bucket_count=*/100);

      UMA_HISTOGRAM_COUNTS_1000(
          "Conversions.TimeFromTriggerToReportSentSuccessfully",
          time_from_conversion_to_report_sent.InHours());
      break;
    case AttributionReport::Type::kAggregatableAttribution:
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "Conversions.AggregatableReport."
          "TimeFromTriggerToReportSentSuccessfully",
          time_from_conversion_to_report_sent, base::Minutes(1), base::Days(24),
          50);

      UMA_HISTOGRAM_CUSTOM_TIMES(
          "Conversions.AggregatableReport.ExtraReportDelayForSuccessfulSend",
          time_since_original_report_time, base::Seconds(1), base::Days(24),
          /*bucket_count=*/50);
      break;
    case AttributionReport::Type::kNullAggregatable:
      break;
  }
}

std::unique_ptr<AttributionStorageDelegate> MakeStorageDelegate(
    bool debug_mode) {
  if (debug_mode) {
    return std::make_unique<AttributionStorageDelegateImpl>(
        AttributionNoiseMode::kNone, AttributionDelayMode::kNone);
  }

  return std::make_unique<AttributionStorageDelegateImpl>(
      AttributionNoiseMode::kDefault, AttributionDelayMode::kDefault);
}

bool IsOperationAllowed(
    StoragePartitionImpl& storage_partition,
    ContentBrowserClient::AttributionReportingOperation operation,
    content::RenderFrameHost* rfh,
    const url::Origin* source_origin,
    const url::Origin* destination_origin,
    const url::Origin* reporting_origin) {
  return GetContentClient()->browser()->IsAttributionReportingOperationAllowed(
      storage_partition.browser_context(), operation, rfh, source_origin,
      destination_origin, reporting_origin);
}

std::unique_ptr<AttributionOsLevelManager> CreateOsLevelManager() {
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          network::features::kAttributionReportingCrossAppWeb)) {
    return std::make_unique<AttributionOsLevelManagerAndroid>();
  }
#endif
  return std::make_unique<NoOpAttributionOsLevelManager>();
}

bool g_run_in_memory = false;

}  // namespace

struct AttributionManagerImpl::PendingReportTimings {
  base::Time creation_time;
  base::Time report_time;
};

absl::optional<base::TimeDelta> GetFailedReportDelay(int failed_send_attempts) {
  DCHECK_GT(failed_send_attempts, 0);

  const int kMaxFailedSendAttempts = 2;
  const base::TimeDelta kInitialReportDelay = base::Minutes(5);
  const int kDelayFactor = 3;

  if (failed_send_attempts > kMaxFailedSendAttempts) {
    return absl::nullopt;
  }

  return kInitialReportDelay * std::pow(kDelayFactor, failed_send_attempts - 1);
}

ScopedUseInMemoryStorageForTesting::ScopedUseInMemoryStorageForTesting()
    : previous_(g_run_in_memory) {
  g_run_in_memory = true;
}

ScopedUseInMemoryStorageForTesting::~ScopedUseInMemoryStorageForTesting() {
  g_run_in_memory = previous_;
}

bool AttributionManagerImpl::IsReportAllowed(
    const AttributionReport& report) const {
  const attribution_reporting::SuitableOrigin* source_origin = absl::visit(
      base::Overloaded{
          [](const AttributionReport::EventLevelData& data) {
            return &data.source.common_info().source_origin();
          },
          [](const AttributionReport::AggregatableAttributionData& data) {
            return &data.source.common_info().source_origin();
          },
          [&](const AttributionReport::NullAggregatableData&) {
            return &report.attribution_info().context_origin;
          },
      },
      report.data());
  return IsOperationAllowed(
      *storage_partition_,
      ContentBrowserClient::AttributionReportingOperation::kReport,
      /*rfh=*/nullptr, &**source_origin,
      &*report.attribution_info().context_origin,
      &*report.GetReportingOrigin());
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
    std::unique_ptr<AttributionOsLevelManager> os_level_manager,
    StoragePartitionImpl* storage_partition,
    scoped_refptr<base::UpdateableSequencedTaskRunner> storage_task_runner) {
  return base::WrapUnique(new AttributionManagerImpl(
      storage_partition, user_data_directory, max_pending_events,
      std::move(special_storage_policy), std::move(storage_delegate),
      std::move(cookie_checker), std::move(report_sender),
      std::move(os_level_manager), std::move(storage_task_runner)));
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
          MakeStorageDelegate(base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kAttributionReportingDebugMode)),
          std::make_unique<AttributionCookieCheckerImpl>(storage_partition),
          std::make_unique<AttributionReportNetworkSender>(
              storage_partition->GetURLLoaderFactoryForBrowserProcess()),
          CreateOsLevelManager(),
          // This uses BLOCK_SHUTDOWN as some data deletion operations may be
          // running when the browser is closed, and we want to ensure all data
          // is deleted correctly. Additionally, we use MUST_USE_FOREGROUND to
          // avoid priority inversions if a task is already running when the
          // priority is increased.
          base::ThreadPool::CreateUpdateableSequencedTaskRunner(
              base::TaskTraits(base::TaskPriority::BEST_EFFORT,
                               base::MayBlock(),
                               base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
                               base::ThreadPolicy::MUST_USE_FOREGROUND))) {
}  // namespace content

AttributionManagerImpl::AttributionManagerImpl(
    StoragePartitionImpl* storage_partition,
    const base::FilePath& user_data_directory,
    size_t max_pending_events,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
    std::unique_ptr<AttributionStorageDelegate> storage_delegate,
    std::unique_ptr<AttributionCookieChecker> cookie_checker,
    std::unique_ptr<AttributionReportSender> report_sender,
    std::unique_ptr<AttributionOsLevelManager> os_level_manager,
    scoped_refptr<base::UpdateableSequencedTaskRunner> storage_task_runner)
    : storage_partition_(
          raw_ref<StoragePartitionImpl>::from_ptr(storage_partition)),
      max_pending_events_(max_pending_events),
      storage_task_runner_(std::move(storage_task_runner)),
      attribution_storage_(base::SequenceBound<AttributionStorageSql>(
          storage_task_runner_,
          g_run_in_memory ? base::FilePath() : user_data_directory,
          std::move(storage_delegate))),
      scheduler_timer_(std::make_unique<AttributionReportScheduler>(
          base::BindRepeating(&AttributionManagerImpl::GetReportsToSend,
                              base::Unretained(this)),
          base::BindRepeating(
              &AttributionManagerImpl::RecordPendingAggregatableReportsTimings,
              base::Unretained(this)),
          attribution_storage_)),
      data_host_manager_(
          std::make_unique<AttributionDataHostManagerImpl>(this)),
      special_storage_policy_(std::move(special_storage_policy)),
      cookie_checker_(std::move(cookie_checker)),
      report_sender_(std::move(report_sender)),
      os_level_manager_(std::move(os_level_manager)) {
  DCHECK_GT(max_pending_events_, 0u);
  DCHECK(storage_task_runner_);
  DCHECK(cookie_checker_);
  DCHECK(report_sender_);
  DCHECK(os_level_manager_);
}

AttributionManagerImpl::~AttributionManagerImpl() {
  RecordPendingAggregatableReportsTimings();

  // Browser contexts are not required to have a special storage policy.
  if (!special_storage_policy_ ||
      !special_storage_policy_->HasSessionOnlyOrigins()) {
    return;
  }

  // Delete stored data for all session only origins given by
  // |special_storage_policy|.
  StoragePartition::StorageKeyMatcherFunction
      session_only_storage_key_predicate = base::BindRepeating(
          &IsStorageKeySessionOnly, std::move(special_storage_policy_));
  ClearData(base::Time::Min(), base::Time::Max(),
            std::move(session_only_storage_key_predicate),
            /*filter_builder=*/nullptr,
            /*delete_rate_limit_data=*/true, /*done=*/base::DoNothing());
}

void AttributionManagerImpl::AddObserver(AttributionObserver* observer) {
  observers_.AddObserver(observer);
}

void AttributionManagerImpl::RemoveObserver(AttributionObserver* observer) {
  observers_.RemoveObserver(observer);
}

AttributionDataHostManager* AttributionManagerImpl::GetDataHostManager() {
  DCHECK(data_host_manager_);
  return data_host_manager_.get();
}

void AttributionManagerImpl::HandleSource(
    StorableSource source,
    GlobalRenderFrameHostId render_frame_id) {
  bool allowed = IsOperationAllowed(
      *storage_partition_,
      ContentBrowserClient::AttributionReportingOperation::kSource,
      RenderFrameHost::FromID(render_frame_id),
      &*source.common_info().source_origin(),
      /*destination_origin=*/nullptr,
      &*source.common_info().reporting_origin());
  if (!allowed) {
    OnSourceStored(
        source,
        /*cleared_debug_key=*/absl::nullopt,
        /*is_debug_cookie_set=*/false,
        StoreSourceResult(StorableSource::Result::kProhibitedByBrowserPolicy));
    return;
  }

  MaybeEnqueueEvent(std::move(source));
}

void AttributionManagerImpl::RecordPendingAggregatableReportsTimings() {
  const base::Time now = base::Time::Now();

  for (const auto& [key, timing] : pending_aggregatable_reports_) {
    UMA_HISTOGRAM_LONG_TIMES(
        "Conversions.AggregatableReport.PendingAndBrowserWentOffline."
        "TimeSinceCreation",
        now - timing.creation_time);
    UMA_HISTOGRAM_LONG_TIMES(
        "Conversions.AggregatableReport.PendingAndBrowserWentOffline."
        "TimeUntilReportTime",
        timing.report_time - now);
  }
  pending_aggregatable_reports_.clear();
}

void AttributionManagerImpl::OnSourceStored(
    const StorableSource& source,
    absl::optional<uint64_t> cleared_debug_key,
    bool is_debug_cookie_set,
    StoreSourceResult result) {
  RecordStoreSourceStatus(result);

  base::Time now = base::Time::Now();
  for (auto& observer : observers_) {
    observer.OnSourceHandled(source, now, cleared_debug_key, result.status);
  }

  scheduler_timer_.MaybeSet(result.min_fake_report_time);

  NotifySourcesChanged();

  MaybeSendVerboseDebugReport(source, is_debug_cookie_set, result);
}

void AttributionManagerImpl::HandleTrigger(
    AttributionTrigger trigger,
    GlobalRenderFrameHostId render_frame_id) {
  bool allowed = IsOperationAllowed(
      *storage_partition_,
      ContentBrowserClient::AttributionReportingOperation::kTrigger,
      RenderFrameHost::FromID(render_frame_id),
      /*source_origin=*/nullptr, &*trigger.destination_origin(),
      &*trigger.reporting_origin());
  if (!allowed) {
    OnReportStored(
        trigger,
        /*cleared_debug_key=*/absl::nullopt, /*is_debug_cookie_set=*/false,
        CreateReportResult(
            /*trigger_time=*/base::Time::Now(),
            AttributionTrigger::EventLevelResult::kProhibitedByBrowserPolicy,
            AttributionTrigger::AggregatableResult::
                kProhibitedByBrowserPolicy));
    return;
  }

  MaybeEnqueueEvent(std::move(trigger));
}

void AttributionManagerImpl::StoreTrigger(AttributionTrigger trigger,
                                          bool is_debug_cookie_set) {
  absl::optional<uint64_t> cleared_debug_key;
  if (!is_debug_cookie_set) {
    cleared_debug_key =
        std::exchange(trigger.registration().debug_key, absl::nullopt);
  }

  attribution_storage_.AsyncCall(&AttributionStorage::MaybeCreateAndStoreReport)
      .WithArgs(trigger)
      .Then(base::BindOnce(&AttributionManagerImpl::OnReportStored,
                           weak_factory_.GetWeakPtr(), std::move(trigger),
                           cleared_debug_key, is_debug_cookie_set));
}

void AttributionManagerImpl::MaybeEnqueueEvent(SourceOrTrigger event) {
  const size_t size_before_push = pending_events_.size();

  // Avoid unbounded memory growth with adversarial input.
  bool allowed = size_before_push < max_pending_events_;
  base::UmaHistogramBoolean("Conversions.EnqueueEventAllowed", allowed);
  if (!allowed) {
    return;
  }

  pending_events_.push_back(std::move(event));

  // Only process the new event if it is the only one in the queue. Otherwise,
  // there's already an async cookie-check in progress.
  if (size_before_push == 0) {
    ProcessEvents();
  }
}

void AttributionManagerImpl::ProcessEvents() {
  // Process as many events not requiring a cookie check (synchronously) as
  // possible. Once reaching the first to require a cookie check, start the
  // async check and stop processing further events.
  while (!pending_events_.empty()) {
    const attribution_reporting::SuitableOrigin* cookie_origin = absl::visit(
        base::Overloaded{
            [](const StorableSource& source) {
              return source.registration().debug_key.has_value() ||
                             source.registration().debug_reporting
                         ? &source.common_info().reporting_origin()
                         : nullptr;
            },
            [](const AttributionTrigger& trigger) {
              const attribution_reporting::TriggerRegistration& registration =
                  trigger.registration();
              return registration.debug_key.has_value() ||
                             registration.debug_reporting
                         ? &trigger.reporting_origin()
                         : nullptr;
            },
        },
        pending_events_.front());
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

  absl::visit(base::Overloaded{
                  [&](StorableSource& source) {
                    StoreSource(std::move(source), is_debug_cookie_set);
                  },
                  [&](AttributionTrigger& trigger) {
                    StoreTrigger(std::move(trigger), is_debug_cookie_set);
                  },
              },
              pending_events_.front());

  pending_events_.pop_front();
}

void AttributionManagerImpl::StoreSource(StorableSource source,
                                         bool is_debug_cookie_set) {
  absl::optional<uint64_t> cleared_debug_key;
  if (!is_debug_cookie_set) {
    cleared_debug_key =
        std::exchange(source.registration().debug_key, absl::nullopt);
  }

  attribution_storage_.AsyncCall(&AttributionStorage::StoreSource)
      .WithArgs(source)
      .Then(base::BindOnce(&AttributionManagerImpl::OnSourceStored,
                           weak_factory_.GetWeakPtr(), std::move(source),
                           cleared_debug_key, is_debug_cookie_set));
}

void AttributionManagerImpl::AddPendingAggregatableReportTiming(
    const AttributionReport& report) {
  // The maximum number of pending reports that should be considered. Past this
  // value, events will be ignored.
  constexpr size_t kMaxPendingReportsTimings = 50;
  if (pending_aggregatable_reports_.size() >= kMaxPendingReportsTimings) {
    return;
  }

  DCHECK_EQ(report.GetReportType(),
            AttributionReport::Type::kAggregatableAttribution);

  auto [it, inserted] = pending_aggregatable_reports_.try_emplace(
      report.id(), PendingReportTimings{
                       .creation_time = base::Time::Now(),
                       .report_time = report.report_time(),
                   });
  DCHECK(inserted);
}

void AttributionManagerImpl::OnReportStored(
    const AttributionTrigger& trigger,
    absl::optional<uint64_t> cleared_debug_key,
    bool is_debug_cookie_set,
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

    AddPendingAggregatableReportTiming(*report);

    MaybeSendDebugReport(std::move(*report));
  }

  min_new_report_time = AttributionReport::MinReportTime(
      min_new_report_time, result.min_null_aggregatable_report_time());

  scheduler_timer_.MaybeSet(min_new_report_time);

  bool notify_reports_changed = false;

  if (result.event_level_status() !=
          AttributionTrigger::EventLevelResult::kInternalError ||
      result.aggregatable_status() ==
          AttributionTrigger::AggregatableResult::kSuccess) {
    // Sources are changed here because storing an event-level report or
    // aggregatable report can cause sources to reach event-level attribution
    // limit or become associated with a dedup key.
    NotifySourcesChanged();

    notify_reports_changed = true;
  }

  if (notify_reports_changed ||
      result.min_null_aggregatable_report_time().has_value()) {
    NotifyReportsChanged();
  }

  for (auto& observer : observers_) {
    observer.OnTriggerHandled(trigger, cleared_debug_key, result);
  }

  MaybeSendVerboseDebugReport(trigger, is_debug_cookie_set, result);
}

void AttributionManagerImpl::MaybeSendDebugReport(AttributionReport&& report) {
  const AttributionInfo& attribution_info = report.attribution_info();
  const StoredSource* source = report.GetStoredSource();
  DCHECK(source);
  if (!attribution_info.debug_key || !source->debug_key() ||
      !IsReportAllowed(report)) {
    return;
  }

  // We don't delete from storage for debug reports.
  PrepareToSendReport(std::move(report), /*is_debug_report=*/true,
                      base::BindOnce(&AttributionManagerImpl::NotifyReportSent,
                                     weak_factory_.GetWeakPtr(),
                                     /*is_debug_report=*/true));
}

// TODO(apaseltiner): Consider `OnUserVisibleTaskStarted()` here, since this is
// used by the internals UI, which is user-visible.
void AttributionManagerImpl::GetActiveSourcesForWebUI(
    base::OnceCallback<void(std::vector<StoredSource>)> callback) {
  const int kMaxSources = 1000;
  attribution_storage_.AsyncCall(&AttributionStorage::GetActiveSources)
      .WithArgs(kMaxSources)
      .Then(std::move(callback));
}

// TODO(apaseltiner): Consider `OnUserVisibleTaskStarted()` here, since this is
// used by the internals UI, which is user-visible.
void AttributionManagerImpl::GetPendingReportsForInternalUse(
    int limit,
    base::OnceCallback<void(std::vector<AttributionReport>)> callback) {
  attribution_storage_.AsyncCall(&AttributionStorage::GetAttributionReports)
      .WithArgs(/*max_report_time=*/base::Time::Max(), limit)
      .Then(std::move(callback));
}

// TODO(apaseltiner): Consider `OnUserVisibleTaskStarted()` here, since this is
// used by the internals UI, which is user-visible.
void AttributionManagerImpl::SendReportsForWebUI(
    const std::vector<AttributionReport::Id>& ids,
    base::OnceClosure done) {
  DCHECK(done);
  attribution_storage_.AsyncCall(&AttributionStorage::GetReports)
      .WithArgs(ids)
      .Then(base::BindOnce(&AttributionManagerImpl::OnGetReportsToSendFromWebUI,
                           weak_factory_.GetWeakPtr(), std::move(done)));
}

void AttributionManagerImpl::ClearData(
    base::Time delete_begin,
    base::Time delete_end,
    StoragePartition::StorageKeyMatcherFunction filter,
    BrowsingDataFilterBuilder* filter_builder,
    bool delete_rate_limit_data,
    base::OnceClosure done) {
  auto barrier = base::BarrierClosure(2, std::move(done));
  done = barrier;

  if (filter_builder) {
    auto* filter_builder_impl =
        static_cast<BrowsingDataFilterBuilderImpl*>(filter_builder);
    os_level_manager_->ClearData(
        delete_begin, delete_end, filter_builder_impl->GetOrigins(),
        filter_builder_impl->GetRegisterableDomains(),
        filter_builder->GetMode(), delete_rate_limit_data, std::move(barrier));
  } else {
    // When there is not filter_builder, we clear all the data.
    os_level_manager_->ClearData(delete_begin, delete_end, /*origins=*/{},
                                 /*domains=*/{},
                                 // By preserving data only from an empty list,
                                 // we are effectively clearing all the data.
                                 BrowsingDataFilterBuilder::Mode::kPreserve,
                                 delete_rate_limit_data, std::move(barrier));
  }

  // TODO(apaseltiner): It's not necessarily true that this deletion is user
  // visible, as a site can initiate deletion via the Clear-Site-Data header. We
  // could inspect `delete_rate_limit_data` to determine this, as its value is
  // true only for user-initiated deletions, not site-initiated ones.
  OnUserVisibleTaskStarted();

  attribution_storage_.AsyncCall(&AttributionStorage::ClearData)
      .WithArgs(delete_begin, delete_end, std::move(filter),
                delete_rate_limit_data)
      .Then(std::move(done).Then(
          base::BindOnce(&AttributionManagerImpl::OnClearDataComplete,
                         weak_factory_.GetWeakPtr())));
}

void AttributionManagerImpl::OnUserVisibleTaskStarted() {
  // When a user-visible task is queued or running, we use a higher priority.
  ++num_pending_user_visible_tasks_;
  storage_task_runner_->UpdatePriority(base::TaskPriority::USER_VISIBLE);
}

void AttributionManagerImpl::OnUserVisibleTaskComplete() {
  DCHECK_GT(num_pending_user_visible_tasks_, 0);
  --num_pending_user_visible_tasks_;

  // No more user-visible tasks, so we can reset the priority.
  if (num_pending_user_visible_tasks_ == 0) {
    storage_task_runner_->UpdatePriority(base::TaskPriority::BEST_EFFORT);
  }
}

void AttributionManagerImpl::OnClearDataComplete() {
  OnUserVisibleTaskComplete();
  NotifySourcesChanged();
  NotifyReportsChanged();
}

void AttributionManagerImpl::GetAllDataKeys(
    base::OnceCallback<void(std::set<DataKey>)> callback) {
  OnUserVisibleTaskStarted();
  attribution_storage_.AsyncCall(&AttributionStorage::GetAllDataKeys)
      .Then(std::move(callback).Then(
          base::BindOnce(&AttributionManagerImpl::OnUserVisibleTaskComplete,
                         weak_factory_.GetWeakPtr())));
}

void AttributionManagerImpl::RemoveAttributionDataByDataKey(
    const DataKey& data_key,
    base::OnceClosure callback) {
  auto barrier = base::BarrierClosure(2, std::move(callback));
  callback = barrier;

  os_level_manager_->ClearData(
      /*delete_begin=*/base::Time::Min(), /*delete_end=*/base::Time::Max(),
      /*origins=*/{data_key.reporting_origin()},
      /*domains=*/{}, BrowsingDataFilterBuilder::Mode::kDelete,
      /*delete_rate_limit_data=*/true, std::move(barrier));

  OnUserVisibleTaskStarted();

  attribution_storage_.AsyncCall(&AttributionStorage::DeleteByDataKey)
      .WithArgs(data_key)
      .Then(std::move(callback).Then(
          base::BindOnce(&AttributionManagerImpl::OnClearDataComplete,
                         weak_factory_.GetWeakPtr())));
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
      .WithArgs(/*max_report_time=*/base::Time::Now(), /*limit=*/-1)
      .Then(base::BindOnce(&AttributionManagerImpl::SendReports,
                           weak_factory_.GetWeakPtr(),
                           /*web_ui_callback=*/base::NullCallback()));
}

void AttributionManagerImpl::OnGetReportsToSendFromWebUI(
    base::OnceClosure done,
    std::vector<AttributionReport> reports) {
  DCHECK(done);

  if (reports.empty()) {
    std::move(done).Run();
    return;
  }

  // Give all reports the same report time for consistency in the internals UI.
  const base::Time now = base::Time::Now();
  for (AttributionReport& report : reports) {
    report.set_report_time(now);
  }

  auto barrier = base::BarrierClosure(reports.size(), std::move(done));
  SendReports(std::move(barrier), std::move(reports));
}

// If `web_ui_callback` is null, assumes that `reports` are being sent at their
// intended time, and logs metrics for them. Otherwise, does not log metrics.
void AttributionManagerImpl::SendReports(
    base::RepeatingClosure web_ui_callback,
    std::vector<AttributionReport> reports) {
  const base::Time now = base::Time::Now();
  for (AttributionReport& report : reports) {
    DCHECK_LE(report.report_time(), now);

    bool inserted = reports_being_sent_.emplace(report.id()).second;
    if (!inserted) {
      if (web_ui_callback) {
        web_ui_callback.Run();
      }

      continue;
    }

    if (report.GetReportType() ==
        AttributionReport::Type::kAggregatableAttribution) {
      pending_aggregatable_reports_.erase(report.id());
    }

    if (!IsReportAllowed(report)) {
      // If measurement is disallowed, just drop the report on the floor. We
      // need to make sure we forward that the report was "sent" to ensure it is
      // deleted from storage, etc. This simulates sending the report through a
      // null channel.
      OnReportSent(web_ui_callback, std::move(report),
                   SendResult(SendResult::Status::kDropped));
      continue;
    }

    if (!web_ui_callback) {
      LogMetricsOnReportSend(report, now);
    }

    PrepareToSendReport(
        std::move(report), /*is_debug_report=*/false,
        base::BindOnce(&AttributionManagerImpl::OnReportSent,
                       weak_factory_.GetWeakPtr(), web_ui_callback));
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
    case AttributionReport::Type::kEventLevel:
      report_sender_->SendReport(std::move(report), is_debug_report,
                                 std::move(callback));
      break;
    case AttributionReport::Type::kAggregatableAttribution:
    case AttributionReport::Type::kNullAggregatable:
      AssembleAggregatableReport(std::move(report), is_debug_report,
                                 std::move(callback));
      break;
  }
}

void AttributionManagerImpl::OnReportSent(base::OnceClosure done,
                                          const AttributionReport& report,
                                          SendResult info) {
  // If there was a transient failure, and another attempt is allowed,
  // update the report's DB state to reflect that. Otherwise, delete the report
  // from storage.

  absl::optional<base::Time> new_report_time;
  // TODO(linnan): Retry on transient assembly failure isn't privacy sensitive,
  // therefore we could consider subjecting these failures to a different limit.
  if (info.status == SendResult::Status::kTransientFailure ||
      info.status == SendResult::Status::kTransientAssemblyFailure) {
    if (absl::optional<base::TimeDelta> delay =
            GetFailedReportDelay(report.failed_send_attempts() + 1)) {
      new_report_time = base::Time::Now() + *delay;
    }
  }

  if (info.status == SendResult::Status::kTransientFailure ||
      info.status == SendResult::Status::kFailure) {
    RecordNetworkConnectionTypeOnFailure(report.GetReportType(),
                                         scheduler_timer_.connection_type());
  }

  base::OnceCallback then = base::BindOnce(
      [](base::OnceClosure done, base::WeakPtr<AttributionManagerImpl> manager,
         AttributionReport::Id report_id,
         absl::optional<base::Time> new_report_time, bool success) {
        if (done) {
          std::move(done).Run();
        }

        if (manager && success) {
          manager->MarkReportCompleted(report_id);
          manager->scheduler_timer_.MaybeSet(new_report_time);
          manager->NotifyReportsChanged();
        }
      },
      std::move(done), weak_factory_.GetWeakPtr(), report.id(),
      new_report_time);

  if (new_report_time) {
    attribution_storage_
        .AsyncCall(&AttributionStorage::UpdateReportForSendFailure)
        .WithArgs(report.id(), *new_report_time)
        .Then(std::move(then));

    // TODO(apaseltiner): Consider surfacing retry attempts in internals UI.

    return;
  }

  attribution_storage_.AsyncCall(&AttributionStorage::DeleteReport)
      .WithArgs(report.id())
      .Then(std::move(then));

  LogMetricsOnReportCompleted(report, info.status);

  if (info.status == SendResult::Status::kSent) {
    LogMetricsOnReportSent(report);
  }

  NotifyReportSent(/*is_debug_report=*/false, report, info);
}

void AttributionManagerImpl::NotifyReportSent(bool is_debug_report,
                                              const AttributionReport& report,
                                              SendResult info) {
  for (auto& observer : observers_) {
    observer.OnReportSent(report, /*is_debug_report=*/is_debug_report, info);
  }
}

void AttributionManagerImpl::NotifyDebugReportSent(
    const AttributionDebugReport& report,
    const int status) {
  // Use the same time for all observers.
  const base::Time time = base::Time::Now();
  for (auto& observer : observers_) {
    observer.OnDebugReportSent(report, status, time);
  }
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
                            SendResult(SendResult::Status::kAssemblyFailure));
    return;
  }

  absl::optional<AggregatableReportRequest> request =
      CreateAggregatableReportRequest(report);
  if (!request.has_value()) {
    RecordAssembleAggregatableReportStatus(
        AssembleAggregatableReportStatus::kCreateRequestFailed);
    std::move(callback).Run(std::move(report),
                            SendResult(SendResult::Status::kAssemblyFailure));
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
    AggregatableReportRequest,
    absl::optional<AggregatableReport> assembled_report,
    AggregationService::AssemblyStatus) {
  if (!assembled_report.has_value()) {
    RecordAssembleAggregatableReportStatus(
        AssembleAggregatableReportStatus::kAssembleReportFailed);
    std::move(callback).Run(
        std::move(report),
        SendResult(SendResult::Status::kTransientAssemblyFailure));
    return;
  }

  absl::visit(
      base::Overloaded{
          [](const AttributionReport::EventLevelData&) { NOTREACHED(); },
          [&](AttributionReport::AggregatableAttributionData& data) {
            data.common_data.assembled_report = std::move(assembled_report);
          },
          [&](AttributionReport::NullAggregatableData& data) {
            data.common_data.assembled_report = std::move(assembled_report);
          },
      },
      report.data());

  RecordAssembleAggregatableReportStatus(
      AssembleAggregatableReportStatus::kSuccess);

  report_sender_->SendReport(std::move(report), is_debug_report,
                             std::move(callback));
}

void AttributionManagerImpl::NotifySourcesChanged() {
  for (auto& observer : observers_) {
    observer.OnSourcesChanged();
  }
}

void AttributionManagerImpl::NotifyReportsChanged() {
  for (auto& observer : observers_) {
    observer.OnReportsChanged();
  }
}

void AttributionManagerImpl::MaybeSendVerboseDebugReport(
    const StorableSource& source,
    bool is_debug_cookie_set,
    const StoreSourceResult& result) {
  if (!base::FeatureList::IsEnabled(kAttributionVerboseDebugReporting)) {
    return;
  }

  if (!IsOperationAllowed(*storage_partition_,
                          ContentBrowserClient::AttributionReportingOperation::
                              kSourceVerboseDebugReport,
                          /*rfh=*/nullptr,
                          &*source.common_info().source_origin(),
                          /*destination_origin=*/nullptr,
                          &*source.common_info().reporting_origin())) {
    return;
  }

  if (absl::optional<AttributionDebugReport> debug_report =
          AttributionDebugReport::Create(source, is_debug_cookie_set, result)) {
    report_sender_->SendReport(
        std::move(*debug_report),
        base::BindOnce(&AttributionManagerImpl::NotifyDebugReportSent,
                       weak_factory_.GetWeakPtr()));
  }
}

void AttributionManagerImpl::MaybeSendVerboseDebugReport(
    const AttributionTrigger& trigger,
    bool is_debug_cookie_set,
    const CreateReportResult& result) {
  if (!base::FeatureList::IsEnabled(kAttributionVerboseDebugReporting)) {
    return;
  }

  if (!IsOperationAllowed(*storage_partition_,
                          ContentBrowserClient::AttributionReportingOperation::
                              kTriggerVerboseDebugReport,
                          /*rfh=*/nullptr,
                          /*source_origin=*/nullptr,
                          &*trigger.destination_origin(),
                          &*trigger.reporting_origin())) {
    return;
  }

  if (absl::optional<AttributionDebugReport> debug_report =
          AttributionDebugReport::Create(trigger, is_debug_cookie_set,
                                         result)) {
    report_sender_->SendReport(
        std::move(*debug_report),
        base::BindOnce(&AttributionManagerImpl::NotifyDebugReportSent,
                       weak_factory_.GetWeakPtr()));
  }
}

void AttributionManagerImpl::HandleOsRegistration(
    OsRegistration registration,
    GlobalRenderFrameHostId render_frame_id) {
  if (!network::HasAttributionOsSupport(GetSupport())) {
    NotifyOsRegistration(registration,
                         /*is_debug_key_allowed=*/false,
                         OsRegistrationResult::kUnsupported);
    return;
  }

  const auto registration_origin =
      url::Origin::Create(registration.registration_url);
  if (registration_origin.opaque()) {
    NotifyOsRegistration(registration,
                         /*is_debug_key_allowed=*/false,
                         OsRegistrationResult::kInvalidRegistrationUrl);
    return;
  }

  ContentBrowserClient::AttributionReportingOperation operation;
  const url::Origin* source_origin;
  const url::Origin* destination_origin;
  switch (registration.GetType()) {
    case RegistrationType::kSource:
      operation =
          ContentBrowserClient::AttributionReportingOperation::kOsSource;
      source_origin = &registration.top_level_origin;
      destination_origin = nullptr;
      break;
    case RegistrationType::kTrigger:
      operation =
          ContentBrowserClient::AttributionReportingOperation::kOsTrigger;
      source_origin = nullptr;
      destination_origin = &registration.top_level_origin;
      break;
  }

  if (!IsOperationAllowed(*storage_partition_, operation,
                          RenderFrameHost::FromID(render_frame_id),
                          source_origin, destination_origin,
                          /*reporting_origin=*/&registration_origin)) {
    NotifyOsRegistration(registration,
                         /*is_debug_key_allowed=*/false,
                         OsRegistrationResult::kProhibitedByBrowserPolicy);
    return;
  }

  const size_t size_before_push = pending_os_events_.size();

  // Avoid unbounded memory growth with adversarial input.
  bool allowed = size_before_push < max_pending_events_;
  base::UmaHistogramBoolean("Conversions.EnqueueOsEventAllowed", allowed);
  if (!allowed) {
    NotifyOsRegistration(registration,
                         /*is_debug_key_allowed=*/false,
                         OsRegistrationResult::kExcessiveQueueSize);
    return;
  }

  pending_os_events_.push_back(std::move(registration));

  // Only process the new event if it is the only one in the queue. Otherwise,
  // there's already an async cookie-check in progress.
  if (size_before_push == 0) {
    ProcessNextOsEvent();
  }
}

void AttributionManagerImpl::ProcessNextOsEvent() {
  DCHECK(!pending_os_events_.empty());

  cookie_checker_->IsDebugCookieSet(
      url::Origin::Create(pending_os_events_.front().registration_url),
      base::BindOnce(
          [](base::WeakPtr<AttributionManagerImpl> manager,
             bool is_debug_key_allowed) {
            if (!manager) {
              return;
            }

            DCHECK(!manager->pending_os_events_.empty());

            {
              auto& event = manager->pending_os_events_.front();
              manager->os_level_manager_->Register(
                  std::move(event), is_debug_key_allowed,
                  base::BindOnce(&AttributionManagerImpl::OnOsRegistration,
                                 manager, is_debug_key_allowed));
            }

            manager->pending_os_events_.pop_front();
            if (!manager->pending_os_events_.empty()) {
              manager->ProcessNextOsEvent();
            }
          },
          weak_factory_.GetWeakPtr()));
}

void AttributionManagerImpl::NotifyOsRegistration(
    const OsRegistration& registration,
    bool is_debug_key_allowed,
    OsRegistrationResult result) {
  base::Time now = base::Time::Now();
  for (auto& observer : observers_) {
    observer.OnOsRegistration(now, registration, is_debug_key_allowed, result);
  }
  switch (registration.GetType()) {
    case attribution_reporting::mojom::RegistrationType::kSource:
      base::UmaHistogramEnumeration("Conversions.OsRegistrationResult.Source",
                                    result);
      break;
    case attribution_reporting::mojom::RegistrationType::kTrigger:
      base::UmaHistogramEnumeration("Conversions.OsRegistrationResult.Trigger",
                                    result);
      break;
  }
}

void AttributionManagerImpl::OnOsRegistration(
    bool is_debug_key_allowed,
    const OsRegistration& registration,
    bool success) {
  MaybeSendVerboseDebugReport(registration);

  NotifyOsRegistration(registration, is_debug_key_allowed,
                       success ? OsRegistrationResult::kPassedToOs
                               : OsRegistrationResult::kRejectedByOs);
}

void AttributionManagerImpl::SetDebugMode(absl::optional<bool> enabled,
                                          base::OnceClosure done) {
  bool debug_mode =
      enabled.value_or(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAttributionReportingDebugMode));

  // TODO(apaseltiner): Observers should be notified when the debug mode changes
  // so they can re-query its value.
  attribution_storage_.AsyncCall(&AttributionStorage::SetDelegate)
      .WithArgs(MakeStorageDelegate(debug_mode))
      .Then(std::move(done));
}

void AttributionManagerImpl::MaybeSendVerboseDebugReport(
    const OsRegistration& registration) {
  if (!base::FeatureList::IsEnabled(kAttributionVerboseDebugReporting)) {
    return;
  }

  const auto registration_origin =
      url::Origin::Create(registration.registration_url);

  ContentBrowserClient::AttributionReportingOperation operation;
  const url::Origin* source_origin;
  const url::Origin* destination_origin;
  switch (registration.GetType()) {
    case RegistrationType::kSource:
      operation = ContentBrowserClient::AttributionReportingOperation::
          kOsSourceVerboseDebugReport;
      source_origin = &registration.top_level_origin;
      destination_origin = nullptr;
      break;
    case RegistrationType::kTrigger:
      operation = ContentBrowserClient::AttributionReportingOperation::
          kOsTriggerVerboseDebugReport;
      source_origin = nullptr;
      destination_origin = &registration.top_level_origin;
      break;
  }

  if (!IsOperationAllowed(*storage_partition_, operation,
                          /*rfh=*/nullptr, source_origin, destination_origin,
                          /*reporting_origin=*/&registration_origin)) {
    return;
  }

  if (absl::optional<AttributionDebugReport> debug_report =
          AttributionDebugReport::Create(registration)) {
    report_sender_->SendReport(
        std::move(*debug_report),
        base::BindOnce(&AttributionManagerImpl::NotifyDebugReportSent,
                       weak_factory_.GetWeakPtr()));
  }
}

}  // namespace content
