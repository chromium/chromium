// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_interop_runner.h"

#include <stddef.h>
#include <stdint.h>

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
#include "base/json/json_reader.h"
#include "base/memory/raw_ref.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task/updateable_sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "content/browser/aggregation_service/aggregation_service_features.h"
#include "content/browser/aggregation_service/aggregation_service_impl.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/attribution_reporting/aggregatable_attribution_utils.h"
#include "content/browser/attribution_reporting/attribution_cookie_checker.h"
#include "content/browser/attribution_reporting/attribution_debug_report.h"
#include "content/browser/attribution_reporting/attribution_interop_parser.h"
#include "content/browser/attribution_reporting/attribution_manager_impl.h"
#include "content/browser/attribution_reporting/attribution_observer.h"
#include "content/browser/attribution_reporting/attribution_os_level_manager.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_report_sender.h"
#include "content/browser/attribution_reporting/attribution_storage_delegate_impl.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/create_report_result.h"
#include "content/browser/attribution_reporting/send_result.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"

namespace content {

namespace {

struct AttributionReportJsonConverter {
  explicit AttributionReportJsonConverter(base::Time time_origin)
      : time_origin(time_origin) {}

  base::Value::Dict ToJson(const AttributionReport& report,
                           bool is_debug_report) const {
    base::Value::Dict report_body = report.ReportBody();

    absl::visit(
        base::Overloaded{
            [&](const AttributionReport::AggregatableAttributionData&
                    aggregatable_data) {
              // These fields normally encode a random GUID or the absolute time
              // and therefore are sources of nondeterminism in the output.

              // Output attribution_destination from the shared_info field.
              absl::optional<base::Value> shared_info =
                  report_body.Extract("shared_info");
              DCHECK(shared_info);
              std::string* shared_info_str = shared_info->GetIfString();
              DCHECK(shared_info_str);

              absl::optional<base::Value> shared_info_value =
                  base::JSONReader::Read(*shared_info_str,
                                         base::JSON_PARSE_RFC);
              DCHECK(shared_info_value && shared_info_value->is_dict());

              static constexpr char kKeyAttributionDestination[] =
                  "attribution_destination";
              std::string* attribution_destination =
                  shared_info_value->GetDict().FindString(
                      kKeyAttributionDestination);
              DCHECK(attribution_destination);
              DCHECK(!report_body.contains(kKeyAttributionDestination));
              report_body.Set(kKeyAttributionDestination,
                              std::move(*attribution_destination));

              report_body.Remove("aggregation_service_payloads");

              // The aggregation coordinator may be platform specific.
              report_body.Remove("aggregation_coordinator_origin");

              base::Value::List list;
              for (const auto& contribution : aggregatable_data.contributions) {
                base::Value::Dict dict;
                dict.Set("key", attribution_reporting::HexEncodeAggregationKey(
                                    contribution.key()));
                dict.Set("value",
                         base::checked_cast<int>(contribution.value()));

                list.Append(std::move(dict));
              }
              report_body.Set("histograms", std::move(list));
            },
            [&](const AttributionReport::EventLevelData&) {
              // Report IDs are a source of nondeterminism, so remove them.
              report_body.Remove("report_id");

              bool ok = AdjustScheduledReportTime(report_body,
                                                  report.initial_report_time());
              DCHECK(ok);
            },
            [](const AttributionReport::NullAggregatableData&) {
              NOTREACHED();
            },
        },
        report.data());

    base::Value::Dict value;
    value.Set("payload", std::move(report_body));
    value.Set("report_url", report.ReportURL(is_debug_report).spec());

    value.Set("report_time",
              FormatTime(is_debug_report ? report.attribution_info().time
                                         : report.report_time()));

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

      // Report IDs are a source of nondeterminism, so remove them.
      body->Remove("report_id");

      AdjustScheduledReportTime(*body,
                                report.GetOriginalReportTimeForTesting());
    }

    base::Value::Dict value;
    value.Set("payload", std::move(report_body));
    value.Set("report_url", report.ReportUrl().spec());
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

class FakeCookieChecker : public AttributionCookieChecker {
 public:
  FakeCookieChecker() = default;

  ~FakeCookieChecker() override = default;

