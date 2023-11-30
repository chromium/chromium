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
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/aggregation_service/features.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "components/attribution_reporting/registration_eligibility.mojom.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service_features.h"
#include "content/browser/aggregation_service/aggregation_service_impl.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/attribution_reporting/aggregatable_attribution_utils.h"
#include "content/browser/attribution_reporting/attribution_cookie_checker.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager.h"
#include "content/browser/attribution_reporting/attribution_debug_report.h"
#include "content/browser/attribution_reporting/attribution_input_event.h"
#include "content/browser/attribution_reporting/attribution_interop_parser.h"
#include "content/browser/attribution_reporting/attribution_manager_impl.h"
#include "content/browser/attribution_reporting/attribution_os_level_manager.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_report_sender.h"
#include "content/browser/attribution_reporting/attribution_reporting.mojom.h"
#include "content/browser/attribution_reporting/attribution_storage_delegate_impl.h"
#include "content/browser/attribution_reporting/send_result.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using ::attribution_reporting::mojom::RegistrationType;

constexpr int64_t kNavigationId(-1);

base::TimeDelta TimeOffset(base::Time time_origin) {
  return time_origin - base::Time::UnixEpoch();
}

class AttributionReportConverter {
 public:
  explicit AttributionReportConverter(base::Time time_origin)
      : time_origin_(time_origin) {}

  AttributionInteropOutput::Report ToOutput(const AttributionReport& report,
                                            bool is_debug_report) const {
    base::Value::Dict report_body = report.ReportBody();

    absl::visit(
        base::Overloaded{
            [&](const AttributionReport::AggregatableAttributionData&
                    aggregatable_data) {
              // These fields normally encode a random GUID or the absolute time
              // and therefore are sources of nondeterminism in the output.

              // Output attribution_destination from the shared_info field.
              if (absl::optional<base::Value> shared_info =
                      report_body.Extract("shared_info");
                  shared_info.has_value() && shared_info->is_string()) {
                if (absl::optional<base::Value> shared_info_value =
                        base::JSONReader::Read(shared_info->GetString(),
                                               base::JSON_PARSE_RFC)) {
                  static constexpr char kKeyAttributionDestination[] =
                      "attribution_destination";
                  if (absl::optional<base::Value> attribution_destination =
                          shared_info_value->GetDict().Extract(
                              kKeyAttributionDestination)) {
                    report_body.Set(kKeyAttributionDestination,
                                    std::move(*attribution_destination));
                  }
                }
              }

              report_body.Remove("aggregation_service_payloads");

              // The aggregation coordinator may be platform specific.
              report_body.Remove("aggregation_coordinator_origin");

              base::Value::List list;
              for (const auto& contribution : aggregatable_data.contributions) {
                list.Append(
                    base::Value::Dict()
                        .Set("key",
                             attribution_reporting::HexEncodeAggregationKey(
                                 contribution.key()))
                        .Set("value",
                             base::checked_cast<int>(contribution.value())));
              }
              report_body.Set("histograms", std::move(list));
            },
            [&](const AttributionReport::EventLevelData&) {
              // Report IDs are a source of nondeterminism, so remove
              // them.
              report_body.Remove("report_id");

              AdjustScheduledReportTime(report_body,
                                        report.initial_report_time());
            },
            [](const AttributionReport::NullAggregatableData&) {
              NOTREACHED_NORETURN();
            },
        },
        report.data());

    return MakeReport(base::Value(std::move(report_body)),
                      report.ReportURL(is_debug_report));
  }

  AttributionInteropOutput::Report ToOutput(
      const AttributionDebugReport& report) const {
    base::Value::List report_body = report.ReportBody().Clone();
    for (auto& value : report_body) {
      if (base::Value::Dict* dict = value.GetIfDict()) {
        if (base::Value::Dict* body = dict->FindDict("body")) {
          // Report IDs are a source of nondeterminism, so remove them.
          body->Remove("report_id");

          AdjustScheduledReportTime(*body,
                                    report.GetOriginalReportTimeForTesting());
        }
      }
    }

    return MakeReport(base::Value(std::move(report_body)), report.ReportUrl());
  }

