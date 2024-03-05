// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_interop_runner.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/abseil_string_number_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
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
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service_features.h"
#include "content/browser/aggregation_service/aggregation_service_impl.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/attribution_reporting/aggregatable_attribution_utils.h"
#include "content/browser/attribution_reporting/attribution_background_registrations_id.h"
#include "content/browser/attribution_reporting/attribution_constants.h"
#include "content/browser/attribution_reporting/attribution_cookie_checker.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager.h"
#include "content/browser/attribution_reporting/attribution_interop_parser.h"
#include "content/browser/attribution_reporting/attribution_manager_impl.h"
#include "content/browser/attribution_reporting/attribution_os_level_manager.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_report_network_sender.h"
#include "content/browser/attribution_reporting/attribution_reporting.mojom.h"
#include "content/browser/attribution_reporting/attribution_storage_delegate_impl.h"
#include "content/browser/attribution_reporting/attribution_suitable_context.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_version.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/attribution_reporting_runtime_features.h"
#include "services/network/public/cpp/trigger_verification.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using ::attribution_reporting::mojom::RegistrationEligibility;
using ::attribution_reporting::mojom::RegistrationType;

constexpr int64_t kNavigationId(-1);

const GlobalRenderFrameHostId kFrameId = {0, 1};

const aggregation_service::TestHpkeKey kHpkeKey;

base::TimeDelta TimeOffset(base::Time time_origin) {
  return time_origin - base::Time::UnixEpoch();
}

base::Value::List GetDecryptedPayloads(std::optional<base::Value> payloads,
                                       const std::string& shared_info) {
  CHECK(payloads.has_value());

  base::Value::List payloads_list = std::move(*payloads).TakeList();
  CHECK_EQ(payloads_list.size(), 1u);

  base::Value::Dict& payload_dict = payloads_list.front().GetDict();

  std::optional<base::Value> payload = payload_dict.Extract("payload");
  CHECK(payload.has_value());

  std::optional<std::vector<uint8_t>> encrypted_payload =
      base::Base64Decode(payload->GetString());
  CHECK(encrypted_payload.has_value());

  std::vector<uint8_t> decrypted_payload =
      aggregation_service::DecryptPayloadWithHpke(
          *encrypted_payload, kHpkeKey.full_hpke_key(), shared_info);
  std::optional<cbor::Value> deserialized_payload =
      cbor::Reader::Read(decrypted_payload);
  CHECK(deserialized_payload.has_value());
  const cbor::Value::MapValue& payload_map = deserialized_payload->GetMap();
  const auto it = payload_map.find(cbor::Value("data"));
  CHECK(it != payload_map.end());

  base::Value::List list;

  for (const cbor::Value& data : it->second.GetArray()) {
    const cbor::Value::MapValue& data_map = data.GetMap();

    const cbor::Value::BinaryValue& bucket_byte_string =
        data_map.at(cbor::Value("bucket")).GetBytestring();

    absl::uint128 bucket;
    CHECK(
        base::HexStringToUInt128(base::HexEncode(bucket_byte_string), &bucket));

    const cbor::Value::BinaryValue& value_byte_string =
        data_map.at(cbor::Value("value")).GetBytestring();

    uint32_t value;
    CHECK(base::HexStringToUInt(base::HexEncode(value_byte_string), &value));

    // Ignore the paddings.
    if (bucket == 0 && value == 0) {
      continue;
    }

    list.Append(
        base::Value::Dict()
            .Set("key", attribution_reporting::HexEncodeAggregationKey(bucket))
            .Set("value", base::checked_cast<int>(value)));
  }
  return list;
}

void AdjustEventLevelBody(base::Value::Dict& report_body,
                          const base::Time time_origin) {
  // Report IDs are a source of nondeterminism, so remove them.
  report_body.Remove("report_id");

  // This field contains a string encoding seconds from the UNIX epoch. It
  // needs to be adjusted relative to the simulator's origin time in order
  // for test output to be consistent.
  if (std::string* str = report_body.FindString("scheduled_report_time")) {
    if (int64_t seconds; base::StringToInt64(*str, &seconds)) {
      *str =
          base::NumberToString(seconds - TimeOffset(time_origin).InSeconds());
    }
  }
}

