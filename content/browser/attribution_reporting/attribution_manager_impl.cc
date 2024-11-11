// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_manager_impl.h"

#include <stddef.h>

#include <cmath>
#include <functional>
#include <optional>
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
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task/updateable_sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/attribution_reporting/aggregatable_filtering_id_max_bytes.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/os_registration.h"
#include "components/attribution_reporting/registration.mojom.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "content/browser/aggregation_service/aggregation_service.h"
#include "content/browser/aggregation_service/aggregation_service_impl.h"
#include "content/browser/aggregation_service/report_scheduler_timer.h"
#include "content/browser/attribution_reporting/aggregatable_attribution_utils.h"
#include "content/browser/attribution_reporting/aggregatable_debug_report.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager.h"
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
#include "content/browser/attribution_reporting/attribution_resolver.h"
#include "content/browser/attribution_reporting/attribution_resolver_delegate.h"
#include "content/browser/attribution_reporting/attribution_resolver_delegate_impl.h"
#include "content/browser/attribution_reporting/attribution_resolver_impl.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/create_report_result.h"
#include "content/browser/attribution_reporting/os_registration.h"
#include "content/browser/attribution_reporting/send_result.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/store_source_result.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/attribution_data_model.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_change_manager.mojom-forward.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
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

using ::attribution_reporting::OsRegistrationItem;
using ::attribution_reporting::mojom::OsRegistrationResult;
using ::attribution_reporting::mojom::RegistrationType;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ConversionReportSendOutcome)
enum class ConversionReportSendOutcome {
  kSent = 0,
  kFailed = 1,
  kDropped = 2,
  kFailedToAssemble = 3,
  kMaxValue = kFailedToAssemble,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/attribution_reporting/enums.xml:ConversionReportSendOutcome)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ConversionReportSendRetryCount)
enum class ConversionReportSendRetryCount {
  kNone = 0,
  kOnce = 1,
  kTwice = 2,
  kFailed = 3,
  kMaxValue = kFailed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/attribution_reporting/enums.xml:ConversionReportSendRetryCount)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ConversionReportSendRetryCountThirdRetryEnabled)
enum class ConversionReportSendRetryCountThirdRetryEnabled {
  kNone = 0,
  kOnce = 1,
  kTwice = 2,
  kThrice = 3,
  kFailed = 4,
  kMaxValue = kFailed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/attribution_reporting/enums.xml:ConversionReportSendRetryCountThirdRetryEnabled)

}  // namespace

// This class consolidates logic regarding when to schedule the browser to send
// attribution reports. It talks directly to the `AttributionResolver` to help
// make these decisions.
//
// While the class does not make large changes to the underlying database, it
// is responsible for notifying the `AttributionResolver` when the browser comes
// back online, which mutates report times for some scheduled reports.
class AttributionManagerImpl::ReportScheduler
    : public ReportSchedulerTimer::Delegate {
 public:
  explicit ReportScheduler(base::WeakPtr<AttributionManagerImpl> manager)
      : manager_(manager) {}

  ~ReportScheduler() override = default;

  ReportScheduler(const ReportScheduler&) = delete;
  ReportScheduler& operator=(const ReportScheduler&) = delete;
  ReportScheduler(ReportScheduler&&) = delete;
  ReportScheduler& operator=(ReportScheduler&&) = delete;

 private:
  // ReportSchedulerTimer::Delegate:
  void GetNextReportTime(
      base::OnceCallback<void(std::optional<base::Time>)> callback,
      base::Time now) override {
    if (!manager_) {
      std::move(callback).Run(std::nullopt);
      return;
    }

    manager_->attribution_resolver_
        .AsyncCall(&AttributionResolver::GetNextReportTime)
        .WithArgs(now)
        .Then(std::move(callback));
  }

  void OnReportingTimeReached(base::Time now,
                              base::Time timer_desired_run_time) override {
    if (manager_) {
      manager_->GetReportsToSend();
    }
  }

  void AdjustOfflineReportTimes(
      base::OnceCallback<void(std::optional<base::Time>)> maybe_set_timer_cb)
      override {
    if (!manager_) {
      std::move(maybe_set_timer_cb).Run(std::nullopt);
      return;
    }

    // Add delay to all reports that should have been sent while the browser was
    // offline so they are not temporally joinable. We do this in storage to
    // avoid pulling an unbounded number of reports into memory, only to
    // immediately issue async storage calls to modify their report times.
    manager_->attribution_resolver_
        .AsyncCall(&AttributionResolver::AdjustOfflineReportTimes)
        .Then(std::move(maybe_set_timer_cb));
  }

  base::WeakPtr<AttributionManagerImpl> manager_;
};

