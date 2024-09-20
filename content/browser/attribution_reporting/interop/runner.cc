// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/interop/runner.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task/updateable_sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/aggregation_service/aggregation_coordinator_utils.h"
#include "components/attribution_reporting/attribution_scopes_data.h"
#include "components/attribution_reporting/eligibility.h"
#include "components/attribution_reporting/event_level_epsilon.h"
#include "components/attribution_reporting/features.h"
#include "components/attribution_reporting/privacy_math.h"
#include "components/attribution_reporting/registration_eligibility.mojom-forward.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"
#include "components/attribution_reporting/test_utils.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service_features.h"
#include "content/browser/aggregation_service/aggregation_service_impl.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/aggregation_service/public_key.h"
#include "content/browser/attribution_reporting/attribution_background_registrations_id.h"
#include "content/browser/attribution_reporting/attribution_cookie_checker.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager.h"
#include "content/browser/attribution_reporting/attribution_manager_impl.h"
#include "content/browser/attribution_reporting/attribution_os_level_manager.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_report_network_sender.h"
#include "content/browser/attribution_reporting/attribution_reporting.mojom.h"
#include "content/browser/attribution_reporting/attribution_resolver_delegate_impl.h"
#include "content/browser/attribution_reporting/attribution_suitable_context.h"
#include "content/browser/attribution_reporting/interop/parser.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/attribution.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using ::attribution_reporting::RandomizedResponse;
using ::attribution_reporting::mojom::RegistrationEligibility;
using ::network::mojom::AttributionReportingEligibility;

constexpr int64_t kNavigationId(-1);

const GlobalRenderFrameHostId kFrameId = {0, 1};

base::TimeDelta TimeOffset(base::Time time_origin) {
  return time_origin - base::Time::UnixEpoch();
}

class Adjuster : public ReportBodyAdjuster {
 public:
  Adjuster(base::Time time_origin,
           const aggregation_service::TestHpkeKey& hpke_key)
      : time_origin_(time_origin), hpke_key_(hpke_key) {}

  ~Adjuster() override = default;

 private:
  void AdjustEventLevel(base::Value::Dict& report_body) override {
    AdjustTime(report_body, "scheduled_report_time");
  }

  void AdjustAggregatable(base::Value::Dict& report_body) override {
    AdjustAggregatableReportSharedInfo(report_body);
  }

  void AdjustAggregatableDebug(base::Value::Dict& report_body) override {
    AdjustAggregatableReportSharedInfo(report_body);
  }

  void AdjustAggregatableReportSharedInfo(base::Value::Dict& report_body) {
    std::string* shared_info = report_body.FindString("shared_info");
    CHECK(shared_info);

    std::optional<base::Value::Dict> shared_info_dict =
        base::JSONReader::ReadDict(*shared_info, base::JSON_PARSE_RFC);
    CHECK(shared_info_dict);

    AdjustTime(*shared_info_dict, "scheduled_report_time");

    // When source registration time is excluded from the report, its value is
    // set to "0".
    AdjustTime(*shared_info_dict, "source_registration_time",
               /*skip_adjust_value=*/"0");

    std::string adjusted_shared_info;
    base::JSONWriter::Write(*shared_info_dict, &adjusted_shared_info);

    // The payloads were encrypted with the original shared info, therefore
    // need to be re-encrypted with the adjusted shared info.
    base::Value::List* payloads =
        report_body.FindList("aggregation_service_payloads");
    CHECK(payloads);

    for (base::Value& payload : *payloads) {
      std::string* payload_str = payload.GetDict().FindString("payload");
      CHECK(payload_str);

      std::optional<std::vector<uint8_t>> encrypted_payload =
          base::Base64Decode(*payload_str);
      CHECK(encrypted_payload.has_value());

      // Decrypt with the original shared info.
      std::vector<uint8_t> decrypted_payload =
          aggregation_service::DecryptPayloadWithHpke(
              *encrypted_payload, hpke_key_->full_hpke_key(), *shared_info);

      // Re-encrypt with the adjusted shared info.
      std::string authenticated_info_str = base::StrCat(
          {AggregatableReport::kDomainSeparationPrefix, adjusted_shared_info});
      *payload_str =
          base::Base64Encode(EncryptAggregatableReportPayloadWithHpke(
              decrypted_payload, hpke_key_->GetPublicKey().key,
              base::as_bytes(base::make_span(authenticated_info_str))));
    }

    *shared_info = std::move(adjusted_shared_info);
  }