  FakeCookieChecker(const FakeCookieChecker&) = delete;
  FakeCookieChecker(FakeCookieChecker&&) = delete;

  FakeCookieChecker& operator=(const FakeCookieChecker&) = delete;
  FakeCookieChecker& operator=(FakeCookieChecker&&) = delete;

  void set_debug_cookie_set(bool set) { debug_cookie_set_ = set; }

 private:
  // AttributionCookieChecker:
  void IsDebugCookieSet(const url::Origin& origin,
                        base::OnceCallback<void(bool)> callback) override {
    std::move(callback).Run(debug_cookie_set_);
  }

  bool debug_cookie_set_ = false;
};

// Registers sources and triggers in the `AttributionManagerImpl` and records
// sent reports.
class AttributionEventHandler : public AttributionObserver {
 public:
  AttributionEventHandler(std::unique_ptr<AttributionManagerImpl> manager,
                          FakeCookieChecker* fake_cookie_checker,
                          AttributionReportJsonConverter json_converter)
      : manager_(std::move(manager)),
        fake_cookie_checker_(
            raw_ref<FakeCookieChecker>::from_ptr(fake_cookie_checker)),
        json_converter_(json_converter) {
    DCHECK(manager_);

    manager_->AddObserver(this);
  }

  ~AttributionEventHandler() override { manager_->RemoveObserver(this); }

  void Handle(AttributionSimulationEvent event) {
    fake_cookie_checker_->set_debug_cookie_set(event.debug_permission);

    base::Value::Dict* dict = event.registration.GetIfDict();
    if (!dict) {
      AddUnparsableRegistration(event);
      return;
    }

    if (event.source_type.has_value()) {
      auto registration =
          attribution_reporting::SourceRegistration::Parse(std::move(*dict));
      if (!registration.has_value()) {
        AddUnparsableRegistration(event);
        return;
      }

      manager_->HandleSource(
          StorableSource(std::move(event.reporting_origin),
                         std::move(*registration),
                         std::move(event.context_origin), *event.source_type,
                         /*is_within_fenced_frame=*/false),
          GlobalRenderFrameHostId());
      return;
    }

    auto registration =
        attribution_reporting::TriggerRegistration::Parse(std::move(*dict));
    if (!registration.has_value()) {
      AddUnparsableRegistration(event);
      return;
    }

    manager_->HandleTrigger(
        AttributionTrigger(std::move(event.reporting_origin),
                           std::move(*registration),
                           std::move(event.context_origin),
                           /*verifications=*/{},
                           /*is_within_fenced_frame=*/false),
        GlobalRenderFrameHostId());
  }

  base::Value::Dict TakeOutput() {
    base::Value::Dict output;

    if (!event_level_reports_.empty()) {
      output.Set(kEventLevelResultsKey,
                 std::exchange(event_level_reports_, {}));
    }

    if (!debug_event_level_reports_.empty()) {
      output.Set(kDebugEventLevelResultsKey,
                 std::exchange(debug_event_level_reports_, {}));
    }

    if (!aggregatable_reports_.empty()) {
      output.Set(kAggregatableResultsKey,
                 std::exchange(aggregatable_reports_, {}));
    }

    if (!debug_aggregatable_reports_.empty()) {
      output.Set(kDebugAggregatableResultsKey,
                 std::exchange(debug_aggregatable_reports_, {}));
    }

    if (!verbose_debug_reports_.empty()) {
      output.Set(kVerboseDebugReportsKey,
                 std::exchange(verbose_debug_reports_, {}));
    }

    if (!unparsable_.empty()) {
      output.Set(kUnparsableRegistrationsKey, std::exchange(unparsable_, {}));
    }

    return output;
  }

  base::Time max_report_time() const { return max_report_time_; }

 private:
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
      case AttributionReport::Type::kNullAggregatable:
        // TODO(linnan): Consider supporting null reports in interop tests.
        return;
    }

    reports->Append(json_converter_.ToJson(report, is_debug_report));
  }

  void OnDebugReportSent(const AttributionDebugReport& report,
                         int status,
                         base::Time time) override {
    DCHECK_EQ(status, 200);
    verbose_debug_reports_.Append(json_converter_.ToJson(report, time));
  }