namespace {

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

void RecordStoreSourceStatus(const StoreSourceResult& result) {
  base::UmaHistogramEnumeration("Conversions.SourceStoredStatus8",
                                result.status());
}

void RecordCreateReportStatus(const CreateReportResult& result) {
  base::UmaHistogramEnumeration("Conversions.CreateReportStatus9",
                                result.event_level_status());
  base::UmaHistogramEnumeration(
      "Conversions.AggregatableReport.CreateReportStatus4",
      result.aggregatable_status());
}

void RecordReportRetriesEventLevel(int retry_attempts,
                                   bool third_retry_enabled) {
  if (third_retry_enabled) {
    // `retry_attempts` <= 3, represents the number of retries before success.
    // `retry_attempts` == 4, represents failure after three retries.
    DCHECK_LE(retry_attempts, 4);
    base::UmaHistogramEnumeration(
        "Conversions.EventLevelReport.ReportRetriesTillSuccessOrFailure_"
        "ThirdRetryEnabled",
        static_cast<ConversionReportSendRetryCountThirdRetryEnabled>(
            retry_attempts));
  } else {
    // `retry_attempts` <= 2, represents the number of retries before success.
    // `retry_attempts` == 3, represents failure after two retries.
    DCHECK_LE(retry_attempts, 3);
    base::UmaHistogramEnumeration(
        "Conversions.EventLevelReport.ReportRetriesTillSuccessOrFailure",
        static_cast<ConversionReportSendRetryCount>(retry_attempts));
  }
}

void RecordReportRetriesAggregatable(int retry_attempts,
                                     bool third_retry_enabled) {
  if (third_retry_enabled) {
    // `retry_attempts` <= 3, represents the number of retries before success.
    // `retry_attempts` == 4, represents failure after three retries.
    DCHECK_LE(retry_attempts, 4);
    base::UmaHistogramEnumeration(
        "Conversions.AggregatableReport.ReportRetriesTillSuccessOrFailure_"
        "ThirdRetryEnabled",
        static_cast<ConversionReportSendRetryCountThirdRetryEnabled>(
            retry_attempts));
  } else {
    // `retry_attempts` <= 2, represents the number of retries before success.
    // `retry_attempts` == 3, represents failure after two retries.
    DCHECK_LE(retry_attempts, 3);
    base::UmaHistogramEnumeration(
        "Conversions.AggregatableReport.ReportRetriesTillSuccessOrFailure",
        static_cast<ConversionReportSendRetryCount>(retry_attempts));
  }
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

void LogAggregatableReportHistogramCustomTimes(const char* suffix,
                                               bool has_trigger_context_id,
                                               base::TimeDelta sample,
                                               base::TimeDelta min,
                                               base::TimeDelta max,
                                               size_t buckets) {
  base::UmaHistogramCustomTimes(
      base::StrCat({"Conversions.AggregatableReport.", suffix}), sample, min,
      max, buckets);
  if (has_trigger_context_id) {
    base::UmaHistogramCustomTimes(
        base::StrCat({"Conversions.AggregatableReport.ContextID.", suffix}),
        sample, min, max, buckets);
  } else {
    base::UmaHistogramCustomTimes(
        base::StrCat({"Conversions.AggregatableReport.NoContextID.", suffix}),
        sample, min, max, buckets);
  }
}

// Called when |report| is to be sent over network for event-level reports or
// to be assembled for aggregatable reports, for logging metrics.
void LogMetricsOnReportSend(const AttributionReport& report, base::Time now) {
  absl::visit(
      base::Overloaded{
          [&](const AttributionReport::EventLevelData&) {
            // Use a large time range to capture users that might not open the
            // browser for a long time while a conversion report is pending.
            // Revisit this range if it is non-ideal for real world data.
            const AttributionInfo& attribution_info = report.attribution_info();
            base::TimeDelta time_since_original_report_time =
                now - report.initial_report_time();
            base::UmaHistogramCustomTimes("Conversions.ExtraReportDelay2",
                                          time_since_original_report_time,
                                          base::Seconds(1), base::Days(24),
                                          /*buckets=*/100);

            base::TimeDelta time_from_conversion_to_report_send =
                report.report_time() - attribution_info.time;
            UMA_HISTOGRAM_COUNTS_1000(
                "Conversions.TimeFromConversionToReportSend",
                time_from_conversion_to_report_send.InHours());

            UMA_HISTOGRAM_CUSTOM_TIMES("Conversions.SchedulerReportDelay",
                                       now - report.report_time(),
                                       base::Seconds(1), base::Days(1), 50);
          },
          [&](const AttributionReport::AggregatableData& data) {
            if (data.is_null()) {
              return;
            }

            base::TimeDelta time_from_conversion_to_report_assembly =
                report.report_time() - report.attribution_info().time;
            UMA_HISTOGRAM_CUSTOM_TIMES(
                "Conversions.AggregatableReport."
                "TimeFromTriggerToReportAssembly2",
                time_from_conversion_to_report_assembly, base::Minutes(1),
                base::Days(24), 50);

            LogAggregatableReportHistogramCustomTimes(
                "ExtraReportDelay",
                data.aggregatable_trigger_config()
                    .trigger_context_id()
                    .has_value(),
                now - report.initial_report_time(), base::Seconds(1),
                base::Days(24), 50);

            UMA_HISTOGRAM_CUSTOM_TIMES(
                "Conversions.AggregatableReport.SchedulerReportDelay",
                now - report.report_time(), base::Seconds(1), base::Days(1),
                50);
          },
      },
      report.data());
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
void LogMetricsOnReportSent(const AttributionReport& report,
                            bool third_retry_enabled) {
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
      RecordReportRetriesEventLevel(report.failed_send_attempts(),
                                    third_retry_enabled);
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
      RecordReportRetriesAggregatable(report.failed_send_attempts(),
                                      third_retry_enabled);
      break;
    case AttributionReport::Type::kNullAggregatable:
      break;
  }
}

bool HasNonDefaultFilteringId(const AttributionTrigger& trigger) {
  return base::ranges::any_of(
      trigger.registration().aggregatable_values, [](const auto& value) {
        return base::ranges::any_of(value.values(), [](const auto& val) {
          return val.second.filtering_id() !=
                 attribution_reporting::kDefaultFilteringId;
        });
      });
}

void RecordAggregatableFilteringIdUsage(const AttributionTrigger& trigger) {
  base::UmaHistogramBoolean("Conversions.NonDefaultAggregatableFilteringId",
                            HasNonDefaultFilteringId(trigger));

  base::UmaHistogramExactLinear(
      "Conversions.AggregatableFilteringIdMaxBytesValue",
      trigger.registration()
          .aggregatable_trigger_config.aggregatable_filtering_id_max_bytes()
          .value(),
      /*exclusive_max=8+1=*/9);
}

std::unique_ptr<AttributionResolverDelegate> MakeResolverDelegate(
    bool debug_mode) {
  if (debug_mode) {
    return std::make_unique<AttributionResolverDelegateImpl>(
        AttributionNoiseMode::kNone, AttributionDelayMode::kNone);
  }

  return std::make_unique<AttributionResolverDelegateImpl>(
      AttributionNoiseMode::kDefault, AttributionDelayMode::kDefault);
}

bool IsOperationAllowed(
    StoragePartitionImpl& storage_partition,
    ContentBrowserClient::AttributionReportingOperation operation,
    content::RenderFrameHost* rfh,
    const url::Origin* source_origin,
    const url::Origin* destination_origin,
    const url::Origin* reporting_origin,
    bool* can_bypass = nullptr) {
  return GetContentClient()->browser()->IsAttributionReportingOperationAllowed(
      storage_partition.browser_context(), operation, rfh, source_origin,
      destination_origin, reporting_origin, can_bypass);
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

// Returns new report time if any.
std::optional<base::Time> HandleTransientFailureOnSendReport(
    const AttributionReport& report,
    bool third_retry_enabled) {
  int retry_attempts = report.failed_send_attempts() + 1;
  if (std::optional<base::TimeDelta> delay =
          GetFailedReportDelay(retry_attempts, third_retry_enabled)) {
    return base::Time::Now() + *delay;
  } else {
    switch (report.GetReportType()) {
      case AttributionReport::Type::kEventLevel:
        RecordReportRetriesEventLevel(retry_attempts, third_retry_enabled);
        break;
      case AttributionReport::Type::kAggregatableAttribution:
        RecordReportRetriesAggregatable(retry_attempts, third_retry_enabled);
        break;
      case AttributionReport::Type::kNullAggregatable:
        break;
    }
    return std::nullopt;
  }
}

bool g_run_in_memory = false;

}  // namespace

std::optional<base::TimeDelta> GetFailedReportDelay(int failed_send_attempts,
                                                    bool third_retry_enabled) {
  DCHECK_GT(failed_send_attempts, 0);

  switch (failed_send_attempts) {
    case 1:
      return base::Minutes(5);
    case 2:
      return base::Minutes(15);
    case 3:
      return third_retry_enabled ? std::make_optional(base::Days(1))
                                 : std::nullopt;
    default:
      return std::nullopt;
  }
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
  return IsOperationAllowed(
      *storage_partition_,
      ContentBrowserClient::AttributionReportingOperation::kReport,
      /*rfh=*/nullptr, &*report.GetSourceOrigin(),
      &*report.attribution_info().context_origin, &*report.reporting_origin());
}

// static
std::unique_ptr<AttributionManagerImpl>
AttributionManagerImpl::CreateForTesting(
    const base::FilePath& user_data_directory,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
    std::unique_ptr<AttributionResolverDelegate> resolver_delegate,
    std::unique_ptr<AttributionReportSender> report_sender,
    std::unique_ptr<AttributionOsLevelManager> os_level_manager,
    StoragePartitionImpl* storage_partition,
    scoped_refptr<base::UpdateableSequencedTaskRunner> resolver_task_runner) {
  return base::WrapUnique(new AttributionManagerImpl(
      storage_partition, user_data_directory, std::move(special_storage_policy),
      std::move(resolver_delegate), std::move(report_sender),
      std::move(os_level_manager), std::move(resolver_task_runner),
      /*debug_mode=*/false));
}

AttributionManagerImpl::AttributionManagerImpl(
    StoragePartitionImpl* storage_partition,
    const base::FilePath& user_data_directory,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy)
    : AttributionManagerImpl(
          storage_partition,
          user_data_directory,
          std::move(special_storage_policy),
          /*resolver_delegate=*/nullptr,
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
                               base::ThreadPolicy::MUST_USE_FOREGROUND)),
          /*debug_mode=*/
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kAttributionReportingDebugMode)) {}

