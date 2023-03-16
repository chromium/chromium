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
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/attribution_reporting/os_support.mojom.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
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
#include "content/browser/attribution_reporting/send_result.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/browser/browsing_data/browsing_data_filter_builder_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/attribution_data_model.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/attribution_reporting/attribution_os_level_manager.h"
#include "content/browser/attribution_reporting/attribution_os_level_manager_android.h"
#endif

namespace content {

namespace {

using ScopedUseInMemoryStorageForTesting =
    ::content::AttributionManagerImpl::ScopedUseInMemoryStorageForTesting;

using ScopedOsSupportForTesting =
    ::content::AttributionManagerImpl::ScopedOsSupportForTesting;

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
      base::SequenceBound<AttributionStorage>& attribution_storage)
      : send_reports_(std::move(send_reports)),
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

  base::RepeatingClosure send_reports_;
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

void RecordStoreSourceStatus(AttributionStorage::StoreSourceResult result) {
  static_assert(StorableSource::Result::kMaxValue ==
                    StorableSource::Result::kSuccessNoised,
                "Bump version of Conversions.SourceStoredStatus2 histogram.");
  base::UmaHistogramEnumeration("Conversions.SourceStoredStatus2",
                                result.status);
}

void RecordCreateReportStatus(CreateReportResult result) {
  static_assert(AttributionTrigger::EventLevelResult::kMaxValue ==
                    AttributionTrigger::EventLevelResult::kNotRegistered,
                "Bump version of Conversions.CreateReportStatus7 histogram.");
  base::UmaHistogramEnumeration("Conversions.CreateReportStatus7",
                                result.event_level_status());
  static_assert(
      AttributionTrigger::AggregatableResult::kMaxValue ==
          AttributionTrigger::AggregatableResult::kReportWindowPassed,
      "Bump version of Conversions.AggregatableReport.CreateReportStatus3 "
      "histogram.");
  base::UmaHistogramEnumeration(
      "Conversions.AggregatableReport.CreateReportStatus3",
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
    case AttributionReport::Type::kEventLevel: {
      // Use a large time range to capture users that might not open the browser
      // for a long time while a conversion report is pending. Revisit this
      // range if it is non-ideal for real world data.
      const AttributionInfo& attribution_info = report.attribution_info();
      base::TimeDelta time_since_original_report_time =
          now - report.OriginalReportTime();
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
          now - report.OriginalReportTime(), base::Seconds(1), base::Days(24),
          50);

      UMA_HISTOGRAM_CUSTOM_TIMES(
          "Conversions.AggregatableReport.SchedulerReportDelay",
          now - report.report_time(), base::Seconds(1), base::Days(1), 50);
      break;
    }
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
  }
}

// Called when `report` is sent successfully.
void LogMetricsOnReportSent(const AttributionReport& report) {
  base::TimeDelta time_from_conversion_to_report_sent =
      base::Time::Now() - report.attribution_info().time;

  switch (report.GetReportType()) {
    case AttributionReport::Type::kEventLevel:
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
      break;
  }
}

std::unique_ptr<AttributionStorageDelegate> MakeStorageDelegate() {
  bool debug_mode = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAttributionReportingDebugMode);

  if (debug_mode) {
    return std::make_unique<AttributionStorageDelegateImpl>(
        AttributionNoiseMode::kNone, AttributionDelayMode::kNone);
  }

  return std::make_unique<AttributionStorageDelegateImpl>(
      AttributionNoiseMode::kDefault, AttributionDelayMode::kDefault);
}

bool IsOperationAllowed(
    StoragePartitionImpl* storage_partition,
    ContentBrowserClient::AttributionReportingOperation operation,
    content::RenderFrameHost* rfh,
    const url::Origin* source_origin,
    const url::Origin* destination_origin,
    const url::Origin* reporting_origin) {
  DCHECK(storage_partition);
  return GetContentClient()->browser()->IsAttributionReportingOperationAllowed(
      storage_partition->browser_context(), operation, rfh, source_origin,
      destination_origin, reporting_origin);
}

bool g_run_in_memory = false;