  void OnTriggerHandled(const AttributionTrigger&,
                        absl::optional<uint64_t> cleared_debug_key,
                        const CreateReportResult& result) override {
    if (const auto& report = result.new_event_level_report()) {
      max_report_time_ = std::max(max_report_time_, report->report_time());
    }

    if (const auto& report = result.new_aggregatable_report()) {
      max_report_time_ = std::max(max_report_time_, report->report_time());
    }
  }

  void AddUnparsableRegistration(const AttributionSimulationEvent& event) {
    base::Value::Dict dict;
    dict.Set("time", json_converter_.FormatTime(event.time));
    dict.Set("type", event.source_type.has_value() ? "source" : "trigger");
    unparsable_.Append(std::move(dict));
  }

  const std::unique_ptr<AttributionManagerImpl> manager_;
  const raw_ref<FakeCookieChecker> fake_cookie_checker_;

  const AttributionReportJsonConverter json_converter_;

  base::Time max_report_time_;

  base::Value::List event_level_reports_;
  base::Value::List debug_event_level_reports_;
  base::Value::List aggregatable_reports_;
  base::Value::List debug_aggregatable_reports_;
  base::Value::List verbose_debug_reports_;
  base::Value::List unparsable_;
};

}  // namespace

base::expected<base::Value::Dict, std::string> RunAttributionInteropSimulation(
    base::Value::Dict input,
    const AttributionConfig& config) {
  // Prerequisites for using an environment with mock time.
  content::BrowserTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestBrowserContext browser_context;
  const base::Time time_origin = base::Time::Now();

  ASSIGN_OR_RETURN(AttributionSimulationEvents events,
                   ParseAttributionInteropInput(std::move(input), time_origin));

  if (events.empty()) {
    return base::Value::Dict();
  }

  DCHECK(base::ranges::is_sorted(events));
  DCHECK(base::ranges::adjacent_find(
             events, /*pred=*/{},
             [](const auto& event) { return event.time; }) == events.end());

  const base::Time min_event_time = events.front().time;
  const base::Time max_event_time = events.back().time;

  task_environment.FastForwardBy(min_event_time - time_origin);

  auto* storage_partition = static_cast<StoragePartitionImpl*>(
      browser_context.GetDefaultStoragePartition());

  auto fake_cookie_checker = std::make_unique<FakeCookieChecker>();
  auto* raw_fake_cookie_checker = fake_cookie_checker.get();

  auto manager = AttributionManagerImpl::CreateForTesting(
      // Avoid creating an on-disk sqlite DB.
      /*user_data_directory=*/base::FilePath(),
      /*max_pending_events=*/std::numeric_limits<size_t>::max(),
      /*special_storage_policy=*/nullptr,
      AttributionStorageDelegateImpl::CreateForTesting(
          AttributionNoiseMode::kNone, AttributionDelayMode::kDefault, config),
      std::move(fake_cookie_checker), std::make_unique<FakeReportSender>(),
      std::make_unique<NoOpAttributionOsLevelManager>(), storage_partition,
      base::ThreadPool::CreateUpdateableSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
           base::ThreadPolicy::MUST_USE_FOREGROUND}));

  AttributionEventHandler handler(std::move(manager), raw_fake_cookie_checker,
                                  AttributionReportJsonConverter(time_origin));

  static_cast<AggregationServiceImpl*>(
      storage_partition->GetAggregationService())
      ->SetPublicKeysForTesting(
          GURL(kPrivacySandboxAggregationServiceTrustedServerUrlAwsParam.Get()),
          PublicKeyset({aggregation_service::GenerateKey().public_key},
                       /*fetch_time=*/base::Time::Now(),
                       /*expiry_time=*/base::Time::Max()));

  for (auto& event : events) {
    base::Time event_time = event.time;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AttributionEventHandler::Handle,
                       base::Unretained(&handler), std::move(event)),
        event_time - base::Time::Now());
  }

  task_environment.FastForwardBy(max_event_time - base::Time::Now());

  if (base::Time max_report_time = handler.max_report_time(),
      now = base::Time::Now();
      max_report_time >= now) {
    task_environment.FastForwardBy(max_report_time - now);
  }

  return handler.TakeOutput();
}

}  // namespace content