AttributionManagerImpl::AttributionManagerImpl(
    StoragePartitionImpl* storage_partition,
    const base::FilePath& user_data_directory,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
    std::unique_ptr<AttributionResolverDelegate> resolver_delegate,
    std::unique_ptr<AttributionReportSender> report_sender,
    std::unique_ptr<AttributionOsLevelManager> os_level_manager,
    scoped_refptr<base::UpdateableSequencedTaskRunner> resolver_task_runner,
    bool debug_mode)
    : storage_partition_(
          raw_ref<StoragePartitionImpl>::from_ptr(storage_partition)),
      resolver_task_runner_(std::move(resolver_task_runner)),
      attribution_resolver_(base::SequenceBound<AttributionResolverImpl>(
          resolver_task_runner_,
          g_run_in_memory ? base::FilePath() : user_data_directory,
          resolver_delegate ? std::move(resolver_delegate)
                            : MakeResolverDelegate(debug_mode))),
      data_host_manager_(
          std::make_unique<AttributionDataHostManagerImpl>(this)),
      special_storage_policy_(std::move(special_storage_policy)),
      report_sender_(std::move(report_sender)),
      os_level_manager_(std::move(os_level_manager)),
      debug_mode_(debug_mode),
      third_retry_enabled_(base::FeatureList::IsEnabled(
          kAttributionReportDeliveryThirdRetryAttempt)) {
  DCHECK(resolver_task_runner_);
  DCHECK(report_sender_);
  DCHECK(os_level_manager_);

  scheduler_timer_ = std::make_unique<ReportSchedulerTimer>(
      std::make_unique<ReportScheduler>(weak_factory_.GetWeakPtr()));
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
  observer->OnDebugModeChanged(debug_mode_);
}

void AttributionManagerImpl::RemoveObserver(AttributionObserver* observer) {
  observers_.RemoveObserver(observer);
}

AttributionDataHostManager* AttributionManagerImpl::GetDataHostManager() {
  DCHECK(data_host_manager_);
  return data_host_manager_.get();
}

namespace {

enum class BrowserPolicy {
  kProhibited,
  kAllowedWithDebug,
  kAllowedWithoutDebug,
};

BrowserPolicy GetBrowserPolicy(
    StoragePartitionImpl& storage_partition,
    ContentBrowserClient::AttributionReportingOperation registration_operation,
    ContentBrowserClient::AttributionReportingOperation debug_operation,
    GlobalRenderFrameHostId rfh_id,
    const url::Origin* source_origin,
    const url::Origin* destination_origin,
    const url::Origin& reporting_origin,
    bool check_debug) {
  if (!IsOperationAllowed(storage_partition, registration_operation,
                          RenderFrameHost::FromID(rfh_id), source_origin,
                          destination_origin, &reporting_origin)) {
    return BrowserPolicy::kProhibited;
  }

  if (check_debug) {
    // TODO(crbug.com/40941634): Clean up `can_bypass` after the cookie
    // deprecation experiment.
    bool can_bypass = false;
    if (IsOperationAllowed(storage_partition, debug_operation,
                           /*rfh=*/nullptr, source_origin, destination_origin,
                           &reporting_origin, &can_bypass) ||
        can_bypass) {
      return BrowserPolicy::kAllowedWithDebug;
    }
  }

  return BrowserPolicy::kAllowedWithoutDebug;
}

}  // namespace