 private:
  void AdjustScheduledReportTime(base::Value::Dict& report_body,
                                 base::Time original_report_time) const {
    // This field contains a string encoding seconds from the UNIX epoch. It
    // needs to be adjusted relative to the simulator's origin time in order
    // for test output to be consistent.
    std::string* str = report_body.FindString("scheduled_report_time");
    if (str) {
      *str = base::NumberToString(
          (original_report_time - time_origin_).InSeconds());
    }
  }

  AttributionInteropOutput::Report MakeReport(base::Value payload,
                                              const GURL& report_url) const {
    return AttributionInteropOutput::Report(
        base::Time::Now() - TimeOffset(time_origin_), report_url,
        std::move(payload));
  }

  const base::Time time_origin_;
};

class FakeReportSender : public AttributionReportSender {
 public:
  FakeReportSender(std::vector<AttributionInteropOutput::Report>* reports,
                   base::Time time_origin)
      : reports_(
            raw_ref<std::vector<AttributionInteropOutput::Report>>::from_ptr(
                reports)),
        converter_(time_origin) {}

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
    reports_->emplace_back(converter_.ToOutput(report, is_debug_report));

    std::move(sent_callback)
        .Run(std::move(report), SendResult(SendResult::Status::kSent,
                                           /*http_response_code=*/200));
  }

  void SendReport(AttributionDebugReport report,
                  DebugReportSentCallback done) override {
    reports_->emplace_back(converter_.ToOutput(report));
    std::move(done).Run(std::move(report), /*status=*/200);
  }

  raw_ref<std::vector<AttributionInteropOutput::Report>> reports_;
  const AttributionReportConverter converter_;
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
// unparsable registrations.
class AttributionEventHandler {
 public:
  AttributionEventHandler(std::unique_ptr<AttributionManagerImpl> manager,
                          FakeCookieChecker* fake_cookie_checker,
                          base::Time time_origin)
      : manager_(std::move(manager)),
        fake_cookie_checker_(
            raw_ref<FakeCookieChecker>::from_ptr(fake_cookie_checker)),
        time_offset_(TimeOffset(time_origin)) {
    DCHECK(manager_);
  }

  void Handle(AttributionSimulationEvent event) {
    fake_cookie_checker_->set_debug_cookie_set(event.debug_permission);

    base::Value::Dict* dict = event.registration.GetIfDict();
    if (!dict) {
      AddUnparsableRegistration(event);
      return;
    }

    if (event.source_type.has_value()) {
      auto registration = attribution_reporting::SourceRegistration::Parse(
          std::move(*dict), *event.source_type);
      if (!registration.has_value()) {
        AddUnparsableRegistration(event);
        return;
      }

      auto* attribution_data_host_manager = manager_->GetDataHostManager();

      mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;

      switch (*event.source_type) {
        case attribution_reporting::mojom::SourceType::kNavigation: {
          const blink::AttributionSrcToken attribution_src_token;
          attribution_data_host_manager->RegisterNavigationDataHost(
              data_host_remote.BindNewPipeAndPassReceiver(),
              attribution_src_token, /*expected_registrations=*/1);
          attribution_data_host_manager->NotifyNavigationRegistrationStarted(
              attribution_src_token, AttributionInputEvent(),
              event.context_origin,
              /*is_within_fenced_frame=*/false, GlobalRenderFrameHostId(),
              /*navigation_id=*/kNavigationId, /*devtools_request_id=*/"");
          attribution_data_host_manager->NotifyNavigationRegistrationCompleted(
              attribution_src_token);
          break;
        }
        case attribution_reporting::mojom::SourceType::kEvent:
          attribution_data_host_manager->RegisterDataHost(
              data_host_remote.BindNewPipeAndPassReceiver(),
              std::move(event.context_origin),
              /*is_within_fenced_frame=*/false,
              attribution_reporting::mojom::RegistrationEligibility::
                  kSourceOrTrigger,
              GlobalRenderFrameHostId(),
              /*last_navigation_id=*/kNavigationId);
          break;
      }

      data_host_remote->SourceDataAvailable(std::move(event.reporting_origin),
                                            std::move(*registration));
      data_host_remote.FlushForTesting();  // IN-TEST
      return;
    }

    auto registration =
        attribution_reporting::TriggerRegistration::Parse(std::move(*dict));
    if (!registration.has_value()) {
      AddUnparsableRegistration(event);
      return;
    }

    mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;

    manager_->GetDataHostManager()->RegisterDataHost(
        data_host_remote.BindNewPipeAndPassReceiver(),
        std::move(event.context_origin),
        /*is_within_fenced_frame=*/false,
        attribution_reporting::mojom::RegistrationEligibility::kSourceOrTrigger,
        GlobalRenderFrameHostId(),
        /*last_navigation_id=*/kNavigationId);
    data_host_remote->TriggerDataAvailable(std::move(event.reporting_origin),
                                           std::move(*registration),
                                           /*verifications=*/{});
    data_host_remote.FlushForTesting();  // IN-TEST
  }

