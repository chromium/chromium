// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_NQE_NETWORK_QUALITY_ESTIMATOR_TEST_UTIL_H_
#define NET_NQE_NETWORK_QUALITY_ESTIMATOR_TEST_UTIL_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "net/base/network_change_notifier.h"
#include "net/log/net_log.h"
#include "net/log/test_net_log.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

// Helps in setting the current network type and id.
class TestNetworkQualityEstimator : public NetworkQualityEstimator {
 public:
  TestNetworkQualityEstimator();

  explicit TestNetworkQualityEstimator(
      const std::map<std::string, std::string>& variation_params);

  TestNetworkQualityEstimator(
      const std::map<std::string, std::string>& variation_params,
      bool allow_local_host_requests_for_tests,
      bool allow_smaller_responses_for_tests);

  TestNetworkQualityEstimator(
      const std::map<std::string, std::string>& variation_params,
      bool allow_local_host_requests_for_tests,
      bool allow_smaller_responses_for_tests,
      bool suppress_notifications_for_testing);

  explicit TestNetworkQualityEstimator(
      std::unique_ptr<NetworkQualityEstimatorParams> params);

  TestNetworkQualityEstimator(const TestNetworkQualityEstimator&) = delete;
  TestNetworkQualityEstimator& operator=(const TestNetworkQualityEstimator&) =
      delete;

  ~TestNetworkQualityEstimator() override;

  // Runs one URL request to completion.
  void RunOneRequest();

  // Overrides the current network type and id.
  // Notifies network quality estimator of a change in connection.
  void SimulateNetworkChange(
      NetworkChangeNotifier::ConnectionType new_connection_type,
      const std::string& network_id);

  // Returns a GURL hosted at the embedded test server.
  const GURL GetEchoURL();

  // Returns a GURL hosted at the embedded test server which contains redirect
  // to another HTTPS URL.
  const GURL GetRedirectURL();

  void set_effective_connection_type(EffectiveConnectionType type) {
    effective_connection_type_ = type;
  }

  // Returns the effective connection type that was set using
  // |set_effective_connection_type|. If the connection type has not been set,
  // then the base implementation is called.
  EffectiveConnectionType GetEffectiveConnectionType() const override;

  void set_recent_effective_connection_type(EffectiveConnectionType type) {
    // Callers should not set effective connection type along with the
    // lower-layer metrics.
    DCHECK(!start_time_null_http_rtt_ && !recent_http_rtt_ &&
           !start_time_null_transport_rtt_ && !recent_transport_rtt_ &&
           !start_time_null_downlink_throughput_kbps_ &&
           !recent_downlink_throughput_kbps_);
    recent_effective_connection_type_ = type;
  }

  // Returns the effective connection type that was set using
  // |set_effective_connection_type|. If the connection type has not been set,
  // then the base implementation is called. |http_rtt|, |transport_rtt| and
  // |downstream_throughput_kbps| are set to the values that were previously
  // set by calling set_recent_http_rtt(), set_recent_transport_rtt()
  // and set_recent_transport_rtt() methods, respectively.
  EffectiveConnectionType GetRecentEffectiveConnectionTypeUsingMetrics(
      base::TimeDelta* http_rtt,
      base::TimeDelta* transport_rtt,
      base::TimeDelta* end_to_end_rtt,
      int32_t* downstream_throughput_kbps,
      size_t* observations_count,
      size_t* end_to_end_rtt_observation_count) const override;

  void NotifyObserversOfRTTOrThroughputComputed() const override;

  void NotifyRTTAndThroughputEstimatesObserverIfPresent(
      RTTAndThroughputEstimatesObserver* observer) const override;

  // Force set the HTTP RTT estimate.
  void SetStartTimeNullHttpRtt(const base::TimeDelta http_rtt);

  void set_recent_http_rtt(const base::TimeDelta& recent_http_rtt) {
    // Callers should not set effective connection type along with the
    // lower-layer metrics.
    DCHECK(!effective_connection_type_ && !recent_effective_connection_type_);
    recent_http_rtt_ = recent_http_rtt;
  }

