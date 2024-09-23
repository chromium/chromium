// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/nqe/network_quality_estimator_params.h"

#include <stdint.h>

#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "net/base/features.h"

namespace net {

const char kForceEffectiveConnectionType[] = "force_effective_connection_type";
const char kEffectiveConnectionTypeSlow2GOnCellular[] = "Slow-2G-On-Cellular";
const base::TimeDelta
    kHttpRttEffectiveConnectionTypeThresholds[EFFECTIVE_CONNECTION_TYPE_LAST] =
        {base::Milliseconds(0),    base::Milliseconds(0),
         base::Milliseconds(2010), base::Milliseconds(1420),
         base::Milliseconds(272),  base::Milliseconds(0)};

namespace {

// Minimum valid value of the variation parameter that holds RTT (in
// milliseconds) values.
static const int kMinimumRTTVariationParameterMsec = 1;

// Minimum valid value of the variation parameter that holds throughput (in
// kilobits per second) values.
static const int kMinimumThroughputVariationParameterKbps = 1;

// Returns the value of |parameter_name| read from |params|. If the
// value is unavailable from |params|, then |default_value| is returned.
int64_t GetValueForVariationParam(
    const std::map<std::string, std::string>& params,
    const std::string& parameter_name,
    int64_t default_value) {
  const auto it = params.find(parameter_name);
  int64_t variations_value = default_value;
  if (it != params.end() &&
      base::StringToInt64(it->second, &variations_value)) {
    return variations_value;
  }
  return default_value;
}

// Returns the variation value for |parameter_name|. If the value is
// unavailable, |default_value| is returned.
double GetDoubleValueForVariationParamWithDefaultValue(
    const std::map<std::string, std::string>& params,
    const std::string& parameter_name,
    double default_value) {
  const auto it = params.find(parameter_name);
  if (it == params.end())
    return default_value;

  double variations_value = default_value;
  if (!base::StringToDouble(it->second, &variations_value))
    return default_value;
  return variations_value;
}

// Returns the variation value for |parameter_name|. If the value is
// unavailable, |default_value| is returned.
std::string GetStringValueForVariationParamWithDefaultValue(
    const std::map<std::string, std::string>& params,
    const std::string& parameter_name,
    const std::string& default_value) {
  const auto it = params.find(parameter_name);
  if (it == params.end())
    return default_value;
  return it->second;
}

double GetWeightMultiplierPerSecond(
    const std::map<std::string, std::string>& params) {
  // Default value of the half life (in seconds) for computing time weighted
  // percentiles. Every half life, the weight of all observations reduces by
  // half. Lowering the half life would reduce the weight of older values
  // faster.
  int half_life_seconds = 60;
  int32_t variations_value = 0;
  auto it = params.find("HalfLifeSeconds");
  if (it != params.end() && base::StringToInt(it->second, &variations_value) &&
      variations_value >= 1) {
    half_life_seconds = variations_value;
  }
  DCHECK_GT(half_life_seconds, 0);
  return pow(0.5, 1.0 / half_life_seconds);
}

bool GetPersistentCacheReadingEnabled(
    const std::map<std::string, std::string>& params) {
  if (GetStringValueForVariationParamWithDefaultValue(
          params, "persistent_cache_reading_enabled", "true") != "true") {
    return false;
  }
  return true;
}

base::TimeDelta GetMinSocketWatcherNotificationInterval(
    const std::map<std::string, std::string>& params) {
  // Use 1000 milliseconds as the default value.
  return base::Milliseconds(GetValueForVariationParam(
      params, "min_socket_watcher_notification_interval_msec", 1000));
}

// static
const char* GetNameForConnectionTypeInternal(
    NetworkChangeNotifier::ConnectionType connection_type) {
  switch (connection_type) {
    case NetworkChangeNotifier::CONNECTION_UNKNOWN:
      return "Unknown";
    case NetworkChangeNotifier::CONNECTION_ETHERNET:
      return "Ethernet";
    case NetworkChangeNotifier::CONNECTION_WIFI:
      return "WiFi";
    case NetworkChangeNotifier::CONNECTION_2G:
      return "2G";
    case NetworkChangeNotifier::CONNECTION_3G:
      return "3G";
    case NetworkChangeNotifier::CONNECTION_4G:
      return "4G";
    case NetworkChangeNotifier::CONNECTION_5G:
      return "5G";
    case NetworkChangeNotifier::CONNECTION_NONE:
      return "None";
    case NetworkChangeNotifier::CONNECTION_BLUETOOTH:
      return "Bluetooth";
  }
  return "";
}

// Sets the default observation for different connection types in
// |default_observations|. The default observations are different for
// different connection types (e.g., 2G, 3G, 4G, WiFi). The default
// observations may be used to determine the network quality in absence of any
// other information.
void ObtainDefaultObservations(
    const std::map<std::string, std::string>& params,
    nqe::internal::NetworkQuality default_observations[]) {
  for (size_t i = 0; i < NetworkChangeNotifier::CONNECTION_LAST; ++i) {
    DCHECK_EQ(nqe::internal::InvalidRTT(), default_observations[i].http_rtt());
    DCHECK_EQ(nqe::internal::InvalidRTT(),
              default_observations[i].transport_rtt());
    DCHECK_EQ(nqe::internal::INVALID_RTT_THROUGHPUT,
              default_observations[i].downstream_throughput_kbps());
  }

  // Default observations for HTTP RTT, transport RTT, and downstream throughput
  // Kbps for the various connection types. These may be overridden by
  // variations params. The default observation for a connection type
  // corresponds to typical network quality for that connection type.
  default_observations[NetworkChangeNotifier::CONNECTION_UNKNOWN] =
      nqe::internal::NetworkQuality(base::Milliseconds(115),
                                    base::Milliseconds(55), 1961);

  default_observations[NetworkChangeNotifier::CONNECTION_ETHERNET] =
      nqe::internal::NetworkQuality(base::Milliseconds(90),
                                    base::Milliseconds(33), 1456);

  default_observations[NetworkChangeNotifier::CONNECTION_WIFI] =
      nqe::internal::NetworkQuality(base::Milliseconds(116),
                                    base::Milliseconds(66), 2658);

  default_observations[NetworkChangeNotifier::CONNECTION_2G] =
      nqe::internal::NetworkQuality(base::Milliseconds(1726),
                                    base::Milliseconds(1531), 74);

  default_observations[NetworkChangeNotifier::CONNECTION_3G] =
      nqe::internal::NetworkQuality(base::Milliseconds(273),
                                    base::Milliseconds(209), 749);

  default_observations[NetworkChangeNotifier::CONNECTION_4G] =
      nqe::internal::NetworkQuality(base::Milliseconds(137),
                                    base::Milliseconds(80), 1708);

  default_observations[NetworkChangeNotifier::CONNECTION_NONE] =
      nqe::internal::NetworkQuality(base::Milliseconds(163),
                                    base::Milliseconds(83), 575);

  default_observations[NetworkChangeNotifier::CONNECTION_BLUETOOTH] =
      nqe::internal::NetworkQuality(base::Milliseconds(385),
                                    base::Milliseconds(318), 476);

  // Override using the values provided via variation params.
  for (size_t i = 0; i <= NetworkChangeNotifier::CONNECTION_LAST; ++i) {
    NetworkChangeNotifier::ConnectionType type =
        static_cast<NetworkChangeNotifier::ConnectionType>(i);

    int32_t variations_value = kMinimumRTTVariationParameterMsec - 1;
    std::string parameter_name =
        std::string(GetNameForConnectionTypeInternal(type))
            .append(".DefaultMedianRTTMsec");
    auto it = params.find(parameter_name);
    if (it != params.end() &&
        base::StringToInt(it->second, &variations_value) &&
        variations_value >= kMinimumRTTVariationParameterMsec) {
      default_observations[i] = nqe::internal::NetworkQuality(
          base::Milliseconds(variations_value),
          default_observations[i].transport_rtt(),
          default_observations[i].downstream_throughput_kbps());
    }

    variations_value = kMinimumRTTVariationParameterMsec - 1;
    parameter_name = std::string(GetNameForConnectionTypeInternal(type))
                         .append(".DefaultMedianTransportRTTMsec");
    it = params.find(parameter_name);
    if (it != params.end() &&
        base::StringToInt(it->second, &variations_value) &&
        variations_value >= kMinimumRTTVariationParameterMsec) {
      default_observations[i] = nqe::internal::NetworkQuality(
          default_observations[i].http_rtt(),
          base::Milliseconds(variations_value),
          default_observations[i].downstream_throughput_kbps());
    }

    variations_value = kMinimumThroughputVariationParameterKbps - 1;
    parameter_name = std::string(GetNameForConnectionTypeInternal(type))
                         .append(".DefaultMedianKbps");
    it = params.find(parameter_name);

    if (it != params.end() &&
        base::StringToInt(it->second, &variations_value) &&
        variations_value >= kMinimumThroughputVariationParameterKbps) {
      default_observations[i] = nqe::internal::NetworkQuality(
          default_observations[i].http_rtt(),
          default_observations[i].transport_rtt(), variations_value);
    }
  }
}

// Typical HTTP RTT value corresponding to a given WebEffectiveConnectionType
// value. Taken from
// https://cs.chromium.org/chromium/src/net/nqe/network_quality_estimator_params.cc.
const base::TimeDelta kTypicalHttpRttEffectiveConnectionType
    [net::EFFECTIVE_CONNECTION_TYPE_LAST] = {
        base::Milliseconds(0),    base::Milliseconds(0),
        base::Milliseconds(3600), base::Milliseconds(1800),
        base::Milliseconds(450),  base::Milliseconds(175)};

// Typical downlink throughput (in Mbps) value corresponding to a given
// WebEffectiveConnectionType value. Taken from
// https://cs.chromium.org/chromium/src/net/nqe/network_quality_estimator_params.cc.
const int32_t kTypicalDownlinkKbpsEffectiveConnectionType
    [net::EFFECTIVE_CONNECTION_TYPE_LAST] = {0, 0, 40, 75, 400, 1600};

// Sets |typical_network_quality| to typical network quality for different
// effective connection types.
void ObtainTypicalNetworkQualities(
    const std::map<std::string, std::string>& params,
    nqe::internal::NetworkQuality typical_network_quality[]) {
  for (size_t i = 0; i < EFFECTIVE_CONNECTION_TYPE_LAST; ++i) {
    DCHECK_EQ(nqe::internal::InvalidRTT(),
              typical_network_quality[i].http_rtt());
    DCHECK_EQ(nqe::internal::InvalidRTT(),
              typical_network_quality[i].transport_rtt());
    DCHECK_EQ(nqe::internal::INVALID_RTT_THROUGHPUT,
              typical_network_quality[i].downstream_throughput_kbps());
  }

  typical_network_quality[EFFECTIVE_CONNECTION_TYPE_SLOW_2G] =
      nqe::internal::NetworkQuality(
          // Set to the 77.5th percentile of 2G RTT observations on Android.
          // This corresponds to the median RTT observation when effective
          // connection type is Slow 2G.
          kTypicalHttpRttEffectiveConnectionType
              [EFFECTIVE_CONNECTION_TYPE_SLOW_2G],
          base::Milliseconds(3000),
          kTypicalDownlinkKbpsEffectiveConnectionType
              [EFFECTIVE_CONNECTION_TYPE_SLOW_2G]);

  typical_network_quality[EFFECTIVE_CONNECTION_TYPE_2G] =
      nqe::internal::NetworkQuality(
          // Set to the 58th percentile of 2G RTT observations on Android. This
          // corresponds to the median RTT observation when effective connection
          // type is 2G.
          kTypicalHttpRttEffectiveConnectionType[EFFECTIVE_CONNECTION_TYPE_2G],
          base::Milliseconds(1500),
          kTypicalDownlinkKbpsEffectiveConnectionType
              [EFFECTIVE_CONNECTION_TYPE_2G]);

  typical_network_quality[EFFECTIVE_CONNECTION_TYPE_3G] =
      nqe::internal::NetworkQuality(
          // Set to the 75th percentile of 3G RTT observations on Android. This
          // corresponds to the median RTT observation when effective connection
          // type is 3G.
          kTypicalHttpRttEffectiveConnectionType[EFFECTIVE_CONNECTION_TYPE_3G],
          base::Milliseconds(400),
          kTypicalDownlinkKbpsEffectiveConnectionType
              [EFFECTIVE_CONNECTION_TYPE_3G]);

  // Set to the 25th percentile of 3G RTT observations on Android.
  typical_network_quality[EFFECTIVE_CONNECTION_TYPE_4G] =
      nqe::internal::NetworkQuality(
          kTypicalHttpRttEffectiveConnectionType[EFFECTIVE_CONNECTION_TYPE_4G],
          base::Milliseconds(125),
          kTypicalDownlinkKbpsEffectiveConnectionType
              [EFFECTIVE_CONNECTION_TYPE_4G]);

  static_assert(
      EFFECTIVE_CONNECTION_TYPE_4G + 1 == EFFECTIVE_CONNECTION_TYPE_LAST,
      "Missing effective connection type");
}

// Sets the thresholds for different effective connection types in
// |connection_thresholds|.
void ObtainConnectionThresholds(
    const std::map<std::string, std::string>& params,
    nqe::internal::NetworkQuality connection_thresholds[]) {
  // First set the default thresholds.
  nqe::internal::NetworkQuality default_effective_connection_type_thresholds
      [EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_LAST];

  DCHECK_LT(base::TimeDelta(), kHttpRttEffectiveConnectionTypeThresholds
                                   [EFFECTIVE_CONNECTION_TYPE_SLOW_2G]);
  default_effective_connection_type_thresholds
      [EFFECTIVE_CONNECTION_TYPE_SLOW_2G] = nqe::internal::NetworkQuality(
          // Set to the 66th percentile of 2G RTT observations on Android.
          kHttpRttEffectiveConnectionTypeThresholds
              [EFFECTIVE_CONNECTION_TYPE_SLOW_2G],
          nqe::internal::InvalidRTT(), nqe::internal::INVALID_RTT_THROUGHPUT);

  DCHECK_LT(
      base::TimeDelta(),
      kHttpRttEffectiveConnectionTypeThresholds[EFFECTIVE_CONNECTION_TYPE_2G]);
  default_effective_connection_type_thresholds[EFFECTIVE_CONNECTION_TYPE_2G] =
      nqe::internal::NetworkQuality(
          // Set to the 50th percentile of RTT observations on Android.
          kHttpRttEffectiveConnectionTypeThresholds
              [EFFECTIVE_CONNECTION_TYPE_2G],
          nqe::internal::InvalidRTT(), nqe::internal::INVALID_RTT_THROUGHPUT);

  DCHECK_LT(
      base::TimeDelta(),
      kHttpRttEffectiveConnectionTypeThresholds[EFFECTIVE_CONNECTION_TYPE_3G]);
  default_effective_connection_type_thresholds[EFFECTIVE_CONNECTION_TYPE_3G] =
      nqe::internal::NetworkQuality(
          // Set to the 50th percentile of 3G RTT observations on Android.
          kHttpRttEffectiveConnectionTypeThresholds
              [EFFECTIVE_CONNECTION_TYPE_3G],
          nqe::internal::InvalidRTT(), nqe::internal::INVALID_RTT_THROUGHPUT);

  // Connection threshold should not be set for 4G effective connection type
  // since it is the fastest.
  static_assert(
      EFFECTIVE_CONNECTION_TYPE_3G + 1 == EFFECTIVE_CONNECTION_TYPE_4G,
      "Missing effective connection type");
  static_assert(
      EFFECTIVE_CONNECTION_TYPE_4G + 1 == EFFECTIVE_CONNECTION_TYPE_LAST,
      "Missing effective connection type");
  for (size_t i = 0; i <= EFFECTIVE_CONNECTION_TYPE_3G; ++i) {
    EffectiveConnectionType effective_connection_type =
        static_cast<EffectiveConnectionType>(i);
    DCHECK_EQ(nqe::internal::InvalidRTT(), connection_thresholds[i].http_rtt());
    DCHECK_EQ(nqe::internal::InvalidRTT(),
              connection_thresholds[i].transport_rtt());
    DCHECK_EQ(nqe::internal::INVALID_RTT_THROUGHPUT,
              connection_thresholds[i].downstream_throughput_kbps());
    if (effective_connection_type == EFFECTIVE_CONNECTION_TYPE_UNKNOWN)
      continue;

    std::string connection_type_name = std::string(
        DeprecatedGetNameForEffectiveConnectionType(effective_connection_type));

    connection_thresholds[i].set_http_rtt(
        base::Milliseconds(GetValueForVariationParam(
            params, connection_type_name + ".ThresholdMedianHttpRTTMsec",
            default_effective_connection_type_thresholds[i]
                .http_rtt()
                .InMilliseconds())));

    DCHECK_EQ(nqe::internal::InvalidRTT(),
              default_effective_connection_type_thresholds[i].transport_rtt());
    DCHECK_EQ(nqe::internal::INVALID_RTT_THROUGHPUT,
              default_effective_connection_type_thresholds[i]
                  .downstream_throughput_kbps());
    DCHECK(i == 0 ||
           connection_thresholds[i].IsFaster(connection_thresholds[i - 1]));
  }
}

std::string GetForcedEffectiveConnectionTypeString(
    const std::map<std::string, std::string>& params) {
  return GetStringValueForVariationParamWithDefaultValue(
      params, kForceEffectiveConnectionType, "");
}

bool GetForcedEffectiveConnectionTypeOnCellularOnly(
    const std::map<std::string, std::string>& params) {
  return GetForcedEffectiveConnectionTypeString(params) ==
         kEffectiveConnectionTypeSlow2GOnCellular;
}

std::optional<EffectiveConnectionType> GetInitForcedEffectiveConnectionType(
    const std::map<std::string, std::string>& params) {
  if (GetForcedEffectiveConnectionTypeOnCellularOnly(params)) {
    return std::nullopt;
  }
  std::string forced_value = GetForcedEffectiveConnectionTypeString(params);
  std::optional<EffectiveConnectionType> ect =
      GetEffectiveConnectionTypeForName(forced_value);
  DCHECK(forced_value.empty() || ect);
  return ect;
}

}  // namespace

NetworkQualityEstimatorParams::NetworkQualityEstimatorParams(
    const std::map<std::string, std::string>& params)
    : params_(params),
      throughput_min_requests_in_flight_(
          GetValueForVariationParam(params_,
                                    "throughput_min_requests_in_flight",
                                    5)),
      throughput_min_transfer_size_kilobytes_(
          GetValueForVariationParam(params_,
                                    "throughput_min_transfer_size_kilobytes",
                                    32)),
      throughput_hanging_requests_cwnd_size_multiplier_(
          GetDoubleValueForVariationParamWithDefaultValue(
              params_,
              "throughput_hanging_requests_cwnd_size_multiplier",
              1)),
      weight_multiplier_per_second_(GetWeightMultiplierPerSecond(params_)),
      forced_effective_connection_type_(
          GetInitForcedEffectiveConnectionType(params_)),
      forced_effective_connection_type_on_cellular_only_(
          GetForcedEffectiveConnectionTypeOnCellularOnly(params_)),
      persistent_cache_reading_enabled_(
          GetPersistentCacheReadingEnabled(params_)),
      min_socket_watcher_notification_interval_(
          GetMinSocketWatcherNotificationInterval(params_)),
      upper_bound_http_rtt_endtoend_rtt_multiplier_(
          GetDoubleValueForVariationParamWithDefaultValue(
              params_,
              "upper_bound_http_rtt_endtoend_rtt_multiplier",
              3.0)),
      hanging_request_http_rtt_upper_bound_transport_rtt_multiplier_(
          GetValueForVariationParam(
              params_,
              "hanging_request_http_rtt_upper_bound_transport_rtt_multiplier",
              8)),
      hanging_request_http_rtt_upper_bound_http_rtt_multiplier_(
          GetValueForVariationParam(
              params_,
              "hanging_request_http_rtt_upper_bound_http_rtt_multiplier",
              6)),
      http_rtt_transport_rtt_min_count_(
          GetValueForVariationParam(params_,
                                    "http_rtt_transport_rtt_min_count",
                                    5)),
      increase_in_transport_rtt_logging_interval_(
          base::Milliseconds(GetDoubleValueForVariationParamWithDefaultValue(
              params_,
              "increase_in_transport_rtt_logging_interval",
              10000))),
      recent_time_threshold_(
          base::Milliseconds(GetDoubleValueForVariationParamWithDefaultValue(
              params_,
              "recent_time_threshold",
              5000))),
      historical_time_threshold_(
          base::Milliseconds(GetDoubleValueForVariationParamWithDefaultValue(
              params_,
              "historical_time_threshold",
              60000))),
      hanging_request_duration_http_rtt_multiplier_(GetValueForVariationParam(
          params_,
          "hanging_request_duration_http_rtt_multiplier",
          5)),
      add_default_platform_observations_(
          GetStringValueForVariationParamWithDefaultValue(
              params_,
              "add_default_platform_observations",
              "true") == "true"),
      count_new_observations_received_compute_ect_(
          features::kCountNewObservationsReceivedComputeEct.Get()),
      observation_buffer_size_(features::kObservationBufferSize.Get()),
      socket_watchers_min_notification_interval_(
          base::Milliseconds(GetValueForVariationParam(
              params_,
              "socket_watchers_min_notification_interval_msec",
              200))),
      upper_bound_typical_kbps_multiplier_(
          GetDoubleValueForVariationParamWithDefaultValue(
              params_,
              "upper_bound_typical_kbps_multiplier",
              3.5)),
      adjust_rtt_based_on_rtt_counts_(
          GetStringValueForVariationParamWithDefaultValue(
              params_,
              "adjust_rtt_based_on_rtt_counts",
              "false") == "true") {
  DCHECK(hanging_request_http_rtt_upper_bound_transport_rtt_multiplier_ == -1 ||
         hanging_request_http_rtt_upper_bound_transport_rtt_multiplier_ > 0);
  DCHECK(hanging_request_http_rtt_upper_bound_http_rtt_multiplier_ == -1 ||
         hanging_request_http_rtt_upper_bound_http_rtt_multiplier_ > 0);
  DCHECK(hanging_request_http_rtt_upper_bound_transport_rtt_multiplier_ == -1 ||
         hanging_request_http_rtt_upper_bound_http_rtt_multiplier_ == -1 ||
         hanging_request_http_rtt_upper_bound_transport_rtt_multiplier_ >=
             hanging_request_http_rtt_upper_bound_http_rtt_multiplier_);

  DCHECK_LT(0, hanging_request_duration_http_rtt_multiplier());
  DCHECK_LT(0, hanging_request_http_rtt_upper_bound_http_rtt_multiplier());
  DCHECK_LT(0, hanging_request_http_rtt_upper_bound_transport_rtt_multiplier());

  ObtainDefaultObservations(params_, default_observations_);
  ObtainTypicalNetworkQualities(params_, typical_network_quality_);
  ObtainConnectionThresholds(params_, connection_thresholds_);
}

NetworkQualityEstimatorParams::~NetworkQualityEstimatorParams() = default;

void NetworkQualityEstimatorParams::SetUseSmallResponsesForTesting(
    bool use_small_responses) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  use_small_responses_ = use_small_responses;
}

