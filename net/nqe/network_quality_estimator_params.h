// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_NQE_NETWORK_QUALITY_ESTIMATOR_PARAMS_H_
#define NET_NQE_NETWORK_QUALITY_ESTIMATOR_PARAMS_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_quality.h"

namespace net {

// Forces NQE to return a specific effective connection type. Set using the
// |params| provided to the NetworkQualityEstimatorParams constructor.
NET_EXPORT extern const char kForceEffectiveConnectionType[];
NET_EXPORT extern const char kEffectiveConnectionTypeSlow2GOnCellular[];

// HTTP RTT thresholds for different effective connection types.
NET_EXPORT extern const base::TimeDelta
    kHttpRttEffectiveConnectionTypeThresholds[EFFECTIVE_CONNECTION_TYPE_LAST];

// NetworkQualityEstimatorParams computes the configuration parameters for
// the network quality estimator.
class NET_EXPORT NetworkQualityEstimatorParams {
 public:
  // |params| is the map containing all field trial parameters related to
  // NetworkQualityEstimator field trial.
  explicit NetworkQualityEstimatorParams(
      const std::map<std::string, std::string>& params);

  ~NetworkQualityEstimatorParams();

  // Returns the default observation for connection |type|. The default
  // observations are different for different connection types (e.g., 2G, 3G,
  // 4G, WiFi). The default observations may be used to determine the network
  // quality in absence of any other information.
  const nqe::internal::NetworkQuality& DefaultObservation(
      NetworkChangeNotifier::ConnectionType type) const;

  // Returns the typical network quality for connection |type|.
  const nqe::internal::NetworkQuality& TypicalNetworkQuality(
      EffectiveConnectionType type) const;

  // Returns the threshold for effective connection type |type|.
  const nqe::internal::NetworkQuality& ConnectionThreshold(
      EffectiveConnectionType type) const;

  // Returns the minimum number of requests in-flight to consider the network
  // fully utilized. A throughput observation is taken only when the network is
  // considered as fully utilized.
  size_t throughput_min_requests_in_flight() const;

  // Tiny transfer sizes may give inaccurate throughput results.
  // Minimum size of the transfer over which the throughput is computed.
  int64_t GetThroughputMinTransferSizeBits() const;

  // Returns the weight multiplier per second, which represents the factor by
  // which the weight of an observation reduces every second.
  double weight_multiplier_per_second() const {
    return weight_multiplier_per_second_;
  }

  // Returns the factor by which the weight of an observation reduces for every
  // signal strength level difference between the current signal strength, and
  // the signal strength at the time when the observation was taken.
  double weight_multiplier_per_signal_strength_level() const {
    return weight_multiplier_per_signal_strength_level_;
  }

  // Returns an unset value if the effective connection type has not been forced
  // via the |params| provided to this class. Otherwise, returns a value set to
  // the effective connection type that has been forced. Forced ECT can be
  // forced based on |connection_type| (e.g. Slow-2G on cellular, and default on
  // other connection type).
  base::Optional<EffectiveConnectionType> GetForcedEffectiveConnectionType(
      NetworkChangeNotifier::ConnectionType connection_type);

  void SetForcedEffectiveConnectionType(
      EffectiveConnectionType forced_effective_connection_type) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    forced_effective_connection_type_ = forced_effective_connection_type;
  }