  // Returns the recent RTT that was set using set_recent_http_rtt() or
  // set_recent_transport_rtt(). If the recent RTT has not been set, then the
  // base implementation is called.
  bool GetRecentRTT(nqe::internal::ObservationCategory observation_category,
                    const base::TimeTicks& start_time,
                    base::TimeDelta* rtt,
                    size_t* observations_count) const override;

  // Force set the transport RTT estimate.
  void SetStartTimeNullTransportRtt(const base::TimeDelta transport_rtt);

  void set_recent_transport_rtt(const base::TimeDelta& recent_transport_rtt) {
    // Callers should not set effective connection type along with the
    // lower-layer metrics.
    DCHECK(!effective_connection_type_ && !recent_effective_connection_type_);
    recent_transport_rtt_ = recent_transport_rtt;
  }

  std::optional<base::TimeDelta> GetTransportRTT() const override;

  void set_start_time_null_downlink_throughput_kbps(
      int32_t downlink_throughput_kbps) {
    start_time_null_downlink_throughput_kbps_ = downlink_throughput_kbps;
  }

  void set_recent_downlink_throughput_kbps(
      int32_t recent_downlink_throughput_kbps) {
    // Callers should not set effective connection type along with the
    // lower-layer metrics.
    DCHECK(!effective_connection_type_ && !recent_effective_connection_type_);
    recent_downlink_throughput_kbps_ = recent_downlink_throughput_kbps;
  }
  // Returns the downlink throughput that was set using
  // |set_recent_downlink_throughput_kbps|. If the downlink throughput has not
  // been set, then the base implementation is called.
  bool GetRecentDownlinkThroughputKbps(const base::TimeTicks& start_time,
                                       int32_t* kbps) const override;

  // Returns the recent HTTP RTT value that was set using
  // |set_rtt_estimate_internal|. If it has not been set, then the base
  // implementation is called.
  base::TimeDelta GetRTTEstimateInternal(
      base::TimeTicks start_time,
      nqe::internal::ObservationCategory observation_category,
      int percentile,
      size_t* observations_count) const override;

  void set_rtt_estimate_internal(base::TimeDelta value) {
    rtt_estimate_internal_ = value;
  }

  void set_start_time_null_end_to_end_rtt(const base::TimeDelta rtt) {
    // Callers should not set effective connection type along with the
    // lower-layer metrics.
    DCHECK(!effective_connection_type_ && !recent_effective_connection_type_);
    start_time_null_end_to_end_rtt_ = rtt;
  }

  void set_start_time_null_end_to_end_rtt_observation_count(size_t count) {
    end_to_end_rtt_observation_count_at_last_ect_computation_ = count;
  }

  // Returns the number of entries in |net_log_| that have type set to |type|.
  int GetEntriesCount(NetLogEventType type) const;

  // Returns the value of the parameter with name |key| from the last net log
  // entry that has type set to |type|. Different methods are provided for
  // values of different types.
  std::string GetNetLogLastStringValue(NetLogEventType type,
                                       const std::string& key) const;
  int GetNetLogLastIntegerValue(NetLogEventType type,
                                const std::string& key) const;

  // Notifies the registered observers that the network quality estimate has
  // changed to |network_quality|.
  void NotifyObserversOfRTTOrThroughputEstimatesComputed(
      const net::nqe::internal::NetworkQuality& network_quality);

  // Updates the computed effective connection type to |type| and notifies the
  // registered observers that the effective connection type has changed to
  // |type|.
  void SetAndNotifyObserversOfEffectiveConnectionType(
      EffectiveConnectionType type);

  // Updates the count of active P2P connections to |count| and notifies the
  // registered observers that the active P2P connection counts has changed to
  // |count|.
  void SetAndNotifyObserversOfP2PActiveConnectionsCountChange(uint32_t count);

