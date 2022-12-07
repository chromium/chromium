// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/attribution_simulator.h"

#include <stddef.h>
#include <limits>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/functional/overloaded.h"
#include "base/guid.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task/updateable_sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/time/time_to_iso8601.h"
#include "base/values.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "content/browser/aggregation_service/aggregation_service_features.h"
#include "content/browser/aggregation_service/aggregation_service_impl.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/attribution_reporting/aggregatable_attribution_utils.h"
#include "content/browser/attribution_reporting/attribution_cookie_checker.h"
#include "content/browser/attribution_reporting/attribution_cookie_checker_impl.h"
#include "content/browser/attribution_reporting/attribution_debug_report.h"
#include "content/browser/attribution_reporting/attribution_default_random_generator.h"
#include "content/browser/attribution_reporting/attribution_insecure_random_generator.h"
#include "content/browser/attribution_reporting/attribution_manager_impl.h"
#include "content/browser/attribution_reporting/attribution_observer.h"
#include "content/browser/attribution_reporting/attribution_observer_types.h"
#include "content/browser/attribution_reporting/attribution_random_generator.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_report_sender.h"
#include "content/browser/attribution_reporting/attribution_storage_delegate_impl.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/send_result.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/attribution_simulator_input_parser.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_options.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

namespace content {

namespace {

base::Time GetEventTime(const AttributionSimulationEventAndValue& event) {
  return absl::visit(
      base::Overloaded{
          [](const StorableSource& source) {
            return source.common_info().source_time();
          },
          [](const AttributionTriggerAndTime& trigger) { return trigger.time; },
          [](const AttributionSimulatorCookie& cookie) {
            return cookie.cookie.CreationDate();
          },
          [](const AttributionDataClear& clear) { return clear.time; },
      },
      event.first);
}

class AlwaysSetCookieChecker : public AttributionCookieChecker {
 public:
  AlwaysSetCookieChecker() = default;

  ~AlwaysSetCookieChecker() override = default;

  AlwaysSetCookieChecker(const AlwaysSetCookieChecker&) = delete;
  AlwaysSetCookieChecker(AlwaysSetCookieChecker&&) = delete;

  AlwaysSetCookieChecker& operator=(const AlwaysSetCookieChecker&) = delete;
  AlwaysSetCookieChecker& operator=(AlwaysSetCookieChecker&&) = delete;

 private:
  // AttributionManagerImpl::CookieChecker:
  void IsDebugCookieSet(const url::Origin& origin,
                        base::OnceCallback<void(bool)> callback) override {
    std::move(callback).Run(true);
  }
};

struct AttributionReportJsonConverter {
  AttributionReportJsonConverter(AttributionSimulationOutputOptions options,
                                 base::Time time_origin)
      : options(options), time_origin(time_origin) {}

  base::Value::Dict ToJson(
      const AttributionReport& report,
      bool is_debug_report,
      const absl::optional<base::GUID>& replaced_by = absl::nullopt) const {
    base::Value::Dict report_body = report.ReportBody();
    if (options.remove_report_ids)
      report_body.Remove("report_id");

    if (options.remove_assembled_report &&
        absl::holds_alternative<AttributionReport::AggregatableAttributionData>(
            report.data())) {
      // Output attribution_destination from the shared_info field.
      absl::optional<base::Value> shared_info =
          report_body.Extract("shared_info");
      DCHECK(shared_info);
      std::string* shared_info_str = shared_info->GetIfString();
      DCHECK(shared_info_str);

      base::Value shared_info_value = base::test::ParseJson(*shared_info_str);
      DCHECK(shared_info_value.is_dict());

      static constexpr char kKeyAttributionDestination[] =
          "attribution_destination";
      std::string* attribution_destination =
          shared_info_value.GetDict().FindString(kKeyAttributionDestination);
      DCHECK(attribution_destination);
      DCHECK(!report_body.contains(kKeyAttributionDestination));
      report_body.Set(kKeyAttributionDestination,
                      std::move(*attribution_destination));

      report_body.Remove("aggregation_service_payloads");
      report_body.Remove("source_registration_time");
    }

    base::Value::Dict value;
    value.Set("report", std::move(report_body));
    value.Set("report_url", report.ReportURL(is_debug_report).spec());

    value.Set("intended_report_time",
              FormatTime(is_debug_report ? report.attribution_info().time
                                         : report.report_time()));

    if (replaced_by) {
      value.Set("replacement_time", FormatTime(base::Time::Now()));
    } else if (!options.remove_actual_report_times) {
      value.Set("report_time", FormatTime(base::Time::Now()));
    }

    base::Value::Dict test_info;
    if (absl::holds_alternative<AttributionReport::EventLevelData>(
            report.data())) {
      test_info.Set("randomized_trigger",
                    report.attribution_info().source.attribution_logic() ==
                        StoredSource::AttributionLogic::kFalsely);
    } else {
      auto* aggregatable_data =
          absl::get_if<AttributionReport::AggregatableAttributionData>(
              &report.data());
      DCHECK(aggregatable_data);
      base::Value::List list;
      for (const auto& contribution : aggregatable_data->contributions) {
        base::Value::Dict dict;
        dict.Set("key", attribution_reporting::HexEncodeAggregationKey(
                            contribution.key()));
        dict.Set("value", base::checked_cast<int>(contribution.value()));

        list.Append(std::move(dict));
      }
      test_info.Set("histograms", std::move(list));
    }
    value.Set("test_info", std::move(test_info));

    if (!options.remove_report_ids && replaced_by) {
      value.Set("replaced_by", replaced_by->AsLowercaseString());
    }

    return value;
  }