// This flag is per device and can only be changed by the OS.
//
// TODO(linnan): As currently we don't listen to the flag changes on the OS and
// the API is synchronous, consider changing this to be per instance instead of
// global which is set on creation. The renderer would be initialized with the
// instance value without further updates.
attribution_reporting::mojom::OsSupport g_os_support =
    attribution_reporting::mojom::OsSupport::kDisabled;

void SetOsSupport(attribution_reporting::mojom::OsSupport os_support) {
  if (g_os_support == os_support) {
    return;
  }

  g_os_support = os_support;

  for (RenderProcessHost::iterator it = RenderProcessHost::AllHostsIterator();
       !it.IsAtEnd(); it.Advance()) {
    it.GetCurrentValue()->SetOsSupportForAttributionReporting(g_os_support);
  }
}

}  // namespace

struct AttributionManagerImpl::SourceOrTriggerRFH {
  SourceOrTrigger source_or_trigger;
  GlobalRenderFrameHostId rfh_id;
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

ScopedOsSupportForTesting::ScopedOsSupportForTesting(
    attribution_reporting::mojom::OsSupport os_support)
    : previous_(g_os_support) {
  SetOsSupport(os_support);
}

ScopedOsSupportForTesting::~ScopedOsSupportForTesting() {
  SetOsSupport(previous_);
}

// static
std::unique_ptr<AttributionManagerImpl>
AttributionManagerImpl::CreateWithNewDbForTesting(
    StoragePartitionImpl* storage_partition,
    const base::FilePath& user_data_directory,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy) {
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    if (!AttributionStorageSql::DeleteStorageForTesting(user_data_directory)) {
      return nullptr;
    }
  }

  return std::make_unique<AttributionManagerImpl>(
      storage_partition, user_data_directory,
      std::move(special_storage_policy));
}

// static
attribution_reporting::mojom::OsSupport AttributionManagerImpl::GetOsSupport() {
  return g_os_support;
}

bool AttributionManagerImpl::IsReportAllowed(
    const AttributionReport& report) const {
  const CommonSourceInfo& common_info =
      report.attribution_info().source.common_info();
  return IsOperationAllowed(
      storage_partition_.get(),
      ContentBrowserClient::AttributionReportingOperation::kReport,
      /*rfh=*/nullptr, &*common_info.source_origin(),
      &*report.attribution_info().context_origin,
      &*common_info.reporting_origin());
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
    StoragePartitionImpl* storage_partition,
    scoped_refptr<base::UpdateableSequencedTaskRunner> storage_task_runner) {
  return base::WrapUnique(new AttributionManagerImpl(
      storage_partition, user_data_directory, max_pending_events,
      std::move(special_storage_policy), std::move(storage_delegate),
      std::move(cookie_checker), std::move(report_sender),
      /*data_host_manager=*/nullptr, std::move(storage_task_runner)));
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
          std::make_unique<AttributionReportNetworkSender>(
              storage_partition->GetURLLoaderFactoryForBrowserProcess()),
          std::make_unique<AttributionDataHostManagerImpl>(this),
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
    std::unique_ptr<AttributionDataHostManager> data_host_manager,
    scoped_refptr<base::UpdateableSequencedTaskRunner> storage_task_runner)
    : storage_partition_(storage_partition),
      max_pending_events_(max_pending_events),
      storage_task_runner_(std::move(storage_task_runner)),
      attribution_storage_(base::SequenceBound<AttributionStorageSql>(
          storage_task_runner_,
          g_run_in_memory ? base::FilePath() : user_data_directory,
          std::move(storage_delegate))),
      scheduler_timer_(std::make_unique<AttributionReportScheduler>(
          base::BindRepeating(&AttributionManagerImpl::GetReportsToSend,
                              base::Unretained(this)),
          attribution_storage_)),
      data_host_manager_(std::move(data_host_manager)),
      special_storage_policy_(std::move(special_storage_policy)),
      cookie_checker_(std::move(cookie_checker)),
      report_sender_(std::move(report_sender)) {
  DCHECK(storage_partition_);
  DCHECK_GT(max_pending_events_, 0u);
  DCHECK(storage_task_runner_);
  DCHECK(cookie_checker_);
  DCHECK(report_sender_);

#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          blink::features::kAttributionReportingCrossAppWeb)) {
    attribution_os_level_manager_ =
        std::make_unique<AttributionOsLevelManagerAndroid>();
    // The measurement API status can only change when user changes the setting
    // on the device, therefore it's fine to update the global variable to keep
    // track of the latest setting.
    SetOsSupport(attribution_os_level_manager_->GetOsSupport());
  }