bool NetworkQualityEstimatorParams::use_small_responses() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return use_small_responses_;
}

// static
base::TimeDelta NetworkQualityEstimatorParams::GetDefaultTypicalHttpRtt(
    EffectiveConnectionType effective_connection_type) {
  return kTypicalHttpRttEffectiveConnectionType[effective_connection_type];
}

// static
int32_t NetworkQualityEstimatorParams::GetDefaultTypicalDownlinkKbps(
    EffectiveConnectionType effective_connection_type) {
  return kTypicalDownlinkKbpsEffectiveConnectionType[effective_connection_type];
}

void NetworkQualityEstimatorParams::SetForcedEffectiveConnectionTypeForTesting(
    EffectiveConnectionType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!forced_effective_connection_type_on_cellular_only_);
  forced_effective_connection_type_ = type;
}

std::optional<EffectiveConnectionType>
NetworkQualityEstimatorParams::GetForcedEffectiveConnectionType(
    NetworkChangeNotifier::ConnectionType connection_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (forced_effective_connection_type_) {
    return forced_effective_connection_type_;
  }

  if (forced_effective_connection_type_on_cellular_only_ &&
      net::NetworkChangeNotifier::IsConnectionCellular(connection_type)) {
    return EFFECTIVE_CONNECTION_TYPE_SLOW_2G;
  }
  return std::nullopt;
}

size_t NetworkQualityEstimatorParams::throughput_min_requests_in_flight()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If |use_small_responses_| is set to true for testing, then consider one
  // request as sufficient for taking throughput sample.
  return use_small_responses_ ? 1 : throughput_min_requests_in_flight_;
}

int64_t NetworkQualityEstimatorParams::GetThroughputMinTransferSizeBits()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return static_cast<int64_t>(throughput_min_transfer_size_kilobytes_) * 8 *
         1000;
}

const nqe::internal::NetworkQuality&
NetworkQualityEstimatorParams::DefaultObservation(
    NetworkChangeNotifier::ConnectionType type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return default_observations_[type];
}

const nqe::internal::NetworkQuality&
NetworkQualityEstimatorParams::TypicalNetworkQuality(
    EffectiveConnectionType type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return typical_network_quality_[type];
}

const nqe::internal::NetworkQuality&
NetworkQualityEstimatorParams::ConnectionThreshold(
    EffectiveConnectionType type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return connection_thresholds_[type];
}

}  // namespace net