void AttributionManagerImpl::HandleSource(
    StorableSource source,
    GlobalRenderFrameHostId render_frame_id) {
  BrowserPolicy browser_policy = GetBrowserPolicy(
      *storage_partition_,
      ContentBrowserClient::AttributionReportingOperation::kSource,
      ContentBrowserClient::AttributionReportingOperation::
          kSourceTransitionalDebugReporting,
      render_frame_id, &*source.common_info().source_origin(),
      /*destination_origin=*/nullptr, source.common_info().reporting_origin(),
      /*check_debug=*/true);

  std::optional<uint64_t> cleared_debug_key;

  switch (browser_policy) {
    case BrowserPolicy::kProhibited:
      OnSourceStored(
          /*cleared_debug_key=*/std::nullopt,
          StoreSourceResult(std::move(source),
                            /*is_noised=*/false,
                            /*source_time=*/base::Time::Now(),
                            /*destination_limit=*/std::nullopt,
                            StoreSourceResult::ProhibitedByBrowserPolicy()));
      return;
    case BrowserPolicy::kAllowedWithDebug:
      source.set_cookie_based_debug_allowed(/*value=*/true);
      break;
    case BrowserPolicy::kAllowedWithoutDebug:
      source.set_cookie_based_debug_allowed(/*value=*/false);
      cleared_debug_key =
          std::exchange(source.registration().debug_key, std::nullopt);
      break;
  }

  attribution_resolver_.AsyncCall(&AttributionResolver::StoreSource)
      .WithArgs(std::move(source))
      .Then(base::BindOnce(&AttributionManagerImpl::OnSourceStored,
                           weak_factory_.GetWeakPtr(), cleared_debug_key));
}

void AttributionManagerImpl::OnSourceStored(
    std::optional<uint64_t> cleared_debug_key,
    StoreSourceResult result) {
  RecordStoreSourceStatus(result);

  base::Time now = base::Time::Now();
  for (auto& observer : observers_) {
    observer.OnSourceHandled(result.source(), now, cleared_debug_key,
                             result.status());
  }

  if (const auto* success =
          absl::get_if<StoreSourceResult::Success>(&result.result())) {
    scheduler_timer_->MaybeSet(success->min_fake_report_time);
    if (success->min_fake_report_time.has_value()) {
      NotifyReportsChanged();
    }
  }

  NotifySourcesChanged();

  MaybeSendVerboseDebugReport(result);

  MaybeSendAggregatableDebugReport(result);
}

void AttributionManagerImpl::HandleTrigger(
    AttributionTrigger trigger,
    GlobalRenderFrameHostId render_frame_id) {
  RecordAggregatableFilteringIdUsage(trigger);

  const attribution_reporting::TriggerRegistration& registration =
      trigger.registration();

  BrowserPolicy browser_policy = GetBrowserPolicy(
      *storage_partition_,
      ContentBrowserClient::AttributionReportingOperation::kTrigger,
      ContentBrowserClient::AttributionReportingOperation::
          kTriggerTransitionalDebugReporting,
      render_frame_id,
      /*source_origin=*/nullptr, &*trigger.destination_origin(),
      trigger.reporting_origin(),
      /*check_debug=*/registration.debug_key.has_value() ||
          registration.debug_reporting);

  std::optional<uint64_t> cleared_debug_key;
  bool cookie_based_debug_allowed = false;

  switch (browser_policy) {
    case BrowserPolicy::kProhibited:
      OnReportStored(
          /*cleared_debug_key=*/std::nullopt,
          /*cookie_based_debug_allowed=*/false,
          CreateReportResult(
              /*trigger_time=*/base::Time::Now(), std::move(trigger),
              /*event_level_result=*/
              CreateReportResult::ProhibitedByBrowserPolicy(),
              /*aggregatable_result=*/
              CreateReportResult::ProhibitedByBrowserPolicy(),
              /*source=*/std::nullopt,
              /*min_null_aggregatable_report_time=*/std::nullopt));
      return;
    case BrowserPolicy::kAllowedWithDebug:
      cookie_based_debug_allowed = true;
      break;
    case BrowserPolicy::kAllowedWithoutDebug:
      cleared_debug_key =
          std::exchange(trigger.registration().debug_key, std::nullopt);
      break;
  }

  attribution_resolver_
      .AsyncCall(&AttributionResolver::MaybeCreateAndStoreReport)
      .WithArgs(std::move(trigger))
      .Then(base::BindOnce(&AttributionManagerImpl::OnReportStored,
                           weak_factory_.GetWeakPtr(), cleared_debug_key,
                           cookie_based_debug_allowed));
}

void AttributionManagerImpl::OnReportStored(
    std::optional<uint64_t> cleared_debug_key,
    bool cookie_based_debug_allowed,
    CreateReportResult result) {
  RecordCreateReportStatus(result);

  std::optional<base::Time> min_new_report_time;

  if (auto* report = result.new_event_level_report()) {
    min_new_report_time = report->report_time();
    MaybeSendDebugReport(std::move(*report));
  }

  if (auto* report = result.new_aggregatable_report()) {
    min_new_report_time = AttributionReport::MinReportTime(
        min_new_report_time, report->report_time());

    MaybeSendDebugReport(std::move(*report));
  }

  min_new_report_time = AttributionReport::MinReportTime(
      min_new_report_time, result.min_null_aggregatable_report_time());

  scheduler_timer_->MaybeSet(min_new_report_time);

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
    observer.OnTriggerHandled(cleared_debug_key, result);
  }

  MaybeSendVerboseDebugReport(cookie_based_debug_allowed, result);

  MaybeSendAggregatableDebugReport(result);
}