  // Adjust the field that contains a string encoding seconds from the UNIX
  // epoch. It needs to be adjusted relative to the simulator's origin time in
  // order for test output to be consistent.
  void AdjustTime(base::Value::Dict& dict,
                  std::string_view key,
                  std::string_view skip_adjust_value = "") {
    if (std::string* str = dict.FindString(key);
        str && *str != skip_adjust_value) {
      if (int64_t seconds; base::StringToInt64(*str, &seconds)) {
        *str = base::NumberToString(seconds -
                                    TimeOffset(time_origin_).InSeconds());
      }
    }
  }

  const base::Time time_origin_;
  const base::raw_ref<const aggregation_service::TestHpkeKey> hpke_key_;
};

AttributionInteropOutput::Report MakeReport(
    const network::ResourceRequest& req,
    const base::Time time_origin,
    const aggregation_service::TestHpkeKey& hpke_key) {
  std::optional<base::Value> value =
      base::JSONReader::Read(network::GetUploadData(req), base::JSON_PARSE_RFC);
  CHECK(value.has_value());

  Adjuster adjuster(time_origin, hpke_key);
  MaybeAdjustReportBody(req.url, *value, adjuster);

  return AttributionInteropOutput::Report(
      base::Time::Now() - TimeOffset(time_origin), req.url, *std::move(value));
}

class FakeCookieChecker : public AttributionCookieChecker {
 public:
  explicit FakeCookieChecker(
      const std::vector<AttributionSimulationEvent>& events) {
    std::vector<base::Time> times;
    for (const auto& event : events) {
      if (const auto* data =
              absl::get_if<AttributionSimulationEvent::Response>(&event.data);
          data && data->debug_permission) {
        times.push_back(event.time);
      }
    }
    debug_cookie_set_.replace(std::move(times));
  }

  ~FakeCookieChecker() override = default;

  FakeCookieChecker(const FakeCookieChecker&) = delete;
  FakeCookieChecker(FakeCookieChecker&&) = delete;

  FakeCookieChecker& operator=(const FakeCookieChecker&) = delete;
  FakeCookieChecker& operator=(FakeCookieChecker&&) = delete;

 private:
  // AttributionCookieChecker:
  void IsDebugCookieSet(const url::Origin&, Callback callback) override {
    std::move(callback).Run(debug_cookie_set_.contains(base::Time::Now()));
  }

  base::flat_set<base::Time> debug_cookie_set_;
};

class ControllableStorageDelegate : public AttributionResolverDelegateImpl {
 public:
  explicit ControllableStorageDelegate(AttributionInteropRun& run)
      : AttributionResolverDelegateImpl(AttributionNoiseMode::kNone,
                                        AttributionDelayMode::kDefault,
                                        run.config.attribution_config) {
    std::vector<std::pair<base::Time, RandomizedResponse>> responses;
    std::vector<std::pair<base::Time, base::flat_set<int>>>
        null_aggregatable_reports_days;
    for (auto& event : run.events) {
      if (auto* data =
              absl::get_if<AttributionSimulationEvent::Response>(&event.data)) {
        if (data->randomized_response.has_value()) {
          responses.emplace_back(
              event.time,
              std::exchange(data->randomized_response, std::nullopt));
        }
        if (!data->null_aggregatable_reports_days.empty()) {
          null_aggregatable_reports_days.emplace_back(
              event.time,
              std::exchange(data->null_aggregatable_reports_days, {}));
        }
      }
    }
    randomized_responses_.replace(std::move(responses));
    null_aggregatable_reports_days_.replace(
        std::move(null_aggregatable_reports_days));
  }

  ~ControllableStorageDelegate() override = default;

  ControllableStorageDelegate(const ControllableStorageDelegate&) = delete;
  ControllableStorageDelegate& operator=(const ControllableStorageDelegate&) =
      delete;