  void SetTransportRTTAtastECTSampleCount(size_t count) {
    transport_rtt_observation_count_last_ect_computation_ = count;
  }

  // Returns count of ping RTTs received from H2/spdy connections.
  size_t ping_rtt_received_count() const { return ping_rtt_received_count_; }

  const NetworkQualityEstimatorParams* params() const;

  void RecordSpdyPingLatency(const HostPortPair& host_port_pair,
                             base::TimeDelta rtt) override;

  using NetworkQualityEstimator::SetTickClockForTesting;
  using NetworkQualityEstimator::OnConnectionTypeChanged;
  using NetworkQualityEstimator::OnUpdatedTransportRTTAvailable;
  using NetworkQualityEstimator::AddAndNotifyObserversOfRTT;
  using NetworkQualityEstimator::AddAndNotifyObserversOfThroughput;
  using NetworkQualityEstimator::IsHangingRequest;

 private:
  class LocalHttpTestServer : public EmbeddedTestServer {
   public:
    explicit LocalHttpTestServer(const base::FilePath& document_root);
  };

  // NetworkQualityEstimator implementation that returns the overridden
  // network id and signal strength (instead of invoking platform APIs).
  nqe::internal::NetworkID GetCurrentNetworkID() const override;

  std::optional<net::EffectiveConnectionType> GetOverrideECT() const override;

  // Net log observer used to test correctness of NetLog entries.
  net::RecordingNetLogObserver net_log_observer_;

  // If set, GetEffectiveConnectionType() and GetRecentEffectiveConnectionType()
  // would return the set values, respectively.
  std::optional<EffectiveConnectionType> effective_connection_type_;
  std::optional<EffectiveConnectionType> recent_effective_connection_type_;

  NetworkChangeNotifier::ConnectionType current_network_type_ =
      NetworkChangeNotifier::CONNECTION_UNKNOWN;
  std::string current_network_id_;

  // If set, GetRecentHttpRTT() would return one of the set values.
  // |start_time_null_http_rtt_| is returned if the |start_time| is null.
  // Otherwise, |recent_http_rtt_| is returned.
  std::optional<base::TimeDelta> start_time_null_http_rtt_;
  std::optional<base::TimeDelta> recent_http_rtt_;

  // If set, GetRecentTransportRTT() would return one of the set values.
  // |start_time_null_transport_rtt_| is returned if the |start_time| is null.
  // Otherwise, |recent_transport_rtt_| is returned.
  std::optional<base::TimeDelta> start_time_null_transport_rtt_;
  std::optional<base::TimeDelta> recent_transport_rtt_;

  // If set, GetRecentDownlinkThroughputKbps() would return one of the set
  // values. |start_time_null_downlink_throughput_kbps_| is returned if the
  // |start_time| is null. Otherwise, |recent_downlink_throughput_kbps_| is
  // returned.
  std::optional<int32_t> start_time_null_downlink_throughput_kbps_;
  std::optional<int32_t> recent_downlink_throughput_kbps_;

  // If set, GetRTTEstimateInternal() would return the set value.
  std::optional<base::TimeDelta> rtt_estimate_internal_;

  // If set, GetRTTEstimateInternal() would return the set value.
  std::optional<base::TimeDelta> start_time_null_end_to_end_rtt_;

  // If true, notifications are not sent to any of the observers.
  const bool suppress_notifications_for_testing_;

  size_t ping_rtt_received_count_ = 0;

  std::optional<size_t> transport_rtt_observation_count_last_ect_computation_;

  // Destroy this first, since destroying the test server will wait (and run an
  // event loop while waiting for the test server to stop on its task runner).
  // While this loop is running, the estimator may receive posted tasks with new
  // observations, which may potentially read and write other fields of `this`.
  LocalHttpTestServer embedded_test_server_;
};

}  // namespace net

#endif  // NET_NQE_NETWORK_QUALITY_ESTIMATOR_TEST_UTIL_H_