  base::Value::Dict ToJson(const AttributionDebugReport& report,
                           base::Time time) const {
    base::Value::List report_body = report.ReportBody();
    if (options.remove_report_ids) {
      for (auto& value : report_body) {
        base::Value::Dict* dict = value.GetIfDict();
        DCHECK(dict);
        dict->RemoveByDottedPath("body.report_id");
      }
    }

    base::Value::Dict value;
    value.Set("report", std::move(report_body));
    value.Set("report_url", report.ReportURL().spec());
    value.Set("report_time", FormatTime(time));
    return value;
  }

  std::string FormatTime(base::Time time) const {
    base::TimeDelta time_delta = time - time_origin;

    switch (options.report_time_format) {
      case AttributionReportTimeFormat::kMillisecondsSinceUnixEpoch:
        return base::NumberToString(time_delta.InMilliseconds());
      case AttributionReportTimeFormat::kISO8601:
        return base::TimeToISO8601(base::Time::UnixEpoch() + time_delta);
    }
  }

  const AttributionSimulationOutputOptions options;
  const base::Time time_origin;
};

class FakeReportSender : public AttributionReportSender {
 public:
  FakeReportSender() = default;

  ~FakeReportSender() override = default;

  FakeReportSender(const FakeReportSender&) = delete;
  FakeReportSender(FakeReportSender&&) = delete;

  FakeReportSender& operator=(const FakeReportSender&) = delete;
  FakeReportSender& operator=(FakeReportSender&&) = delete;

 private:
  // AttributionManagerImpl::ReportSender:
  void SendReport(AttributionReport report,
                  bool is_debug_report,
                  ReportSentCallback sent_callback) override {
    std::move(sent_callback)
        .Run(std::move(report), SendResult(SendResult::Status::kSent,
                                           /*http_response_code=*/200));
  }

  void SendReport(AttributionDebugReport report,
                  DebugReportSentCallback done) override {
    std::move(done).Run(std::move(report), /*status=*/200);
  }
};

// Registers sources and triggers in the `AttributionManagerImpl` and records
// rejected sources in a JSON list.
class AttributionEventHandler : public AttributionObserver {
 public:
  AttributionEventHandler(AttributionManagerImpl* manager,
                          StoragePartitionImpl* storage_partition,
                          AttributionReportJsonConverter json_converter)
      : manager_(manager),
        storage_partition_(storage_partition),
        json_converter_(json_converter) {
    DCHECK(manager_);
    DCHECK(storage_partition_);

    observation_.Observe(manager);
  }

  ~AttributionEventHandler() override = default;

  void Handle(AttributionSimulationEventAndValue event) {
    // Sources and triggers are handled in order; this includes observer
    // invocations. Therefore, we can track the original `base::Value`
    // associated with the event using a queue.

    input_values_.push_back(std::move(event.second));
    absl::visit(*this, std::move(event.first));
  }

  // For use with `absl::visit()`.
  void operator()(StorableSource source) {
    manager_->HandleSource(std::move(source));
    FlushCookies();
  }