  ControllableStorageDelegate(ControllableStorageDelegate&&) = delete;
  ControllableStorageDelegate& operator=(ControllableStorageDelegate&&) =
      delete;

 private:
  // AttributionResolverDelegateImpl:
  GetRandomizedResponseResult GetRandomizedResponse(
      const attribution_reporting::mojom::SourceType source_type,
      const attribution_reporting::TriggerSpecs& trigger_specs,
      const attribution_reporting::EventLevelEpsilon epsilon,
      const std::optional<attribution_reporting::AttributionScopesData>&
          scopes_data) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    ASSIGN_OR_RETURN(auto response_data,
                     AttributionResolverDelegateImpl::GetRandomizedResponse(
                         source_type, trigger_specs, epsilon, scopes_data));

    auto it = randomized_responses_.find(base::Time::Now());
    if (it == randomized_responses_.end()) {
      return response_data;
    }

    // Avoid crashing in `AttributionStorageSql::StoreSource()` by returning an
    // arbitrary error here, which will manifest as unexpected test output.
    if (!attribution_reporting::IsValid(it->second, trigger_specs)) {
      LOG(ERROR) << "invalid randomized response with trigger_specs="
                 << trigger_specs;
      return base::unexpected(attribution_reporting::RandomizedResponseError::
                                  kExceedsChannelCapacityLimit);
    }

    response_data.response() = std::exchange(it->second, std::nullopt);
    return response_data;
  }

  bool GenerateNullAggregatableReportForLookbackDay(
      int lookback_day,
      attribution_reporting::mojom::SourceRegistrationTimeConfig
          source_registration_time_config) const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    bool ret = AttributionResolverDelegateImpl::
        GenerateNullAggregatableReportForLookbackDay(
            lookback_day, source_registration_time_config);
    auto it = null_aggregatable_reports_days_.find(base::Time::Now());
    if (it != null_aggregatable_reports_days_.end()) {
      ret = it->second.contains(lookback_day);
    }
    return ret;
  }

  base::flat_map<base::Time, RandomizedResponse> randomized_responses_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::flat_map<base::Time, base::flat_set<int>>
      null_aggregatable_reports_days_ GUARDED_BY_CONTEXT(sequence_checker_);
};

void Handle(const AttributionSimulationEvent::StartRequest& event,
            AttributionDataHostManager& data_host_manager) {
  std::optional<RegistrationEligibility> eligibility =
      attribution_reporting::GetRegistrationEligibility(event.eligibility);
  if (!eligibility.has_value()) {
    return;
  }

  auto suitable_context = AttributionSuitableContext::CreateForTesting(
      event.context_origin, event.fenced, kFrameId,
      /*last_navigation_id=*/kNavigationId);

  std::optional<blink::AttributionSrcToken> attribution_src_token;
  if (event.eligibility == AttributionReportingEligibility::kNavigationSource) {
    attribution_src_token.emplace();
    data_host_manager.NotifyNavigationWithBackgroundRegistrationsWillStart(
        *attribution_src_token,
        /*background_registrations_count=*/1);
    data_host_manager.NotifyNavigationRegistrationStarted(
        suitable_context, *attribution_src_token, kNavigationId,
        /*devtools_request_id=*/"");
    data_host_manager.NotifyNavigationRegistrationCompleted(
        *attribution_src_token);
  }

  data_host_manager.NotifyBackgroundRegistrationStarted(
      BackgroundRegistrationsId(event.request_id), std::move(suitable_context),
      *eligibility, attribution_src_token,
      /*devtools_request_id=*/"");
}

void Handle(const AttributionSimulationEvent::Response& event,
            AttributionDataHostManager& data_host_manager) {
  data_host_manager.NotifyBackgroundRegistrationData(
      BackgroundRegistrationsId(event.request_id), event.response_headers.get(),
      event.url);
}

void Handle(const AttributionSimulationEvent::EndRequest& event,
            AttributionDataHostManager& data_host_manager) {
  data_host_manager.NotifyBackgroundRegistrationCompleted(
      BackgroundRegistrationsId(event.request_id));
}