#endif
}

AttributionManagerImpl::~AttributionManagerImpl() {
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
  return data_host_manager_.get();
}

void AttributionManagerImpl::HandleSource(
    StorableSource source,
    GlobalRenderFrameHostId render_frame_id) {
  MaybeEnqueueEvent(SourceOrTriggerRFH{.source_or_trigger = std::move(source),
                                       .rfh_id = render_frame_id});
}

void AttributionManagerImpl::StoreSource(
    StorableSource source,
    absl::optional<uint64_t> cleared_debug_key,
    bool is_debug_cookie_set) {
  attribution_storage_.AsyncCall(&AttributionStorage::StoreSource)
      .WithArgs(source)
      .Then(base::BindOnce(&AttributionManagerImpl::OnSourceStored,
                           weak_factory_.GetWeakPtr(), std::move(source),
                           cleared_debug_key, is_debug_cookie_set));
}

void AttributionManagerImpl::OnSourceStored(
    const StorableSource& source,
    absl::optional<uint64_t> cleared_debug_key,
    bool is_debug_cookie_set,
    AttributionStorage::StoreSourceResult result) {
  RecordStoreSourceStatus(result);

  for (auto& observer : observers_) {
    observer.OnSourceHandled(source, cleared_debug_key, result.status);
  }

  scheduler_timer_.MaybeSet(result.min_fake_report_time);

  NotifySourcesChanged();

  MaybeSendVerboseDebugReport(source, is_debug_cookie_set, result);
}

void AttributionManagerImpl::HandleTrigger(
    AttributionTrigger trigger,
    GlobalRenderFrameHostId render_frame_id) {
  MaybeEnqueueEvent(SourceOrTriggerRFH{.source_or_trigger = std::move(trigger),
                                       .rfh_id = render_frame_id});
}

void AttributionManagerImpl::StoreTrigger(
    AttributionTrigger trigger,
    absl::optional<uint64_t> cleared_debug_key,
    bool is_debug_cookie_set) {
  attribution_storage_.AsyncCall(&AttributionStorage::MaybeCreateAndStoreReport)
      .WithArgs(trigger)
      .Then(base::BindOnce(&AttributionManagerImpl::OnReportStored,
                           weak_factory_.GetWeakPtr(), std::move(trigger),
                           cleared_debug_key, is_debug_cookie_set));
}