  // For use with `absl::visit()`.
  void operator()(AttributionTriggerAndTime trigger) {
    manager_->HandleTrigger(std::move(trigger.trigger));
    FlushCookies();
  }

  // For use with `absl::visit()`.
  void operator()(AttributionSimulatorCookie cookie) {
    DCHECK(!input_values_.empty());
    input_values_.pop_front();

    // TODO(apaseltiner): Consider surfacing `net::CookieAccessResult` in
    // output.

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &network::mojom::CookieManager::SetCanonicalCookie,
            base::Unretained(
                storage_partition_->GetCookieManagerForBrowserProcess()),
            cookie.cookie, cookie.source_url,
            net::CookieOptions::MakeAllInclusive(), base::DoNothing()));
  }

  // For use with `absl::visit()`.
  void operator()(AttributionDataClear clear) {
    DCHECK(!input_values_.empty());
    input_values_.pop_front();

    StoragePartition::StorageKeyMatcherFunction filter;
    if (clear.origins.has_value()) {
      filter =
          base::BindLambdaForTesting([origins = std::move(*clear.origins)](
                                         const blink::StorageKey& storage_key) {
            return origins.contains(storage_key.origin());
          });
    }

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&AttributionManagerImpl::ClearData,
                       base::Unretained(manager_), clear.delete_begin,
                       clear.delete_end, std::move(filter),
                       /*filter_builder=*/nullptr,
                       /*delete_rate_limit_data=*/true, base::DoNothing()));
  }

  base::Value::Dict TakeOutput() {
    base::Value::Dict output;

    if (!event_level_reports_.empty()) {
      output.Set("event_level_reports",
                 std::exchange(event_level_reports_, {}));
    }

    if (!debug_event_level_reports_.empty()) {
      output.Set("debug_event_level_reports",
                 std::exchange(debug_event_level_reports_, {}));
    }

    if (!aggregatable_reports_.empty()) {
      output.Set("aggregatable_reports",
                 std::exchange(aggregatable_reports_, {}));
    }

    if (!debug_aggregatable_reports_.empty()) {
      output.Set("debug_aggregatable_reports",
                 std::exchange(debug_aggregatable_reports_, {}));
    }

    if (!verbose_debug_reports_.empty()) {
      output.Set("verbose_debug_reports",
                 std::exchange(verbose_debug_reports_, {}));
    }

    if (!rejected_sources_.empty())
      output.Set("rejected_sources", std::exchange(rejected_sources_, {}));

    if (!rejected_triggers_.empty())
      output.Set("rejected_triggers", std::exchange(rejected_triggers_, {}));

    if (!replaced_event_level_reports_.empty()) {
      output.Set("replaced_event_level_reports",
                 std::exchange(replaced_event_level_reports_, {}));
    }

    return output;
  }

 private:
  void FlushCookies() {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &network::mojom::CookieManager::FlushCookieStore,
            base::Unretained(
                storage_partition_->GetCookieManagerForBrowserProcess()),
            base::DoNothing()));
  }

  // AttributionObserver:

  void OnSourceHandled(const StorableSource& source,
                       absl::optional<uint64_t> cleared_debug_key,
                       StorableSource::Result result) override {
    DCHECK(!input_values_.empty());
    base::Value input_value = std::move(input_values_.front());
    input_values_.pop_front();

    std::ostringstream reason;
    switch (result) {
      case StorableSource::Result::kSuccess:
      case StorableSource::Result::kSuccessNoised:
        return;
      case StorableSource::Result::kInternalError:
      case StorableSource::Result::kInsufficientSourceCapacity:
      case StorableSource::Result::kInsufficientUniqueDestinationCapacity:
      case StorableSource::Result::kExcessiveReportingOrigins:
      case StorableSource::Result::kProhibitedByBrowserPolicy:
        reason << result;
        break;
    }

    base::Value::Dict dict;
    dict.Set("reason", reason.str());
    dict.Set("source", std::move(input_value));

    rejected_sources_.Append(std::move(dict));
  }

  void OnTriggerHandled(const AttributionTrigger& trigger,
                        absl::optional<uint64_t> cleared_debug_key,
                        const CreateReportResult& result) override {
    DCHECK(!input_values_.empty());
    base::Value input_value = std::move(input_values_.front());
    input_values_.pop_front();

    std::ostringstream event_level_reason;
    switch (result.event_level_status()) {
      case AttributionTrigger::EventLevelResult::kSuccess:
        break;
      case AttributionTrigger::EventLevelResult::kSuccessDroppedLowerPriority:
        replaced_event_level_reports_.Append(json_converter_.ToJson(
            *result.replaced_event_level_report(),
            /*is_debug_report=*/false,
            result.new_event_level_report()->external_report_id()));
        break;
      case AttributionTrigger::EventLevelResult::kInternalError:
      case AttributionTrigger::EventLevelResult::
          kNoCapacityForConversionDestination:
      case AttributionTrigger::EventLevelResult::kNoMatchingImpressions:
      case AttributionTrigger::EventLevelResult::kDeduplicated:
      case AttributionTrigger::EventLevelResult::kExcessiveAttributions:
      case AttributionTrigger::EventLevelResult::kPriorityTooLow:
      case AttributionTrigger::EventLevelResult::kDroppedForNoise:
      case AttributionTrigger::EventLevelResult::kExcessiveReportingOrigins:
      case AttributionTrigger::EventLevelResult::kNoMatchingSourceFilterData:
      case AttributionTrigger::EventLevelResult::kProhibitedByBrowserPolicy:
      case AttributionTrigger::EventLevelResult::kNoMatchingConfigurations:
      case AttributionTrigger::EventLevelResult::kExcessiveReports:
      case AttributionTrigger::EventLevelResult::kFalselyAttributedSource:
      case AttributionTrigger::EventLevelResult::kReportWindowPassed:
        event_level_reason << result.event_level_status();
        break;
    }

    std::ostringstream aggregatable_reason;
    switch (result.aggregatable_status()) {
      case AttributionTrigger::AggregatableResult::kSuccess:
      case AttributionTrigger::AggregatableResult::kNotRegistered:
        break;
      case AttributionTrigger::AggregatableResult::kInternalError:
      case AttributionTrigger::AggregatableResult::
          kNoCapacityForConversionDestination:
      case AttributionTrigger::AggregatableResult::kNoMatchingImpressions:
      case AttributionTrigger::AggregatableResult::kExcessiveAttributions:
      case AttributionTrigger::AggregatableResult::kExcessiveReportingOrigins:
      case AttributionTrigger::AggregatableResult::kInsufficientBudget:
      case AttributionTrigger::AggregatableResult::kNoMatchingSourceFilterData:
      case AttributionTrigger::AggregatableResult::kNoHistograms:
      case AttributionTrigger::AggregatableResult::kProhibitedByBrowserPolicy:
      case AttributionTrigger::AggregatableResult::kDeduplicated:
      case AttributionTrigger::AggregatableResult::kReportWindowPassed:
        aggregatable_reason << result.aggregatable_status();
        break;
    }

    std::string event_level_reason_str = event_level_reason.str();
    std::string aggregatable_reason_str = aggregatable_reason.str();

    if (event_level_reason_str.empty() && aggregatable_reason_str.empty())
      return;

    base::Value::Dict dict;
    if (!event_level_reason_str.empty())
      dict.Set("event_level_reason", std::move(event_level_reason_str));

    if (!aggregatable_reason_str.empty())
      dict.Set("aggregatable_reason", std::move(aggregatable_reason_str));

    dict.Set("trigger", std::move(input_value));

    rejected_triggers_.Append(std::move(dict));
  }

  void OnReportSent(const AttributionReport& report,
                    bool is_debug_report,
                    const SendResult& info) override {
    DCHECK_EQ(info.status, SendResult::Status::kSent);

    base::Value::List* reports;
    switch (report.GetReportType()) {
      case AttributionReport::Type::kEventLevel:
        reports = is_debug_report ? &debug_event_level_reports_
                                  : &event_level_reports_;
        break;
      case AttributionReport::Type::kAggregatableAttribution:
        reports = is_debug_report ? &debug_aggregatable_reports_
                                  : &aggregatable_reports_;
        break;
    }

    reports->Append(json_converter_.ToJson(report, is_debug_report));
  }

  void OnDebugReportSent(const AttributionDebugReport& report,
                         int status,
                         base::Time time) override {
    DCHECK_EQ(status, 200);
    verbose_debug_reports_.Append(json_converter_.ToJson(report, time));
  }

  base::ScopedObservation<AttributionManagerImpl, AttributionObserver>
      observation_{this};

  const base::raw_ptr<AttributionManagerImpl> manager_;
  const base::raw_ptr<StoragePartitionImpl> storage_partition_;

  const AttributionReportJsonConverter json_converter_;

  base::Value::List rejected_sources_;
  base::Value::List rejected_triggers_;
  base::Value::List replaced_event_level_reports_;

  base::Value::List event_level_reports_;
  base::Value::List debug_event_level_reports_;
  base::Value::List aggregatable_reports_;
  base::Value::List debug_aggregatable_reports_;
  base::Value::List verbose_debug_reports_;

  base::circular_deque<base::Value> input_values_;
};

}  // namespace