void FastForwardUntilReportsConsumed(AttributionManager& manager,
                                     BrowserTaskEnvironment& task_environment) {
  while (true) {
    auto delta = base::TimeDelta::Min();
    base::RunLoop run_loop;

    manager.GetPendingReportsForInternalUse(
        /*limit=*/-1,
        base::BindLambdaForTesting([&](std::vector<AttributionReport> reports) {
          auto it = base::ranges::max_element(reports, /*comp=*/{},
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

}  // namespace

base::expected<AttributionInteropOutput, std::string>
RunAttributionInteropSimulation(
    AttributionInteropRun run,
    const aggregation_service::TestHpkeKey& hpke_key) {
  if (run.events.empty()) {
    return AttributionInteropOutput();
  }

  DCHECK(base::ranges::is_sorted(run.events, /*comp=*/{},
                                 &AttributionSimulationEvent::time));

  std::vector<base::test::FeatureRef> enabled_features(
      {blink::features::kKeepAliveInBrowserMigration,
       blink::features::kAttributionReportingInBrowserMigration});

  std::optional<AttributionOsLevelManager::ScopedApiStateForTesting>
      scoped_api_state;
  if (run.config.needs_cross_app_web) {
    enabled_features.emplace_back(
        network::features::kAttributionReportingCrossAppWeb);
    scoped_api_state.emplace(AttributionOsLevelManager::ApiState::kEnabled);
  }
  if (run.config.needs_aggregatable_debug) {
    enabled_features.emplace_back(attribution_reporting::features::
                                      kAttributionAggregatableDebugReporting);
  }

  if (run.config.needs_source_destination_limit) {
    enabled_features.emplace_back(
        attribution_reporting::features::kAttributionSourceDestinationLimit);
  }

  if (run.config.needs_aggregatable_filtering_ids) {
    enabled_features.emplace_back(
        attribution_reporting::features::
            kAttributionReportingAggregatableFilteringIds);
    enabled_features.emplace_back(
        kPrivacySandboxAggregationServiceFilteringIds);
  }

  if (run.config.needs_attribution_scopes) {
    enabled_features.emplace_back(
        attribution_reporting::features::kAttributionScopes);
  }

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      enabled_features,
      /*disabled_features=*/{
          // This UMA records a sample every 30s via a periodic task which
          // interacts poorly with TaskEnvironment::FastForward using day long
          // delays (we need to run the uma update every 30s for that
          // interval)
          network::features::kGetCookiesStringUma,
      });

  attribution_reporting::ScopedMaxEventLevelEpsilonForTesting
      scoped_max_event_level_epsilon(run.config.max_event_level_epsilon);

  attribution_reporting::ScopedMaxTriggerStateCardinalityForTesting
      scoped_max_trigger_state_cardinality(
          run.config.max_trigger_state_cardinality);

  // Prerequisites for using an environment with mock time.
  BrowserTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestBrowserContext browser_context;

  // Ensure that `time_origin` has a whole number of days to make
  // `AdjustEventLevelBody()` and `AdjustAggregatableReportSharedInfo()` time
  // calculations robust against sub-second-precision report times and rounding,
  // which otherwise cannot be recovered because the `scheduled_report_time`
  // field has second precision and `source_registration_time` is rounded to
  // whole day.
  {
    const base::Time non_whole_day = base::Time::Now();

    base::Time::Exploded exploded;
    non_whole_day.UTCExplode(&exploded);
    DCHECK(exploded.HasValidValues());
    exploded.millisecond = 0;
    exploded.second = 0;
    exploded.minute = 0;
    exploded.hour = 0;

    base::Time whole_day;
    bool ok = base::Time::FromUTCExploded(exploded, &whole_day);
    DCHECK(ok);

    task_environment.FastForwardBy((whole_day + base::Days(1)) - non_whole_day);
  }

  const base::Time time_origin = base::Time::Now();

  for (auto& event : run.events) {
    event.time = time_origin + (event.time - base::Time::UnixEpoch());
  }

  const base::Time min_event_time = run.events.front().time;

  task_environment.FastForwardBy(min_event_time - time_origin);

  auto* storage_partition = static_cast<StoragePartitionImpl*>(
      browser_context.GetDefaultStoragePartition());

  auto fake_cookie_checker = std::make_unique<FakeCookieChecker>(run.events);

  AttributionInteropOutput output;

  network::TestURLLoaderFactory test_url_loader_factory;
  test_url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& req) {
        output.reports.emplace_back(MakeReport(req, time_origin, hpke_key));
        test_url_loader_factory.AddResponse(req.url.spec(), /*content=*/"");
      }));

  // Speed-up parsing in `AttributionDataHostManagerImpl`.
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;

  auto storage_task_runner =
      base::ThreadPool::CreateUpdateableSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
           base::ThreadPolicy::MUST_USE_FOREGROUND});

  auto manager = AttributionManagerImpl::CreateForTesting(
      // Avoid creating an on-disk sqlite DB.
      /*user_data_directory=*/base::FilePath(),
      /*max_pending_events=*/std::numeric_limits<size_t>::max(),
      /*special_storage_policy=*/nullptr,
      std::make_unique<ControllableStorageDelegate>(run),
      std::move(fake_cookie_checker),
      std::make_unique<AttributionReportNetworkSender>(
          test_url_loader_factory.GetSafeWeakWrapper()),
      std::make_unique<NoOpAttributionOsLevelManager>(), storage_partition,
      storage_task_runner);

  for (const auto& origin : run.config.aggregation_coordinator_origins) {
    // TODO: Consider using a different public key for each origin.
    static_cast<AggregationServiceImpl*>(
        storage_partition->GetAggregationService())
        ->SetPublicKeysForTesting(  // IN-TEST
            GetAggregationServiceProcessingUrl(origin),
            PublicKeyset({hpke_key.GetPublicKey()},
                         /*fetch_time=*/base::Time::Now(),
                         /*expiry_time=*/base::Time::Max()));
  }

  ::aggregation_service::ScopedAggregationCoordinatorAllowlistForTesting
      scoped_aggregation_coordinators(
          std::move(run.config.aggregation_coordinator_origins));

  for (const auto& event : run.events) {
    task_environment.FastForwardBy(event.time - base::Time::Now());

    absl::visit(
        [&](const auto& data) { Handle(data, *manager->GetDataHostManager()); },
        event.data);
  }

  FastForwardUntilReportsConsumed(*manager, task_environment);

  return output;
}