AttributionInteropOutput::Report MakeReport(const network::ResourceRequest& req,
                                            const base::Time time_origin) {
  std::optional<base::Value> value =
      base::JSONReader::Read(network::GetUploadData(req), base::JSON_PARSE_RFC);
  CHECK(value.has_value());

  if (base::EndsWith(req.url.path_piece(), "/report-aggregate-attribution")) {
    base::Value::Dict& report_body = value->GetDict();

    // These fields normally encode a random GUID or the absolute time
    // and therefore are sources of nondeterminism in the output.

    // Output attribution_destination from the shared_info field.
    std::optional<base::Value> shared_info = report_body.Extract("shared_info");
    CHECK(shared_info.has_value());
    std::string shared_info_str = shared_info->GetString();

    std::optional<base::Value> shared_info_value =
        base::JSONReader::Read(shared_info_str, base::JSON_PARSE_RFC);
    CHECK(shared_info_value.has_value());
    static constexpr char kKeyAttributionDestination[] =
        "attribution_destination";
    std::optional<base::Value> attribution_destination =
        shared_info_value->GetDict().Extract(kKeyAttributionDestination);
    CHECK(attribution_destination.has_value());
    report_body.Set(kKeyAttributionDestination,
                    std::move(*attribution_destination));

    // The aggregation coordinator may be platform specific.
    report_body.Remove("aggregation_coordinator_origin");

    report_body.Set("histograms",
                    GetDecryptedPayloads(
                        report_body.Extract("aggregation_service_payloads"),
                        shared_info_str));
  } else if (base::EndsWith(req.url.path_piece(),
                            "/report-event-attribution")) {
    AdjustEventLevelBody(value->GetDict(), time_origin);
  } else if (req.url.path_piece() ==
             "/.well-known/attribution-reporting/debug/verbose") {
    for (auto& item : value->GetList()) {
      if (base::Value::Dict* dict = item.GetIfDict()) {
        if (base::Value::Dict* body = dict->FindDict("body")) {
          AdjustEventLevelBody(*body, time_origin);
        }
      }
    }
  }

  return AttributionInteropOutput::Report(
      base::Time::Now() - TimeOffset(time_origin), req.url, std::move(*value));
}

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

    const BackgroundRegistrationsId id(unique_id_counter_++);

    std::string registration_str;
    if (std::string* str = event.registration.GetIfString()) {
      registration_str = std::move(*str);
    } else {
      CHECK(base::JSONWriter::Write(event.registration, &registration_str));
    }

    auto* attribution_data_host_manager = manager_->GetDataHostManager();

    const bool is_source = event.source_type.has_value();
    std::optional<blink::AttributionSrcToken> attribution_src_token;

    if (is_source) {
      if (auto registration = attribution_reporting::SourceRegistration::Parse(
              registration_str, *event.source_type);
          !registration.has_value()) {
        AddUnparsableRegistration(event);
      }

      if (event.source_type ==
          attribution_reporting::mojom::SourceType::kNavigation) {
        attribution_src_token.emplace();
        attribution_data_host_manager
            ->NotifyNavigationWithBackgroundRegistrationsWillStart(
                attribution_src_token.value(),
                /*background_registrations_count=*/1);
        attribution_data_host_manager->NotifyNavigationRegistrationStarted(
            AttributionSuitableContext::CreateForTesting(
                event.context_origin,
                /*is_nested_within_fenced_frame=*/false, kFrameId,
                /*last_navigation_id=*/kNavigationId),
            attribution_src_token.value(), kNavigationId,
            /*devtools_request_id=*/"");
        attribution_data_host_manager->NotifyNavigationRegistrationCompleted(
            attribution_src_token.value());
      }
    } else if (auto registration =
                   attribution_reporting::TriggerRegistration::Parse(
                       registration_str);
               !registration.has_value()) {
      AddUnparsableRegistration(event);
    }

    attribution_data_host_manager->NotifyBackgroundRegistrationStarted(
        id,
        AttributionSuitableContext::CreateForTesting(
            event.context_origin,
            /*is_nested_within_fenced_frame=*/false, kFrameId, kNavigationId),
        is_source ? RegistrationEligibility::kSource
                  : RegistrationEligibility::kTrigger,
        attribution_src_token,
        /*devtools_request_id=*/"");

    scoped_refptr<net::HttpResponseHeaders> headers =
        net::HttpResponseHeaders::Builder(/*version=*/{1, 1},
                                          /*status=*/"200 OK")
            .AddHeader(is_source ? kAttributionReportingRegisterSourceHeader
                                 : kAttributionReportingRegisterTriggerHeader,
                       registration_str)
            .AddHeader(kAttributionReportingInfoHeader, event.info_header)
            .Build();
    attribution_data_host_manager->NotifyBackgroundRegistrationData(
        id, headers.get(), event.reporting_origin->GetURL(),
        network::AttributionReportingRuntimeFeatures(),
        /*trigger_verification=*/{});
    attribution_data_host_manager->NotifyBackgroundRegistrationCompleted(id);
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
  // TODO(linnan): Consider removing `unparsable_registrations`.
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

  int64_t unique_id_counter_ = 0;

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

  // Ensure that `time_origin` has a whole number of seconds to make
  // `AdjustEventLevelBody()` time calculations robust against
  // sub-second-precision report times, which otherwise cannot be recovered
  // because the `scheduled_report_time` field has second precision.
  {
    const base::Time with_millis = base::Time::Now();

    base::Time::Exploded exploded;
    with_millis.UTCExplode(&exploded);
    DCHECK(exploded.HasValidValues());
    exploded.millisecond = 0;

    base::Time without_millis;
    bool ok = base::Time::FromUTCExploded(exploded, &without_millis);
    DCHECK(ok);

    task_environment.FastForwardBy((without_millis + base::Seconds(1)) -
                                   with_millis);
  }

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

  network::TestURLLoaderFactory test_url_loader_factory;
  test_url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& req) {
        output.reports.emplace_back(MakeReport(req, time_origin));
        test_url_loader_factory.AddResponse(req.url.spec(), /*content=*/"");
      }));

  // Speed-up parsing in `AttributionDataHostManagerImpl`.
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;

  auto manager = AttributionManagerImpl::CreateForTesting(
      // Avoid creating an on-disk sqlite DB.
      /*user_data_directory=*/base::FilePath(),
      /*max_pending_events=*/std::numeric_limits<size_t>::max(),
      /*special_storage_policy=*/nullptr,
      AttributionStorageDelegateImpl::CreateForTesting(
          AttributionNoiseMode::kNone, AttributionDelayMode::kDefault, config),
      std::move(fake_cookie_checker),
      std::make_unique<AttributionReportNetworkSender>(
          test_url_loader_factory.GetSafeWeakWrapper()),
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
          PublicKeyset({kHpkeKey.GetPublicKey()},
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