void AttributionManagerImpl::MaybeSendDebugReport(AttributionReport&& report) {
  if (!report.CanDebuggingBeEnabled() || !IsReportAllowed(report)) {
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
  OnUserVisibleTaskStarted();

  const int kMaxSources = 1000;
  attribution_resolver_.AsyncCall(&AttributionResolver::GetActiveSources)
      .WithArgs(kMaxSources)
      .Then(std::move(callback).Then(
          base::BindOnce(&AttributionManagerImpl::OnUserVisibleTaskComplete,
                         weak_factory_.GetWeakPtr())));
}

void AttributionManagerImpl::GetPendingReportsForInternalUse(
    int limit,
    base::OnceCallback<void(std::vector<AttributionReport>)> callback) {
  OnUserVisibleTaskStarted();

  attribution_resolver_.AsyncCall(&AttributionResolver::GetAttributionReports)
      .WithArgs(/*max_report_time=*/base::Time::Max(), limit)
      .Then(std::move(callback).Then(
          base::BindOnce(&AttributionManagerImpl::OnUserVisibleTaskComplete,
                         weak_factory_.GetWeakPtr())));
}

void AttributionManagerImpl::SendReportForWebUI(AttributionReport::Id id,
                                                base::OnceClosure done) {
  DCHECK(done);

  OnUserVisibleTaskStarted();
  done = std::move(done).Then(
      base::BindOnce(&AttributionManagerImpl::OnUserVisibleTaskComplete,
                     weak_factory_.GetWeakPtr()));

  attribution_resolver_.AsyncCall(&AttributionResolver::GetReport)
      .WithArgs(id)
      .Then(base::BindOnce(&AttributionManagerImpl::OnGetReportToSendFromWebUI,
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
    os_level_manager_->ClearData(
        delete_begin, delete_end, filter_builder->GetOrigins(),
        filter_builder->GetRegisterableDomains(), filter_builder->GetMode(),
        delete_rate_limit_data, std::move(barrier));
  } else {
    // When there is not filter_builder, we clear all the data.
    os_level_manager_->ClearData(delete_begin, delete_end, /*origins=*/{},
                                 /*domains=*/{},
                                 // By preserving data only from an empty list,
                                 // we are effectively clearing all the data.
                                 BrowsingDataFilterBuilder::Mode::kPreserve,
                                 delete_rate_limit_data, std::move(barrier));
  }

  // Rate-limit data is only deleted when initiated by a user, not a site via
  // the Clear-Site-Data header.
  if (delete_rate_limit_data) {
    OnUserVisibleTaskStarted();
  }

  attribution_resolver_.AsyncCall(&AttributionResolver::ClearData)
      .WithArgs(delete_begin, delete_end, std::move(filter),
                delete_rate_limit_data)
      .Then(std::move(done).Then(
          base::BindOnce(&AttributionManagerImpl::OnClearDataComplete,
                         weak_factory_.GetWeakPtr(),
                         /*was_user_visible=*/delete_rate_limit_data)));
}

void AttributionManagerImpl::OnUserVisibleTaskStarted() {
  // When a user-visible task is queued or running, we use a higher priority.
  ++num_pending_user_visible_tasks_;
  resolver_task_runner_->UpdatePriority(base::TaskPriority::USER_VISIBLE);
}

void AttributionManagerImpl::OnUserVisibleTaskComplete() {
  DCHECK_GT(num_pending_user_visible_tasks_, 0);
  --num_pending_user_visible_tasks_;

  // No more user-visible tasks, so we can reset the priority.
  if (num_pending_user_visible_tasks_ == 0) {
    resolver_task_runner_->UpdatePriority(base::TaskPriority::BEST_EFFORT);
  }
}

void AttributionManagerImpl::OnClearDataComplete(bool was_user_visible) {
  if (was_user_visible) {
    OnUserVisibleTaskComplete();
  }
  NotifySourcesChanged();
  NotifyReportsChanged();
}

void AttributionManagerImpl::GetAllDataKeys(
    base::OnceCallback<void(std::set<DataKey>)> callback) {
  OnUserVisibleTaskStarted();
  attribution_resolver_.AsyncCall(&AttributionResolver::GetAllDataKeys)
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

  attribution_resolver_.AsyncCall(&AttributionResolver::DeleteByDataKey)
      .WithArgs(data_key)
      .Then(std::move(callback).Then(base::BindOnce(
          &AttributionManagerImpl::OnClearDataComplete,
          weak_factory_.GetWeakPtr(), /*was_user_visible=*/true)));
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
  attribution_resolver_.AsyncCall(&AttributionResolver::GetAttributionReports)
      .WithArgs(/*max_report_time=*/base::Time::Now(), /*limit=*/-1)
      .Then(base::BindOnce(&AttributionManagerImpl::SendReports,
                           weak_factory_.GetWeakPtr()));
}

void AttributionManagerImpl::OnGetReportToSendFromWebUI(
    base::OnceClosure done,
    std::optional<AttributionReport> report) {
  DCHECK(done);

  if (!report.has_value()) {
    std::move(done).Run();
    return;
  }

  const base::Time now = base::Time::Now();
  report->set_report_time(now);
  SendReport(std::move(done), now, *std::move(report));
}

void AttributionManagerImpl::SendReports(
    std::vector<AttributionReport> reports) {
  const base::Time now = base::Time::Now();
  for (auto& report : reports) {
    SendReport(base::NullCallback(), now, std::move(report));
  }
}

// If `web_ui_callback` is null, assumes that `report` is being sent at its
// intended time, and logs metrics for it. Otherwise, does not log metrics.
void AttributionManagerImpl::SendReport(base::OnceClosure web_ui_callback,
                                        const base::Time now,
                                        AttributionReport report) {
  DCHECK_LE(report.report_time(), now);

  bool inserted = reports_being_sent_.emplace(report.id()).second;
  if (!inserted) {
    if (web_ui_callback) {
      std::move(web_ui_callback).Run();
    }
    return;
  }

  if (!IsReportAllowed(report)) {
    // If measurement is disallowed, just drop the report on the floor. We
    // need to make sure we forward that the report was "sent" to ensure it is
    // deleted from storage, etc. This simulates sending the report through a
    // null channel.
    OnReportSent(std::move(web_ui_callback), std::move(report),
                 SendResult(SendResult::Dropped()));
    return;
  }

  if (!web_ui_callback) {
    LogMetricsOnReportSend(report, now);
  }

  PrepareToSendReport(
      std::move(report), /*is_debug_report=*/false,
      base::BindOnce(&AttributionManagerImpl::OnReportSent,
                     weak_factory_.GetWeakPtr(), std::move(web_ui_callback)));
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
      SendReport(std::move(report), is_debug_report, std::move(callback));
      break;
    case AttributionReport::Type::kAggregatableAttribution:
    case AttributionReport::Type::kNullAggregatable:
      AssembleAggregatableReport(std::move(report), is_debug_report,
                                 std::move(callback));
      break;
  }
}

void AttributionManagerImpl::SendReport(AttributionReport report,
                                        bool is_debug_report,
                                        ReportSentCallback callback) {
  report_sender_->SendReport(
      std::move(report), is_debug_report,
      base::BindOnce(
          [](ReportSentCallback callback, const AttributionReport& report,
             SendResult::Sent sent) {
            std::move(callback).Run(report, SendResult(std::move(sent)));
          },
          std::move(callback)));
}

void AttributionManagerImpl::OnReportSent(base::OnceClosure done,
                                          const AttributionReport& report,
                                          SendResult info) {
  // If there was a transient failure, and another attempt is allowed,
  // update the report's DB state to reflect that. Otherwise, delete the report
  // from storage.

  std::optional<base::Time> new_report_time =
      absl::visit(base::Overloaded{
                      [&](SendResult::Sent sent) -> std::optional<base::Time> {
                        switch (sent.result) {
                          case SendResult::Sent::Result::kSent:
                            LogMetricsOnReportSent(report,
                                                   third_retry_enabled_);
                            return std::nullopt;
                          case SendResult::Sent::Result::kTransientFailure:
                            RecordNetworkConnectionTypeOnFailure(
                                report.GetReportType(),
                                scheduler_timer_->connection_type());
                            return HandleTransientFailureOnSendReport(
                                report, third_retry_enabled_);
                          case SendResult::Sent::Result::kFailure:
                            RecordNetworkConnectionTypeOnFailure(
                                report.GetReportType(),
                                scheduler_timer_->connection_type());
                            return std::nullopt;
                        }
                      },
                      [](SendResult::Dropped) -> std::optional<base::Time> {
                        return std::nullopt;
                      },
                      [&](SendResult::AssemblyFailure failure)
                          -> std::optional<base::Time> {
                        // TODO(linnan): Retry on transient assembly failure
                        // isn't privacy sensitive, therefore we could consider
                        // subjecting these failures to a different limit.
                        return failure.transient
                                   ? HandleTransientFailureOnSendReport(
                                         report, third_retry_enabled_)
                                   : std::nullopt;
                      },
                  },
                  info.result);

  base::OnceCallback then = base::BindOnce(
      [](base::OnceClosure done, base::WeakPtr<AttributionManagerImpl> manager,
         AttributionReport::Id report_id,
         std::optional<base::Time> new_report_time, bool success) {
        if (done) {
          std::move(done).Run();
        }

        if (manager && success) {
          manager->MarkReportCompleted(report_id);
          manager->scheduler_timer_->MaybeSet(new_report_time);
          manager->NotifyReportsChanged();
        }
      },
      std::move(done), weak_factory_.GetWeakPtr(), report.id(),
      new_report_time);

  if (new_report_time) {
    attribution_resolver_
        .AsyncCall(&AttributionResolver::UpdateReportForSendFailure)
        .WithArgs(report.id(), *new_report_time)
        .Then(std::move(then));

    // TODO(apaseltiner): Consider surfacing retry attempts in internals UI.

    return;
  }

  NotifyReportSent(/*is_debug_report=*/false, report, info);

  attribution_resolver_.AsyncCall(&AttributionResolver::DeleteReport)
      .WithArgs(report.id())
      .Then(std::move(then));

  LogMetricsOnReportCompleted(report, info.status());
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
    std::move(callback).Run(
        std::move(report),
        SendResult(SendResult::AssemblyFailure(/*transient=*/false)));
    return;
  }

  std::optional<AggregatableReportRequest> request =
      CreateAggregatableReportRequest(report);
  if (!request.has_value()) {
    RecordAssembleAggregatableReportStatus(
        AssembleAggregatableReportStatus::kCreateRequestFailed);
    std::move(callback).Run(
        std::move(report),
        SendResult(SendResult::AssemblyFailure(/*transient=*/false)));
    return;
  }

  aggregation_service->AssembleReport(
      *std::move(request),
      base::BindOnce(&AttributionManagerImpl::OnAggregatableReportAssembled,
                     weak_factory_.GetWeakPtr(), std::move(report),
                     is_debug_report, std::move(callback)));
}