  // Returns true if reading from the persistent cache is enabled.
  bool persistent_cache_reading_enabled() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return persistent_cache_reading_enabled_;
  }

  void set_persistent_cache_reading_enabled(
      bool persistent_cache_reading_enabled) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    persistent_cache_reading_enabled_ = persistent_cache_reading_enabled;
  }

  // Returns the the minimum interval betweeen consecutive notifications to a
  // single socket watcher.
  base::TimeDelta min_socket_watcher_notification_interval() const {
    return min_socket_watcher_notification_interval_;
  }

  // Number of bytes received during a throughput observation window of duration
  // 1 HTTP RTT should be at least the value returned by this method times
  // the typical size of a congestion window. If not, the throughput observation
  // window is heuristically determined as hanging.
  double throughput_hanging_requests_cwnd_size_multiplier() const {
    return throughput_hanging_requests_cwnd_size_multiplier_;
  }

  // Returns the multiplier by which the transport RTT should be multipled when
  // computing the HTTP RTT. The multiplied value of the transport RTT serves
  // as a lower bound to the HTTP RTT estimate. e.g., if the multiplied
  // transport RTT is 100 msec., then HTTP RTT estimate can't be lower than
  // 100 msec. Returns a negative value if the param is not set.
  double lower_bound_http_rtt_transport_rtt_multiplier() const {
    return lower_bound_http_rtt_transport_rtt_multiplier_;
  }

  // Returns the multiplier by which the end to end RTT estimate should be
  // multiplied when computing the HTTP RTT. The multiplied value of the
  // end to end RTT serves as an upper bound to the HTTP RTT estimate. e.g., if
  // the multiplied end to end RTT is 100 msec., then HTTP RTT estimate can't be
  // more than |upper_bound_http_rtt_endtoend_rtt_multiplier| times 100 msec.
  // Returns a negative value if the param is not set.
  double upper_bound_http_rtt_endtoend_rtt_multiplier() const {
    return upper_bound_http_rtt_endtoend_rtt_multiplier_;
  }

  // For the purpose of estimating the HTTP RTT, a request is marked as hanging
  // only if its RTT is at least this times the transport RTT estimate.
  int hanging_request_http_rtt_upper_bound_transport_rtt_multiplier() const {
    return hanging_request_http_rtt_upper_bound_transport_rtt_multiplier_;
  }

  // For the purpose of estimating the HTTP RTT, a request is marked as hanging
  // only if its RTT is at least this times the HTTP RTT estimate.
  int hanging_request_http_rtt_upper_bound_http_rtt_multiplier() const {
    return hanging_request_http_rtt_upper_bound_http_rtt_multiplier_;
  }

  // For the purpose of estimating the HTTP RTT, a request is marked as hanging
  // only if its RTT is at least as much the value returned by this method.
  base::TimeDelta hanging_request_upper_bound_min_http_rtt() const {
    return hanging_request_upper_bound_min_http_rtt_;
  }

  // Returns the number of transport RTT observations that should be available
  // before the transport RTT estimate can be used to clamp the HTTP RTT
  // estimate. Set to 5 by default which ensures that when the transport RTT
  // is available only from the connection type, it is not used for computing
  // the HTTP RTT estimate.
  size_t http_rtt_transport_rtt_min_count() const {
    return http_rtt_transport_rtt_min_count_;
  }

  // Returns the minimum interval between successive computations of the
  // increase in transport RTT.
  base::TimeDelta increase_in_transport_rtt_logging_interval() const {
    return increase_in_transport_rtt_logging_interval_;
  }

  // The maximum age of RTT observations for them to be considered recent for
  // the computation of the increase in RTT.
  base::TimeDelta recent_time_threshold() const {
    return recent_time_threshold_;
  }

  // The maximum age of observations for them to be considered useful for
  // calculating the minimum transport RTT from the historical data.
  base::TimeDelta historical_time_threshold() const {
    return historical_time_threshold_;
  }

  // Determines if the responses smaller than |kMinTransferSizeInBytes|
  // or shorter than |kMinTransferSizeInBytes| can be used in estimating the
  // network quality. Set to true only for tests.
  bool use_small_responses() const;

  // Returns the typical HTTP RTT that maps to the given
  // |effective_connection_type|. May return invalid value if
  // |effective_connection_type| is less than Slow2G or faster than 4G,
  static base::TimeDelta GetDefaultTypicalHttpRtt(
      EffectiveConnectionType effective_connection_type);

  // Returns the typical downslink throughput (in kbps) that maps to the given
  // |effective_connection_type|. May return invalid value if
  // |effective_connection_type| is less than Slow2G or faster than 4G,
  static int32_t GetDefaultTypicalDownlinkKbps(
      EffectiveConnectionType effective_connection_type);

  // |use_small_responses| should only be true when testing.
  // Allows the responses smaller than |kMinTransferSizeInBits| to be used for
  // network quality estimation.
  void SetUseSmallResponsesForTesting(bool use_small_responses);

  // If an in-flight request does not receive any data for a duration longer
  // than the value of this multiplier times the current HTTP RTT estimate, then
  // the request should be considered as hanging. If this multiplier has a
  // negative or a zero value, then none of the request should be considered as
  // hanging.
  int hanging_request_duration_http_rtt_multiplier() const {
    return hanging_request_duration_http_rtt_multiplier_;
  }

  // An in-flight request may be marked as hanging only if it does not receive
  // any data for at least this duration.
  base::TimeDelta hanging_request_min_duration() const {
    return hanging_request_min_duration_;
  }

  // Returns true if default values provided by the platform should be used for
  // estimation. Set to false only for testing.
  bool add_default_platform_observations() const {
    return add_default_platform_observations_;
  }

  // Number of observations received after which the effective connection type
  // should be recomputed.
  size_t count_new_observations_received_compute_ect() const { return 50; }

  // Maximum number of observations that can be held in a single
  // ObservationBuffer.
  size_t observation_buffer_size() const { return 300; }

  // Minimun interval between consecutive notifications from socket
  // watchers who live on the same thread as the network quality estimator.
  base::TimeDelta socket_watchers_min_notification_interval() const {
    return socket_watchers_min_notification_interval_;
  }

  // Returns true if end-to-end RTT estimates can be used for computing network
  // quality estimate.
  bool use_end_to_end_rtt() const { return use_end_to_end_rtt_; }

  // Return true if ECT value should be capped based on the current signal
  // strength.
  bool cap_ect_based_on_signal_strength() const {
    return cap_ect_based_on_signal_strength_;
  }

  // Returns a multiplier which is used to clamp Kbps on slow connections. For
  // a given ECT, the upper bound on Kbps is computed based on this returned
  // multiplier and the typical Kbps for the given ECT. If
  // upper_bound_typical_kbps_multiplier() is -1, then clamping should be
  // disabled.
  double upper_bound_typical_kbps_multiplier() const {
    return upper_bound_typical_kbps_multiplier_;
  }

  // Returns true if the signal strength should be queried on WiFi connections.
  bool get_wifi_signal_strength() const { return get_wifi_signal_strength_; }

  // Sets the forced effective connection type as |type|.
  void SetForcedEffectiveConnectionTypeForTesting(EffectiveConnectionType type);

 private:
  // Map containing all field trial parameters related to
  // NetworkQualityEstimator field trial.
  const std::map<std::string, std::string> params_;

  const size_t throughput_min_requests_in_flight_;
  const int throughput_min_transfer_size_kilobytes_;
  const double throughput_hanging_requests_cwnd_size_multiplier_;
  const double weight_multiplier_per_second_;
  const double weight_multiplier_per_signal_strength_level_;
  base::Optional<EffectiveConnectionType> forced_effective_connection_type_;
  const bool forced_effective_connection_type_on_cellular_only_;
  bool persistent_cache_reading_enabled_;
  const base::TimeDelta min_socket_watcher_notification_interval_;
  const double lower_bound_http_rtt_transport_rtt_multiplier_;
  const double upper_bound_http_rtt_endtoend_rtt_multiplier_;
  const int hanging_request_http_rtt_upper_bound_transport_rtt_multiplier_;
  const int hanging_request_http_rtt_upper_bound_http_rtt_multiplier_;
  const base::TimeDelta hanging_request_upper_bound_min_http_rtt_;
  const size_t http_rtt_transport_rtt_min_count_;
  const base::TimeDelta increase_in_transport_rtt_logging_interval_;
  const base::TimeDelta recent_time_threshold_;
  const base::TimeDelta historical_time_threshold_;
  const int hanging_request_duration_http_rtt_multiplier_;
  const base::TimeDelta hanging_request_min_duration_;
  const bool add_default_platform_observations_;
  const base::TimeDelta socket_watchers_min_notification_interval_;
  const bool use_end_to_end_rtt_;
  const bool cap_ect_based_on_signal_strength_;
  const double upper_bound_typical_kbps_multiplier_;
  const bool get_wifi_signal_strength_;

  bool use_small_responses_;

  // Default network quality observations obtained from |params_|.
  nqe::internal::NetworkQuality
      default_observations_[NetworkChangeNotifier::CONNECTION_LAST + 1];

  // Typical network quality for different effective connection types obtained
  // from |params_|.
  nqe::internal::NetworkQuality typical_network_quality_
      [EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_LAST];

  // Thresholds for different effective connection types obtained from
  // |params_|. These thresholds encode how different connection types behave
  // in general.
  nqe::internal::NetworkQuality connection_thresholds_
      [EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_LAST];

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(NetworkQualityEstimatorParams);
};

}  // namespace net

#endif  // NET_NQE_NETWORK_QUALITY_ESTIMATOR_PARAMS_H_