void AttributionManagerImpl::MaybeEnqueueEvent(SourceOrTriggerRFH event) {
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
              return source.common_info().debug_key().has_value() ||
                             source.debug_reporting()
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
        pending_events_.front().source_or_trigger);
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

  SourceOrTriggerRFH event = std::move(pending_events_.front());
  pending_events_.pop_front();

  absl::visit(
      base::Overloaded{
          [&](StorableSource source) {
            CommonSourceInfo& common_info = source.common_info();

            bool allowed = IsOperationAllowed(
                this->storage_partition_.get(),
                ContentBrowserClient::AttributionReportingOperation::kSource,
                RenderFrameHost::FromID(event.rfh_id),
                &*common_info.source_origin(),
                /*destination_origin=*/nullptr,
                &*common_info.reporting_origin());
            RecordRegisterImpressionAllowed(allowed);
            if (!allowed) {
              this->OnSourceStored(
                  source,
                  /*cleared_debug_key=*/absl::nullopt, is_debug_cookie_set,
                  AttributionStorage::StoreSourceResult(
                      StorableSource::Result::kProhibitedByBrowserPolicy));
              return;
            }

            absl::optional<uint64_t> cleared_debug_key;
            if (!is_debug_cookie_set && common_info.debug_key().has_value()) {
              cleared_debug_key = common_info.debug_key();
              common_info.ClearDebugKey();
            }

            this->StoreSource(std::move(source), cleared_debug_key,
                              is_debug_cookie_set);
          },

          [&](AttributionTrigger trigger) {
            attribution_reporting::TriggerRegistration& registration =
                trigger.registration();
            bool allowed = IsOperationAllowed(
                this->storage_partition_.get(),
                ContentBrowserClient::AttributionReportingOperation::kTrigger,
                RenderFrameHost::FromID(event.rfh_id),
                /*source_origin=*/nullptr, &*trigger.destination_origin(),
                &*trigger.reporting_origin());
            RecordRegisterConversionAllowed(allowed);
            if (!allowed) {
              this->OnReportStored(
                  std::move(trigger),
                  /*cleared_debug_key=*/absl::nullopt, is_debug_cookie_set,
                  CreateReportResult(/*trigger_time=*/base::Time::Now(),
                                     AttributionTrigger::EventLevelResult::
                                         kProhibitedByBrowserPolicy,
                                     AttributionTrigger::AggregatableResult::
                                         kProhibitedByBrowserPolicy));
              return;
            }

            absl::optional<uint64_t> cleared_debug_key;
            if (!is_debug_cookie_set && registration.debug_key.has_value()) {
              cleared_debug_key = registration.debug_key;
              registration.debug_key.reset();
            }

            this->StoreTrigger(std::move(trigger), cleared_debug_key,
                               is_debug_cookie_set);
          },
      },
      std::move(event.source_or_trigger));
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
    MaybeSendDebugReport(std::move(*report));
  }

  scheduler_timer_.MaybeSet(min_new_report_time);

  if (result.event_level_status() !=
      AttributionTrigger::EventLevelResult::kInternalError) {
    // Sources are changed here because storing an event-level report can
    // cause sources to reach event-level attribution limit or become
    // associated with a dedup key.
    NotifySourcesChanged();
    NotifyReportsChanged(AttributionReport::Type::kEventLevel);
  }

  if (result.aggregatable_status() ==
      AttributionTrigger::AggregatableResult::kSuccess) {
    NotifyReportsChanged(AttributionReport::Type::kAggregatableAttribution);
  }

  for (auto& observer : observers_) {
    observer.OnTriggerHandled(trigger, cleared_debug_key, result);
  }

  MaybeSendVerboseDebugReport(trigger, is_debug_cookie_set, result);
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
    AttributionReport::Types report_types,
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
#if BUILDFLAG(IS_ANDROID)
  const bool should_clear_from_os = attribution_os_level_manager_ != nullptr;
#else
  const bool should_clear_from_os = false;
#endif

  auto on_done =
      base::BarrierClosure(should_clear_from_os ? 2 : 1, std::move(done));

  // When a clear data task is queued or running, we use a higher priority.
  ++num_pending_clear_data_tasks_;
  storage_task_runner_->UpdatePriority(base::TaskPriority::USER_VISIBLE);

  attribution_storage_.AsyncCall(&AttributionStorage::ClearData)
      .WithArgs(delete_begin, delete_end, std::move(filter),
                delete_rate_limit_data)
      .Then(base::OnceClosure(on_done).Then(
          base::BindOnce(&AttributionManagerImpl::OnClearDataComplete,
                         weak_factory_.GetWeakPtr())));

#if BUILDFLAG(IS_ANDROID)
  if (!should_clear_from_os) {
    return;
  }

  if (filter_builder) {
    auto* filter_builder_impl =
        static_cast<BrowsingDataFilterBuilderImpl*>(filter_builder);
    attribution_os_level_manager_->ClearData(
        delete_begin, delete_end, filter_builder_impl->GetOrigins(),
        filter_builder_impl->GetRegisterableDomains(),
        filter_builder->GetMode(), delete_rate_limit_data, std::move(on_done));
  } else {
    // When there is not filter_builder, we clear all the data.
    attribution_os_level_manager_->ClearData(
        delete_begin, delete_end, /*origins=*/{}, /*domains=*/{},
        // By preserving data only from an empty list, we are effectively
        // clearing all the data.
        BrowsingDataFilterBuilder::Mode::kPreserve, delete_rate_limit_data,
        std::move(on_done));
  }