void AttributionManagerImpl::OnAggregatableReportAssembled(
    AttributionReport report,
    bool is_debug_report,
    ReportSentCallback callback,
    AggregatableReportRequest,
    std::optional<AggregatableReport> assembled_report,
    AggregationService::AssemblyStatus) {
  if (!assembled_report.has_value()) {
    RecordAssembleAggregatableReportStatus(
        AssembleAggregatableReportStatus::kAssembleReportFailed);
    std::move(callback).Run(
        std::move(report),
        SendResult(SendResult::AssemblyFailure(/*transient=*/true)));
    return;
  }

  auto* data =
      absl::get_if<AttributionReport::AggregatableData>(&report.data());
  CHECK(data);
  data->SetAssembledReport(std::move(assembled_report));

  RecordAssembleAggregatableReportStatus(
      AssembleAggregatableReportStatus::kSuccess);

  SendReport(std::move(report), is_debug_report, std::move(callback));
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

void AttributionManagerImpl::MaybeSendAggregatableDebugReport(
    const StoreSourceResult& result) {
  const auto is_operation_allowed = [&]() {
    return IsOperationAllowed(
        *storage_partition_,
        ContentBrowserClient::AttributionReportingOperation::
            kSourceAggregatableDebugReport,
        /*rfh=*/nullptr, &*result.source().common_info().source_origin(),
        /*destination_origin=*/nullptr,
        &*result.source().common_info().reporting_origin());
  };

  if (std::optional<AggregatableDebugReport> debug_report =
          AggregatableDebugReport::Create(is_operation_allowed, result)) {
    std::optional<StoredSource::Id> source_id;
    if (const auto* success =
            absl::get_if<StoreSourceResult::Success>(&result.result())) {
      source_id.emplace(success->source_id);
    }

    attribution_resolver_
        .AsyncCall(&AttributionResolver::ProcessAggregatableDebugReport)
        .WithArgs(*std::move(debug_report),
                  result.source()
                      .registration()
                      .aggregatable_debug_reporting_config.budget(),
                  source_id)
        .Then(base::BindOnce(
            &AttributionManagerImpl::OnAggregatableDebugReportProcessed,
            weak_factory_.GetWeakPtr()));
  }
}

void AttributionManagerImpl::MaybeSendAggregatableDebugReport(
    const CreateReportResult& result) {
  const auto is_operation_allowed = [&]() {
    return IsOperationAllowed(
        *storage_partition_,
        ContentBrowserClient::AttributionReportingOperation::
            kTriggerAggregatableDebugReport,
        /*rfh=*/nullptr,
        /*source_origin=*/nullptr, &*result.trigger().destination_origin(),
        &*result.trigger().reporting_origin());
  };

  if (std::optional<AggregatableDebugReport> debug_report =
          AggregatableDebugReport::Create(is_operation_allowed, result)) {
    std::optional<StoredSource::Id> source_id;
    if (const std::optional<StoredSource>& source = result.source();
        source.has_value()) {
      source_id.emplace(source->source_id());
    }
    attribution_resolver_
        .AsyncCall(&AttributionResolver::ProcessAggregatableDebugReport)
        .WithArgs(*std::move(debug_report),
                  /*remaining_budget=*/std::nullopt, source_id)
        .Then(base::BindOnce(
            &AttributionManagerImpl::OnAggregatableDebugReportProcessed,
            weak_factory_.GetWeakPtr()));
  }
}

void AttributionManagerImpl::OnAggregatableDebugReportProcessed(
    ProcessAggregatableDebugReportResult result) {
  AggregationService* aggregation_service =
      storage_partition_->GetAggregationService();
  if (!aggregation_service) {
    NotifyAggregatableDebugReportSent(
        result.report, /*report_body=*/base::Value::Dict(), result.result,
        SendAggregatableDebugReportResult(
            SendAggregatableDebugReportResult::AssemblyFailed()));
    return;
  }
  std::optional<AggregatableReportRequest> request =
      result.report.CreateAggregatableReportRequest();
  if (!request.has_value()) {
    NotifyAggregatableDebugReportSent(
        result.report, /*report_body=*/base::Value::Dict(), result.result,
        SendAggregatableDebugReportResult(
            SendAggregatableDebugReportResult::AssemblyFailed()));
    return;
  }

  aggregation_service->AssembleReport(
      *std::move(request),
      base::BindOnce(
          &AttributionManagerImpl::OnAggregatableDebugReportAssembled,
          weak_factory_.GetWeakPtr(), std::move(result)));
}

void AttributionManagerImpl::OnAggregatableDebugReportAssembled(
    ProcessAggregatableDebugReportResult result,
    AggregatableReportRequest,
    std::optional<AggregatableReport> assembled_report,
    AggregationService::AssemblyStatus) {
  if (!assembled_report.has_value()) {
    NotifyAggregatableDebugReportSent(
        result.report, /*report_body=*/base::Value::Dict(), result.result,
        SendAggregatableDebugReportResult(
            SendAggregatableDebugReportResult::AssemblyFailed()));
    return;
  }

  report_sender_->SendReport(
      std::move(result.report), assembled_report->GetAsJson(),
      base::BindOnce(
          [](base::WeakPtr<AttributionManagerImpl> manager,
             attribution_reporting::mojom::ProcessAggregatableDebugReportResult
                 process_result,
             const AggregatableDebugReport& report, base::ValueView report_body,
             int status) {
            if (!manager) {
              return;
            }

            manager->NotifyAggregatableDebugReportSent(
                report, report_body, process_result,
                SendAggregatableDebugReportResult(
                    SendAggregatableDebugReportResult::Sent(status)));
          },
          weak_factory_.GetWeakPtr(), result.result));
}

void AttributionManagerImpl::NotifyAggregatableDebugReportSent(
    const AggregatableDebugReport& report,
    base::ValueView report_body,
    attribution_reporting::mojom::ProcessAggregatableDebugReportResult
        process_result,
    SendAggregatableDebugReportResult send_result) {
  for (auto& observer : observers_) {
    observer.OnAggregatableDebugReportSent(report, report_body, process_result,
                                           send_result);
  }
}

void AttributionManagerImpl::MaybeSendVerboseDebugReport(
    const StoreSourceResult& result) {
  const auto is_operation_allowed = [&]() {
    return IsOperationAllowed(
        *storage_partition_,
        ContentBrowserClient::AttributionReportingOperation::
            kSourceVerboseDebugReport,
        /*rfh=*/nullptr, &*result.source().common_info().source_origin(),
        /*destination_origin=*/nullptr,
        &*result.source().common_info().reporting_origin());
  };

  if (std::optional<AttributionDebugReport> debug_report =
          AttributionDebugReport::Create(is_operation_allowed, result)) {
    report_sender_->SendReport(
        *std::move(debug_report),
        base::BindOnce(&AttributionManagerImpl::NotifyDebugReportSent,
                       weak_factory_.GetWeakPtr()));
  }
}

void AttributionManagerImpl::MaybeSendVerboseDebugReport(
    bool cookie_based_debug_allowed,
    const CreateReportResult& result) {
  const auto is_operation_allowed = [&]() {
    return IsOperationAllowed(
        *storage_partition_,
        ContentBrowserClient::AttributionReportingOperation::
            kTriggerVerboseDebugReport,
        /*rfh=*/nullptr,
        /*source_origin=*/nullptr, &*result.trigger().destination_origin(),
        &*result.trigger().reporting_origin());
  };

  if (std::optional<AttributionDebugReport> debug_report =
          AttributionDebugReport::Create(is_operation_allowed,
                                         cookie_based_debug_allowed, result)) {
    report_sender_->SendReport(
        *std::move(debug_report),
        base::BindOnce(&AttributionManagerImpl::NotifyDebugReportSent,
                       weak_factory_.GetWeakPtr()));
  }
}

void AttributionManagerImpl::HandleOsRegistration(OsRegistration registration) {
  ContentBrowserClient::AttributionReportingOperation registration_operation;
  ContentBrowserClient::AttributionReportingOperation debug_operation;
  const url::Origin* source_origin;
  const url::Origin* destination_origin;
  switch (registration.GetType()) {
    case RegistrationType::kSource:
      registration_operation =
          ContentBrowserClient::AttributionReportingOperation::kOsSource;
      debug_operation = ContentBrowserClient::AttributionReportingOperation::
          kOsSourceTransitionalDebugReporting;
      source_origin = &registration.top_level_origin;
      destination_origin = nullptr;
      break;
    case RegistrationType::kTrigger:
      registration_operation =
          ContentBrowserClient::AttributionReportingOperation::kOsTrigger;
      debug_operation = ContentBrowserClient::AttributionReportingOperation::
          kOsTriggerTransitionalDebugReporting;
      source_origin = nullptr;
      destination_origin = &registration.top_level_origin;
      break;
  }

  std::vector<bool> debug_allowed;

  std::erase_if(
      registration.registration_items,
      [&, now = base::Time::Now()](const OsRegistrationItem& item) {
        const auto registration_origin = url::Origin::Create(item.url);
        if (registration_origin.opaque()) {
          NotifyOsRegistration(now, item, registration.top_level_origin,
                               /*is_debug_key_allowed=*/false,
                               registration.GetType(),
                               OsRegistrationResult::kInvalidRegistrationUrl);
          return true;
        }

        BrowserPolicy browser_policy = GetBrowserPolicy(
            *storage_partition_, registration_operation, debug_operation,
            registration.render_frame_id, source_origin, destination_origin,
            registration_origin,
            /*check_debug=*/true);

        switch (browser_policy) {
          case BrowserPolicy::kProhibited:
            NotifyOsRegistration(
                now, item, registration.top_level_origin,
                /*is_debug_key_allowed=*/false, registration.GetType(),
                OsRegistrationResult::kProhibitedByBrowserPolicy);
            return true;
          case BrowserPolicy::kAllowedWithDebug:
            debug_allowed.push_back(true);
            return false;
          case BrowserPolicy::kAllowedWithoutDebug:
            debug_allowed.push_back(false);
            return false;
        }

        NOTREACHED();
      });

  if (registration.registration_items.empty()) {
    return;
  }

  os_level_manager_->Register(
      std::move(registration), debug_allowed,
      base::BindOnce(&AttributionManagerImpl::OnOsRegistration,
                     weak_factory_.GetWeakPtr(), debug_allowed));
}

void AttributionManagerImpl::NotifyOsRegistration(
    base::Time time,
    const OsRegistrationItem& registration,
    const url::Origin& top_level_origin,
    bool is_debug_key_allowed,
    attribution_reporting::mojom::RegistrationType type,
    attribution_reporting::mojom::OsRegistrationResult result) {
  for (auto& observer : observers_) {
    observer.OnOsRegistration(time, registration, top_level_origin, type,
                              is_debug_key_allowed, result);
  }
  switch (type) {
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
    const std::vector<bool>& is_debug_key_allowed,
    const OsRegistration& registration,
    const std::vector<bool>& success) {
  const size_t num_items = registration.registration_items.size();

  CHECK_EQ(num_items, is_debug_key_allowed.size());
  CHECK_EQ(num_items, success.size());

  MaybeSendVerboseDebugReports(registration);

  const base::Time now = base::Time::Now();

  for (size_t i = 0; i < num_items; ++i) {
    auto result = success[i] ? OsRegistrationResult::kPassedToOs
                             : OsRegistrationResult::kRejectedByOs;

    NotifyOsRegistration(now, registration.registration_items[i],
                         registration.top_level_origin, is_debug_key_allowed[i],
                         registration.GetType(), result);
  }
}

void AttributionManagerImpl::SetDebugMode(std::optional<bool> enabled,
                                          base::OnceClosure done) {
  bool debug_mode =
      enabled.value_or(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAttributionReportingDebugMode));

  attribution_resolver_.AsyncCall(&AttributionResolver::SetDelegate)
      .WithArgs(MakeResolverDelegate(debug_mode))
      .Then(std::move(done).Then(base::BindOnce(
          [](base::WeakPtr<AttributionManagerImpl> manager,
             const bool debug_mode) {
            if (manager) {
              manager->debug_mode_ = debug_mode;
              for (auto& observer : manager->observers_) {
                observer.OnDebugModeChanged(debug_mode);
              }
            }
          },
          weak_factory_.GetWeakPtr(), debug_mode)));
}

void AttributionManagerImpl::MaybeSendVerboseDebugReports(
    const OsRegistration& registration) {
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

  const auto is_operation_allowed =
      [&](const url::Origin& registration_origin) {
        return IsOperationAllowed(*storage_partition_, operation,
                                  /*rfh=*/nullptr, source_origin,
                                  destination_origin,
                                  /*reporting_origin=*/&registration_origin);
      };

  for (size_t i = 0; i < registration.registration_items.size(); ++i) {
    if (std::optional<AttributionDebugReport> debug_report =
            AttributionDebugReport::Create(registration, /*item_index=*/i,
                                           is_operation_allowed)) {
      report_sender_->SendReport(
          *std::move(debug_report),
          base::BindOnce(&AttributionManagerImpl::NotifyDebugReportSent,
                         weak_factory_.GetWeakPtr()));
    }
  }
}

void AttributionManagerImpl::ReportRegistrationHeaderError(
    attribution_reporting::SuitableOrigin reporting_origin,
    attribution_reporting::RegistrationHeaderError error,
    const attribution_reporting::SuitableOrigin& context_origin,
    bool is_within_fenced_frame,
    GlobalRenderFrameHostId render_frame_id) {
  const auto is_operation_allowed = [&](const url::Origin& reporting_origin) {
    return GetContentClient()
        ->browser()
        ->IsAttributionReportingAllowedForContext(
            storage_partition_->browser_context(),
            RenderFrameHost::FromID(render_frame_id), *context_origin,
            reporting_origin);
  };

  if (std::optional<AttributionDebugReport> debug_report =
          AttributionDebugReport::Create(
              std::move(reporting_origin), std::move(error), context_origin,
              is_within_fenced_frame, is_operation_allowed)) {
    report_sender_->SendReport(
        *std::move(debug_report),
        base::BindOnce(&AttributionManagerImpl::NotifyDebugReportSent,
                       weak_factory_.GetWeakPtr()));
  }
}

}  // namespace content