void MaybeAdjustReportBody(const GURL& url,
                           base::Value& payload,
                           ReportBodyAdjuster& adjuster) {
  if (base::EndsWith(url.path_piece(), "/report-aggregate-attribution")) {
    if (base::Value::Dict* dict = payload.GetIfDict()) {
      adjuster.AdjustAggregatable(*dict);
    }
  } else if (base::EndsWith(url.path_piece(), "/report-event-attribution")) {
    if (base::Value::Dict* dict = payload.GetIfDict()) {
      adjuster.AdjustEventLevel(*dict);
    }
  } else if (url.path_piece() ==
             "/.well-known/attribution-reporting/debug/verbose") {
    if (base::Value::List* list = payload.GetIfList()) {
      for (auto& item : *list) {
        base::Value::Dict* dict = item.GetIfDict();
        if (!dict) {
          continue;
        }

        const std::string* debug_data_type = dict->FindString("type");
        base::Value::Dict* body = dict->FindDict("body");
        if (debug_data_type && body) {
          adjuster.AdjustVerboseDebug(*debug_data_type, *body);
        }
      }
    }
  } else if (url.path_piece() ==
             "/.well-known/attribution-reporting/debug/"
             "report-aggregate-debug") {
    if (base::Value::Dict* dict = payload.GetIfDict()) {
      adjuster.AdjustAggregatableDebug(*dict);
    }
  }
}

void ReportBodyAdjuster::AdjustVerboseDebug(std::string_view debug_data_type,
                                            base::Value::Dict& body) {
  if (debug_data_type == "trigger-event-excessive-reports" ||
      debug_data_type == "trigger-event-low-priority") {
    AdjustEventLevel(body);
  }
}

}  // namespace content
