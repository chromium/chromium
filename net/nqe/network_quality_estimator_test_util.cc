// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/nqe/network_quality_estimator_test_util.h"

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "net/base/load_flags.h"
#include "net/log/net_log.h"
#include "net/log/net_log_with_source.h"
#include "net/log/test_net_log_util.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"

namespace {

const base::FilePath::CharType kTestFilePath[] =
    FILE_PATH_LITERAL("net/data/url_request_unittest");

}  // namespace

namespace net {

TestNetworkQualityEstimator::TestNetworkQualityEstimator()
    : TestNetworkQualityEstimator(std::map<std::string, std::string>()) {}

TestNetworkQualityEstimator::TestNetworkQualityEstimator(
    const std::map<std::string, std::string>& variation_params)
    : TestNetworkQualityEstimator(variation_params, true, true) {}

TestNetworkQualityEstimator::TestNetworkQualityEstimator(
    const std::map<std::string, std::string>& variation_params,
    bool allow_local_host_requests_for_tests,
    bool allow_smaller_responses_for_tests)
    : TestNetworkQualityEstimator(variation_params,
                                  allow_local_host_requests_for_tests,
                                  allow_smaller_responses_for_tests,
                                  false) {}

TestNetworkQualityEstimator::TestNetworkQualityEstimator(
    const std::map<std::string, std::string>& variation_params,
    bool allow_local_host_requests_for_tests,
    bool allow_smaller_responses_for_tests,
    bool suppress_notifications_for_testing)
    : NetworkQualityEstimator(
          std::make_unique<NetworkQualityEstimatorParams>(variation_params),
          NetLog::Get()),
      suppress_notifications_for_testing_(suppress_notifications_for_testing),
      embedded_test_server_(base::FilePath(kTestFilePath)) {
  SetUseLocalHostRequestsForTesting(allow_local_host_requests_for_tests);
  SetUseSmallResponsesForTesting(allow_smaller_responses_for_tests);
}

TestNetworkQualityEstimator::TestNetworkQualityEstimator(
    std::unique_ptr<NetworkQualityEstimatorParams> params)
    : NetworkQualityEstimator(std::move(params), NetLog::Get()),
      suppress_notifications_for_testing_(false),
      embedded_test_server_(base::FilePath(kTestFilePath)) {}

TestNetworkQualityEstimator::~TestNetworkQualityEstimator() = default;

void TestNetworkQualityEstimator::RunOneRequest() {
  // Set up the embedded test server.
  if (!embedded_test_server_.Started()) {
    EXPECT_TRUE(embedded_test_server_.Start());
  }

  TestDelegate test_delegate;
  auto builder = CreateTestURLRequestContextBuilder();
  builder->set_network_quality_estimator(this);
  auto context = builder->Build();
  std::unique_ptr<URLRequest> request(
      context->CreateRequest(GetEchoURL(), DEFAULT_PRIORITY, &test_delegate,
                             TRAFFIC_ANNOTATION_FOR_TESTS));
  request->SetLoadFlags(request->load_flags() | LOAD_MAIN_FRAME_DEPRECATED);
  request->Start();
  test_delegate.RunUntilComplete();
}

void TestNetworkQualityEstimator::SimulateNetworkChange(
    NetworkChangeNotifier::ConnectionType new_connection_type,
    const std::string& network_id) {
  current_network_type_ = new_connection_type;
  current_network_id_ = network_id;
  OnConnectionTypeChanged(new_connection_type);
}

const GURL TestNetworkQualityEstimator::GetEchoURL() {
  // Set up the embedded test server.
  if (!embedded_test_server_.Started()) {
    EXPECT_TRUE(embedded_test_server_.Start());
  }
  return embedded_test_server_.GetURL("/simple.html");
}

const GURL TestNetworkQualityEstimator::GetRedirectURL() {
  // Set up the embedded test server.
  if (!embedded_test_server_.Started()) {
    EXPECT_TRUE(embedded_test_server_.Start());
  }
  return embedded_test_server_.GetURL("/redirect302-to-https");
}

EffectiveConnectionType
TestNetworkQualityEstimator::GetEffectiveConnectionType() const {
  if (effective_connection_type_)
    return effective_connection_type_.value();
  return NetworkQualityEstimator::GetEffectiveConnectionType();
}

EffectiveConnectionType
TestNetworkQualityEstimator::GetRecentEffectiveConnectionTypeUsingMetrics(
    base::TimeDelta* http_rtt,
    base::TimeDelta* transport_rtt,
    base::TimeDelta* end_to_end_rtt,
    int32_t* downstream_throughput_kbps,
    size_t* observations_count,
    size_t* end_to_end_rtt_observation_count) const {
  if (recent_effective_connection_type_) {
    GetRecentRTT(nqe::internal::OBSERVATION_CATEGORY_HTTP, base::TimeTicks(),
                 http_rtt, nullptr);
    GetRecentRTT(nqe::internal::OBSERVATION_CATEGORY_TRANSPORT,
                 base::TimeTicks(), transport_rtt, observations_count);
    GetRecentDownlinkThroughputKbps(base::TimeTicks(),
                                    downstream_throughput_kbps);
    return recent_effective_connection_type_.value();
  }
  return NetworkQualityEstimator::GetRecentEffectiveConnectionTypeUsingMetrics(
      http_rtt, transport_rtt, end_to_end_rtt, downstream_throughput_kbps,
      observations_count, end_to_end_rtt_observation_count);
}

bool TestNetworkQualityEstimator::GetRecentRTT(
    nqe::internal::ObservationCategory observation_category,
    const base::TimeTicks& start_time,
    base::TimeDelta* rtt,
    size_t* observations_count) const {
  switch (observation_category) {
    case nqe::internal::OBSERVATION_CATEGORY_HTTP:

      if (start_time.is_null()) {
        if (start_time_null_http_rtt_) {
          *rtt = start_time_null_http_rtt_.value();
          return true;
        }
        return NetworkQualityEstimator::GetRecentRTT(
            observation_category, start_time, rtt, observations_count);
      }
      if (recent_http_rtt_) {
        *rtt = recent_http_rtt_.value();
        return true;
      }
      break;

    case nqe::internal::OBSERVATION_CATEGORY_TRANSPORT:
      if (start_time.is_null()) {
        if (start_time_null_transport_rtt_) {
          *rtt = start_time_null_transport_rtt_.value();
          if (transport_rtt_observation_count_last_ect_computation_) {
            *observations_count =
                transport_rtt_observation_count_last_ect_computation_.value();
          }
          return true;
        }
        return NetworkQualityEstimator::GetRecentRTT(
            observation_category, start_time, rtt, observations_count);
      }

      if (recent_transport_rtt_) {
        *rtt = recent_transport_rtt_.value();
        return true;
      }
      break;
    case nqe::internal::OBSERVATION_CATEGORY_END_TO_END:
      if (start_time_null_end_to_end_rtt_) {
        *rtt = start_time_null_end_to_end_rtt_.value();
        return true;
      }
      break;
    case nqe::internal::OBSERVATION_CATEGORY_COUNT:
      NOTREACHED_IN_MIGRATION();
  }

  return NetworkQualityEstimator::GetRecentRTT(observation_category, start_time,
                                               rtt, observations_count);
}

std::optional<base::TimeDelta> TestNetworkQualityEstimator::GetTransportRTT()
    const {
  if (start_time_null_transport_rtt_)
    return start_time_null_transport_rtt_;
  return NetworkQualityEstimator::GetTransportRTT();
}

bool TestNetworkQualityEstimator::GetRecentDownlinkThroughputKbps(
    const base::TimeTicks& start_time,
    int32_t* kbps) const {
  if (start_time.is_null()) {
    if (start_time_null_downlink_throughput_kbps_) {
      *kbps = start_time_null_downlink_throughput_kbps_.value();
      return true;
    }
    return NetworkQualityEstimator::GetRecentDownlinkThroughputKbps(start_time,
                                                                    kbps);
  }

  if (recent_downlink_throughput_kbps_) {
    *kbps = recent_downlink_throughput_kbps_.value();
    return true;
  }
  return NetworkQualityEstimator::GetRecentDownlinkThroughputKbps(start_time,
                                                                  kbps);
}

base::TimeDelta TestNetworkQualityEstimator::GetRTTEstimateInternal(
    base::TimeTicks start_time,
    nqe::internal::ObservationCategory observation_category,
    int percentile,
    size_t* observations_count) const {
  if (rtt_estimate_internal_)
    return rtt_estimate_internal_.value();

  return NetworkQualityEstimator::GetRTTEstimateInternal(
      start_time, observation_category, percentile, observations_count);
}

int TestNetworkQualityEstimator::GetEntriesCount(NetLogEventType type) const {
  return net_log_observer_.GetEntriesWithType(type).size();
}

std::string TestNetworkQualityEstimator::GetNetLogLastStringValue(
    NetLogEventType type,
    const std::string& key) const {
  auto entries = net_log_observer_.GetEntries();

  for (int i = entries.size() - 1; i >= 0; --i) {
    if (entries[i].type == type) {
      auto value = GetOptionalStringValueFromParams(entries[i], key);
      if (value)
        return *value;
    }
  }
  return std::string();
}

int TestNetworkQualityEstimator::GetNetLogLastIntegerValue(
    NetLogEventType type,
    const std::string& key) const {
  auto entries = net_log_observer_.GetEntries();

  for (int i = entries.size() - 1; i >= 0; --i) {
    if (entries[i].type == type) {
      auto value = GetOptionalIntegerValueFromParams(entries[i], key);
      if (value)
        return *value;
    }
  }
  return 0;
}

void TestNetworkQualityEstimator::
    NotifyObserversOfRTTOrThroughputEstimatesComputed(
        const net::nqe::internal::NetworkQuality& network_quality) {
  for (auto& observer : rtt_and_throughput_estimates_observer_list_) {
    observer.OnRTTOrThroughputEstimatesComputed(
        network_quality.http_rtt(), network_quality.transport_rtt(),
        network_quality.downstream_throughput_kbps());
  }
}

void TestNetworkQualityEstimator::
    SetAndNotifyObserversOfEffectiveConnectionType(
        EffectiveConnectionType type) {
  set_effective_connection_type(type);
  for (auto& observer : effective_connection_type_observer_list_)
    observer.OnEffectiveConnectionTypeChanged(type);
}

std::optional<net::EffectiveConnectionType>
TestNetworkQualityEstimator::GetOverrideECT() const {
  return effective_connection_type_;
}

void TestNetworkQualityEstimator::
    SetAndNotifyObserversOfP2PActiveConnectionsCountChange(uint32_t count) {
  p2p_connections_count_ = count;
  for (auto& observer : peer_to_peer_type_observer_list_)
    observer.OnPeerToPeerConnectionsCountChange(count);
}

void TestNetworkQualityEstimator::RecordSpdyPingLatency(
    const HostPortPair& host_port_pair,
    base::TimeDelta rtt) {
  ++ping_rtt_received_count_;
  NetworkQualityEstimator::RecordSpdyPingLatency(host_port_pair, rtt);
}

const NetworkQualityEstimatorParams* TestNetworkQualityEstimator::params()
    const {
  return params_.get();
}

nqe::internal::NetworkID TestNetworkQualityEstimator::GetCurrentNetworkID()
    const {
  return nqe::internal::NetworkID(current_network_type_, current_network_id_,
                                  INT32_MIN);
}

TestNetworkQualityEstimator::LocalHttpTestServer::LocalHttpTestServer(
    const base::FilePath& document_root) {
  AddDefaultHandlers(document_root);
}

void TestNetworkQualityEstimator::NotifyObserversOfRTTOrThroughputComputed()
    const {
  if (suppress_notifications_for_testing_)
    return;

  NetworkQualityEstimator::NotifyObserversOfRTTOrThroughputComputed();
}

void TestNetworkQualityEstimator::
    NotifyRTTAndThroughputEstimatesObserverIfPresent(
        RTTAndThroughputEstimatesObserver* observer) const {
  if (suppress_notifications_for_testing_)
    return;

  NetworkQualityEstimator::NotifyRTTAndThroughputEstimatesObserverIfPresent(
      observer);
}

void TestNetworkQualityEstimator::SetStartTimeNullHttpRtt(
    const base::TimeDelta http_rtt) {
  start_time_null_http_rtt_ = http_rtt;
  // Force compute effective connection type so that the new RTT value is
  // immediately picked up. This ensures that the next call to
  // GetEffectiveConnectionType() returns the effective connnection type
  // that was computed based on |http_rtt|.
  ComputeEffectiveConnectionType();
}

void TestNetworkQualityEstimator::SetStartTimeNullTransportRtt(
    const base::TimeDelta transport_rtt) {
  start_time_null_transport_rtt_ = transport_rtt;
  // Force compute effective connection type so that the new RTT value is
  // immediately picked up. This ensures that the next call to
  // GetEffectiveConnectionType() returns the effective connnection type
  // that was computed based on |transport_rtt|.
  ComputeEffectiveConnectionType();
}

}  // namespace net