  std::vector<AttributionInteropOutput::UnparsableRegistration>
  TakeUnparsable() && {
    return std::move(unparsable_);
  }

  void FastForwardUntilReportsConsumed(
      BrowserTaskEnvironment& task_environment) {
    while (true) {
      auto delta = base::TimeDelta::Min();
      base::RunLoop run_loop;

      manager_->GetPendingReportsForInternalUse(
          /*limit=*/-1, base::BindLambdaForTesting(
                            [&](std::vector<AttributionReport> reports) {
                              auto it = base::ranges::max_element(
                                  reports, /*comp=*/{},
                                  &AttributionReport::report_time);
                              if (it != reports.end()) {
                                delta = it->report_time() - base::Time::Now();
                              }
                              run_loop.Quit();
                            }));

      run_loop.Run();

      if (delta.is_negative()) {
        break;
      }
      task_environment.FastForwardBy(delta);
    }
  }

 private:
  void AddUnparsableRegistration(const AttributionSimulationEvent& event) {
    auto& registration = unparsable_.emplace_back();
    registration.time = event.time - time_offset_;
    registration.type = event.source_type.has_value()
                            ? RegistrationType::kSource
                            : RegistrationType::kTrigger;
  }

  const std::unique_ptr<AttributionManagerImpl> manager_;
  const raw_ref<FakeCookieChecker> fake_cookie_checker_;

  const base::TimeDelta time_offset_;

  std::vector<AttributionInteropOutput::UnparsableRegistration> unparsable_;
};

}  // namespace

base::expected<AttributionInteropOutput, std::string>
RunAttributionInteropSimulation(base::Value::Dict input,
                                const AttributionConfig& config) {
  // Prerequisites for using an environment with mock time.
  BrowserTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestBrowserContext browser_context;
  const base::Time time_origin = base::Time::Now();

  ASSIGN_OR_RETURN(AttributionSimulationEvents events,
                   ParseAttributionInteropInput(std::move(input), time_origin));

  if (events.empty()) {
    return AttributionInteropOutput();
  }

  DCHECK(base::ranges::is_sorted(events, /*comp=*/{},
                                 &AttributionSimulationEvent::time));
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

  AttributionInteropOutput output;

  auto manager = AttributionManagerImpl::CreateForTesting(
      // Avoid creating an on-disk sqlite DB.
      /*user_data_directory=*/base::FilePath(),
      /*max_pending_events=*/std::numeric_limits<size_t>::max(),
      /*special_storage_policy=*/nullptr,
      AttributionStorageDelegateImpl::CreateForTesting(
          AttributionNoiseMode::kNone, AttributionDelayMode::kDefault, config),
      std::move(fake_cookie_checker),
      std::make_unique<FakeReportSender>(&output.reports, time_origin),
      std::make_unique<NoOpAttributionOsLevelManager>(), storage_partition,
      base::ThreadPool::CreateUpdateableSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
           base::ThreadPolicy::MUST_USE_FOREGROUND}));

  AttributionEventHandler handler(std::move(manager), raw_fake_cookie_checker,
                                  time_origin);

  static_cast<AggregationServiceImpl*>(
      storage_partition->GetAggregationService())
      ->SetPublicKeysForTesting(
          GetAggregationServiceProcessingUrl(url::Origin::Create(
              GURL(::aggregation_service::kAggregationServiceCoordinatorAwsCloud
                       .Get()))),
          PublicKeyset({aggregation_service::TestHpkeKey().GetPublicKey()},
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

  handler.FastForwardUntilReportsConsumed(task_environment);

  output.unparsable_registrations = std::move(handler).TakeUnparsable();
  return output;
}

}  // namespace content