#endif
}

void AttributionManagerImpl::OnClearDataComplete() {
  DCHECK_GT(num_pending_clear_data_tasks_, 0);
  --num_pending_clear_data_tasks_;

  // No more clear data tasks, so we can reset the priority.
  if (num_pending_clear_data_tasks_ == 0) {
    storage_task_runner_->UpdatePriority(base::TaskPriority::BEST_EFFORT);
  }

  NotifySourcesChanged();
  NotifyReportsChanged(AttributionReport::Type::kEventLevel);
  NotifyReportsChanged(AttributionReport::Type::kAggregatableAttribution);
}

void AttributionManagerImpl::GetAllDataKeys(
    base::OnceCallback<void(std::vector<AttributionManager::DataKey>)>
        callback) {
  attribution_storage_.AsyncCall(&AttributionStorage::GetAllDataKeys)
      .Then(std::move(callback));
}

void AttributionManagerImpl::RemoveAttributionDataByDataKey(
    const AttributionManager::DataKey& data_key,
    base::OnceClosure callback) {
  attribution_storage_.AsyncCall(&AttributionStorage::DeleteByDataKey)
      .WithArgs(data_key)
      .Then(std::move(callback));
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
                AttributionReport::Types{
                    AttributionReport::Type::kEventLevel,
                    AttributionReport::Type::kAggregatableAttribution})
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

    bool inserted = reports_being_sent_.emplace(report.ReportId()).second;
    if (!inserted) {
      if (web_ui_callback) {
        web_ui_callback.Run();
      }

      continue;
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
  if (info.status == SendResult::Status::kTransientFailure) {
    if (absl::optional<base::TimeDelta> delay =
            GetFailedReportDelay(report.failed_send_attempts() + 1)) {
      new_report_time = base::Time::Now() + *delay;
    }
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
          manager->NotifyReportsChanged(
              AttributionReport::GetReportType(report_id));
        }
      },
      std::move(done), weak_factory_.GetWeakPtr(), report.ReportId(),
      new_report_time);

  if (new_report_time) {
    attribution_storage_
        .AsyncCall(&AttributionStorage::UpdateReportForSendFailure)
        .WithArgs(report.ReportId(), *new_report_time)
        .Then(std::move(then));

    // TODO(apaseltiner): Consider surfacing retry attempts in internals UI.

    return;
  }

  attribution_storage_.AsyncCall(&AttributionStorage::DeleteReport)
      .WithArgs(report.ReportId())
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
    AggregatableReportRequest,
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
  for (auto& observer : observers_) {
    observer.OnSourcesChanged();
  }
}

void AttributionManagerImpl::NotifyReportsChanged(
    AttributionReport::Type report_type) {
  for (auto& observer : observers_) {
    observer.OnReportsChanged(report_type);
  }
}

void AttributionManagerImpl::NotifyFailedSourceRegistration(
    const std::string& header_value,
    const attribution_reporting::SuitableOrigin& source_origin,
    const attribution_reporting::SuitableOrigin& reporting_origin,
    attribution_reporting::mojom::SourceType source_type,
    attribution_reporting::mojom::SourceRegistrationError error) {
  base::Time source_time = base::Time::Now();
  for (auto& observer : observers_) {
    observer.OnFailedSourceRegistration(header_value, source_time,
                                        source_origin, reporting_origin,
                                        source_type, error);
  }
}

void AttributionManagerImpl::MaybeSendVerboseDebugReport(
    const StorableSource& source,
    bool is_debug_cookie_set,
    const AttributionStorage::StoreSourceResult& result) {
  if (!base::FeatureList::IsEnabled(kAttributionVerboseDebugReporting)) {
    return;
  }

  if (!IsOperationAllowed(storage_partition_.get(),
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

  if (!IsOperationAllowed(storage_partition_.get(),
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

#if BUILDFLAG(IS_ANDROID)
void AttributionManagerImpl::OverrideOsLevelManagerForTesting(
    std::unique_ptr<AttributionOsLevelManager> os_level_manager) {
  attribution_os_level_manager_ = std::move(os_level_manager);
}
#endif

}  // namespace content
