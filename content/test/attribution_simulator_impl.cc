// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/attribution_simulator.h"

#include <stddef.h>

#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/overloaded.h"
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
#include "base/values.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "content/browser/aggregation_service/aggregation_service_features.h"
#include "content/browser/aggregation_service/aggregation_service_impl.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/attribution_reporting/aggregatable_attribution_utils.h"
#include "content/browser/attribution_reporting/attribution_cookie_checker_impl.h"
#include "content/browser/attribution_reporting/attribution_debug_report.h"
#include "content/browser/attribution_reporting/attribution_manager_impl.h"
#include "content/browser/attribution_reporting/attribution_observer.h"
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

base::Time GetEventTime(const AttributionSimulationEvent& event) {
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
      event);
}

struct AttributionReportJsonConverter {
  AttributionReportJsonConverter(AttributionSimulationOutputOptions options,
                                 base::Time time_origin)
      : options(options), time_origin(time_origin) {}

  base::Value::Dict ToJson(const AttributionReport& report,
                           bool is_debug_report) const {
    base::Value::Dict report_body = report.ReportBody();
    if (options.remove_report_ids)
      report_body.Remove("report_id");

    switch (report.GetReportType()) {
      case AttributionReport::Type::kAggregatableAttribution:
        if (options.remove_assembled_report) {
          // Output attribution_destination from the shared_info field.
          absl::optional<base::Value> shared_info =
              report_body.Extract("shared_info");
          DCHECK(shared_info);
          std::string* shared_info_str = shared_info->GetIfString();
          DCHECK(shared_info_str);

          base::Value shared_info_value =
              base::test::ParseJson(*shared_info_str);
          DCHECK(shared_info_value.is_dict());

          static constexpr char kKeyAttributionDestination[] =
              "attribution_destination";
          std::string* attribution_destination =
              shared_info_value.GetDict().FindString(
                  kKeyAttributionDestination);
          DCHECK(attribution_destination);
          DCHECK(!report_body.contains(kKeyAttributionDestination));
          report_body.Set(kKeyAttributionDestination,
                          std::move(*attribution_destination));

          report_body.Remove("aggregation_service_payloads");
          report_body.Remove("source_registration_time");
        }
        break;
      case AttributionReport::Type::kEventLevel:
        bool ok =
            AdjustScheduledReportTime(report_body, report.OriginalReportTime());
        DCHECK(ok);
        break;
    }

    base::Value::Dict value;
    value.Set("report", std::move(report_body));
    value.Set("report_url", report.ReportURL(is_debug_report).spec());

    value.Set("intended_report_time",
              FormatTime(is_debug_report ? report.attribution_info().time
                                         : report.report_time()));

    if (const auto* aggregatable_data =
            absl::get_if<AttributionReport::AggregatableAttributionData>(
                &report.data())) {
      base::Value::List list;
      for (const auto& contribution : aggregatable_data->contributions) {
        base::Value::Dict dict;
        dict.Set("key", attribution_reporting::HexEncodeAggregationKey(
                            contribution.key()));
        dict.Set("value", base::checked_cast<int>(contribution.value()));

        list.Append(std::move(dict));
      }
      base::Value::Dict test_info;
      test_info.Set("histograms", std::move(list));
      value.Set("test_info", std::move(test_info));
    }

    return value;
  }

  base::Value::Dict ToJson(const AttributionDebugReport& report,
                           base::Time time) const {
    base::Value::List report_body = report.ReportBody().Clone();
    for (auto& value : report_body) {
      base::Value::Dict* dict = value.GetIfDict();
      DCHECK(dict);

      base::Value::Dict* body = dict->FindDict("body");
      DCHECK(body);

      if (options.remove_report_ids) {
        body->Remove("report_id");
      }

      AdjustScheduledReportTime(*body,
                                report.GetOriginalReportTimeForTesting());
    }

    base::Value::Dict value;
    value.Set("report", std::move(report_body));
    value.Set("report_url", report.ReportURL().spec());
    value.Set("report_time", FormatTime(time));
    return value;
  }

  std::string FormatTime(base::Time time) const {
    base::TimeDelta time_delta = time - time_origin;
    return base::NumberToString(time_delta.InMilliseconds());
  }

  bool AdjustScheduledReportTime(base::Value::Dict& report_body,
                                 base::Time original_report_time) const {
    // This field contains a string encoding seconds from the UNIX epoch. It
    // needs to be adjusted relative to the simulator's origin time in order
    // for test output to be consistent.
    std::string* str = report_body.FindString("scheduled_report_time");
    if (!str) {
      return false;
    }

    *str =
        base::NumberToString((original_report_time - time_origin).InSeconds());
    return true;
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

  void Handle(AttributionSimulationEvent event) {
    absl::visit(*this, std::move(event));
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
                       /*filter_builder=*/nullptr, clear.delete_rate_limit_data,
                       base::DoNothing()));
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

  base::Value::List event_level_reports_;
  base::Value::List debug_event_level_reports_;
  base::Value::List aggregatable_reports_;
  base::Value::List debug_aggregatable_reports_;
  base::Value::List verbose_debug_reports_;
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

  absl::optional<AttributionSimulationEvents> events =
      ParseAttributionSimulationInput(std::move(input), base::Time::Now(),
                                      error_stream);
  if (!events)
    return base::Value();

  if (events->empty())
    return base::Value(base::Value::Dict());

  base::ranges::stable_sort(*events, /*comp=*/{}, &GetEventTime);
  task_environment.FastForwardBy(GetEventTime(events->at(0)) - time_origin);

  auto* storage_partition = static_cast<StoragePartitionImpl*>(
      browser_context.GetDefaultStoragePartition());

  auto manager = AttributionManagerImpl::CreateForTesting(
      // Avoid creating an on-disk sqlite DB.
      /*user_data_directory=*/base::FilePath(),
      /*max_pending_events=*/std::numeric_limits<size_t>::max(),
      /*special_storage_policy=*/nullptr,
      AttributionStorageDelegateImpl::CreateForTesting(
          options.noise_mode, options.delay_mode, options.config),
      std::make_unique<AttributionCookieCheckerImpl>(storage_partition),
      std::make_unique<FakeReportSender>(), storage_partition,
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