base::Value RunAttributionSimulation(
    base::Value input,
    const AttributionSimulationOptions& options,
    std::ostream& error_stream) {
  // Prerequisites for using an environment with mock time.
  content::BrowserTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestBrowserContext browser_context;
  const base::Time time_origin = base::Time::Now();

  absl::optional<AttributionSimulationEventAndValues> events =
      ParseAttributionSimulationInput(std::move(input), base::Time::Now(),
                                      error_stream);
  if (!events)
    return base::Value();

  if (events->empty())
    return base::Value(base::Value::Dict());

  base::ranges::stable_sort(*events, /*comp=*/{}, &GetEventTime);
  task_environment.FastForwardBy(GetEventTime(events->at(0)) - time_origin);

  std::unique_ptr<AttributionRandomGenerator> rng;
  if (options.noise_seed.has_value()) {
    rng = std::make_unique<AttributionInsecureRandomGenerator>(
        *options.noise_seed);
  } else {
    rng = std::make_unique<AttributionDefaultRandomGenerator>();
  }

  auto* storage_partition = static_cast<StoragePartitionImpl*>(
      browser_context.GetDefaultStoragePartition());

  std::unique_ptr<AttributionCookieChecker> cookie_checker;
  if (options.skip_debug_cookie_checks) {
    cookie_checker = std::make_unique<AlwaysSetCookieChecker>();
  } else {
    cookie_checker =
        std::make_unique<AttributionCookieCheckerImpl>(storage_partition);
  }

  auto manager = AttributionManagerImpl::CreateForTesting(
      // Avoid creating an on-disk sqlite DB.
      /*user_data_directory=*/base::FilePath(),
      /*max_pending_events=*/std::numeric_limits<size_t>::max(),
      /*special_storage_policy=*/nullptr,
      AttributionStorageDelegateImpl::CreateForTesting(
          options.noise_mode, options.delay_mode, options.config,
          std::move(rng)),
      std::move(cookie_checker), std::make_unique<FakeReportSender>(),
      storage_partition,
      base::ThreadPool::CreateUpdateableSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
           base::ThreadPolicy::MUST_USE_FOREGROUND}));

  AttributionEventHandler handler(
      manager.get(), storage_partition,
      AttributionReportJsonConverter(options.output_options, time_origin));

  static_cast<AggregationServiceImpl*>(
      storage_partition->GetAggregationService())
      ->SetPublicKeysForTesting(
          GURL(kPrivacySandboxAggregationServiceTrustedServerUrlAwsParam.Get()),
          PublicKeyset({aggregation_service::GenerateKey().public_key},
                       /*fetch_time=*/base::Time::Now(),
                       /*expiry_time=*/base::Time::Max()));

  base::Time last_event_time = GetEventTime(events->back());

  for (auto& event : *events) {
    base::Time event_time = GetEventTime(event);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AttributionEventHandler::Handle,
                       base::Unretained(&handler), std::move(event)),
        event_time - base::Time::Now());
  }

  task_environment.FastForwardBy(last_event_time - base::Time::Now());

  std::vector<AttributionReport> pending_reports =
      GetAttributionReportsForTesting(manager.get());

  if (!pending_reports.empty()) {
    base::Time last_report_time =
        base::ranges::max(pending_reports, /*comp=*/{},
                          &AttributionReport::report_time)
            .report_time();
    task_environment.FastForwardBy(last_report_time - base::Time::Now());
  }

  return base::Value(handler.TakeOutput());
}

}  // namespace content
