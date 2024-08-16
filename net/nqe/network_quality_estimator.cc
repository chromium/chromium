// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/nqe/network_quality_estimator.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/load_timing_info.h"
#include "net/base/network_interfaces.h"
#include "net/base/trace_constants.h"
#include "net/base/tracing.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_status_code.h"
#include "net/nqe/network_quality_estimator_util.h"
#include "net/nqe/throughput_analyzer.h"
#include "net/nqe/weighted_observation.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "url/gurl.h"

namespace net {

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
// SequencedTaskRunner to get the network id. A SequencedTaskRunner is used
// rather than parallel tasks to avoid having many threads getting the network
// id concurrently.
base::LazyThreadPoolSequencedTaskRunner g_get_network_id_task_runner =
    LAZY_THREAD_POOL_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::MayBlock(),
                         base::TaskPriority::BEST_EFFORT,
                         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN));
#endif

NetworkQualityObservationSource ProtocolSourceToObservationSource(
    SocketPerformanceWatcherFactory::Protocol protocol) {
  switch (protocol) {
    case SocketPerformanceWatcherFactory::PROTOCOL_TCP:
      return NETWORK_QUALITY_OBSERVATION_SOURCE_TCP;
    case SocketPerformanceWatcherFactory::PROTOCOL_QUIC:
      return NETWORK_QUALITY_OBSERVATION_SOURCE_QUIC;
  }
  NOTREACHED_IN_MIGRATION();
  return NETWORK_QUALITY_OBSERVATION_SOURCE_TCP;
}

// Returns true if the scheme of the |request| is either HTTP or HTTPS.
bool RequestSchemeIsHTTPOrHTTPS(const URLRequest& request) {
  return request.url().is_valid() && request.url().SchemeIsHTTPOrHTTPS();
}

nqe::internal::NetworkID DoGetCurrentNetworkID(
    NetworkQualityEstimatorParams* params) {
  nqe::internal::NetworkID network_id(
      NetworkChangeNotifier::GetConnectionType(), std::string(), INT32_MIN);

  return network_id;
}

const char* CategoryToString(nqe::internal::ObservationCategory category) {
  switch (category) {
    case nqe::internal::OBSERVATION_CATEGORY_HTTP:
      return "HTTP";
    case nqe::internal::OBSERVATION_CATEGORY_TRANSPORT:
      return "Transport";
    case nqe::internal::OBSERVATION_CATEGORY_END_TO_END:
      return "EndToEnd";
    case nqe::internal::OBSERVATION_CATEGORY_COUNT:
      NOTREACHED();
  }
}

base::TimeTicks GetStartTimeFromThreshold(int threshold) {
  if (threshold < 0) {
    return base::TimeTicks();
  }
  return base::TimeTicks::Now() - base::Seconds(threshold);
}

base::TimeTicks GetHTTPStartTime() {
  static const int threshold = features::kRecentHTTPThresholdInSeconds.Get();
  return GetStartTimeFromThreshold(threshold);
}

base::TimeTicks GetTransportStartTime() {
  static const int threshold =
      features::kRecentTransportThresholdInSeconds.Get();
  return GetStartTimeFromThreshold(threshold);
}

base::TimeTicks GetEndToEndStartTime() {
  static const int threshold =
      features::kRecentEndToEndThresholdInSeconds.Get();
  return GetStartTimeFromThreshold(threshold);
}

void RecordFallbackSuccess(std::string_view category, bool fallback_success) {
  base::UmaHistogramBoolean(
      base::StrCat({"NQE.RTT.HittingThreshold.", category, ".FallbackSuccess"}),
      fallback_success);
}

}  // namespace

NetworkQualityEstimator::NetworkQualityEstimator(
    std::unique_ptr<NetworkQualityEstimatorParams> params,
    NetLog* net_log)
    : params_(std::move(params)),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      last_connection_change_(tick_clock_->NowTicks()),
      current_network_id_(nqe::internal::NetworkID(
          NetworkChangeNotifier::ConnectionType::CONNECTION_UNKNOWN,
          std::string(),
          INT32_MIN)),
      http_downstream_throughput_kbps_observations_(
          params_.get(),
          tick_clock_,
          params_->weight_multiplier_per_second(),
          1.0 /*params_->weight_multiplier_per_signal_strength_level()*/),
      rtt_ms_observations_{
          ObservationBuffer(
              params_.get(),
              tick_clock_,
              params_->weight_multiplier_per_second(),
              1.0 /*params_->weight_multiplier_per_signal_strength_level()*/),
          ObservationBuffer(
              params_.get(),
              tick_clock_,
              params_->weight_multiplier_per_second(),
              1.0 /*params_->weight_multiplier_per_signal_strength_level()*/),
          ObservationBuffer(
              params_.get(),
              tick_clock_,
              params_->weight_multiplier_per_second(),
              1.0 /*params_->weight_multiplier_per_signal_strength_level()*/)},
      effective_connection_type_recomputation_interval_(
          features::kEffectiveConnectionTypeRecomputationInterval.Get()),
      net_log_(NetLogWithSource::Make(
          net_log,
          net::NetLogSourceType::NETWORK_QUALITY_ESTIMATOR)),
      event_creator_(net_log_) {
  DCHECK_EQ(nqe::internal::OBSERVATION_CATEGORY_COUNT,
            std::size(rtt_ms_observations_));

  network_quality_store_ =
      std::make_unique<nqe::internal::NetworkQualityStore>();
  NetworkChangeNotifier::AddConnectionTypeObserver(this);
  throughput_analyzer_ = std::make_unique<nqe::internal::ThroughputAnalyzer>(
      this, params_.get(), base::SingleThreadTaskRunner::GetCurrentDefault(),
      base::BindRepeating(
          &NetworkQualityEstimator::OnNewThroughputObservationAvailable,
          weak_ptr_factory_.GetWeakPtr()),
      tick_clock_, net_log_);

  watcher_factory_ = std::make_unique<nqe::internal::SocketWatcherFactory>(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      params_->min_socket_watcher_notification_interval(),
      // OnUpdatedTransportRTTAvailable() may be called via PostTask() by
      // socket watchers that live on a different thread than the current thread
      // (i.e., base::SingleThreadTaskRunner::GetCurrentDefault()).
      // Use WeakPtr() to avoid crashes where the socket watcher is destroyed
      // after |this| is destroyed.
      base::BindRepeating(
          &NetworkQualityEstimator::OnUpdatedTransportRTTAvailable,
          weak_ptr_factory_.GetWeakPtr()),
      // ShouldSocketWatcherNotifyRTT() below is called by only the socket
      // watchers that live on the same thread as the current thread
      // (i.e., base::SingleThreadTaskRunner::GetCurrentDefault()). Also,
      // network quality estimator is destroyed after network contexts and
      // URLRequestContexts. It's safe to use base::Unretained() below since the
      // socket watcher (owned by sockets) would be destroyed before |this|.
      base::BindRepeating(
          &NetworkQualityEstimator::ShouldSocketWatcherNotifyRTT,
          base::Unretained(this)),
      tick_clock_);

  GatherEstimatesForNextConnectionType();
}

void NetworkQualityEstimator::AddDefaultEstimates() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!params_->add_default_platform_observations())
    return;

  if (params_->DefaultObservation(current_network_id_.type).http_rtt() !=
      nqe::internal::InvalidRTT()) {
    Observation rtt_observation(
        params_->DefaultObservation(current_network_id_.type)
            .http_rtt()
            .InMilliseconds(),
        tick_clock_->NowTicks(), INT32_MIN,
        NETWORK_QUALITY_OBSERVATION_SOURCE_DEFAULT_HTTP_FROM_PLATFORM);
    AddAndNotifyObserversOfRTT(rtt_observation);
  }

  if (params_->DefaultObservation(current_network_id_.type).transport_rtt() !=
      nqe::internal::InvalidRTT()) {
    Observation rtt_observation(
        params_->DefaultObservation(current_network_id_.type)
            .transport_rtt()
            .InMilliseconds(),
        tick_clock_->NowTicks(), INT32_MIN,
        NETWORK_QUALITY_OBSERVATION_SOURCE_DEFAULT_TRANSPORT_FROM_PLATFORM);
    AddAndNotifyObserversOfRTT(rtt_observation);
  }

  if (params_->DefaultObservation(current_network_id_.type)
          .downstream_throughput_kbps() !=
      nqe::internal::INVALID_RTT_THROUGHPUT) {
    Observation throughput_observation(
        params_->DefaultObservation(current_network_id_.type)
            .downstream_throughput_kbps(),
        tick_clock_->NowTicks(), INT32_MIN,
        NETWORK_QUALITY_OBSERVATION_SOURCE_DEFAULT_HTTP_FROM_PLATFORM);
    AddAndNotifyObserversOfThroughput(throughput_observation);
  }
}

NetworkQualityEstimator::~NetworkQualityEstimator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
}

void NetworkQualityEstimator::NotifyStartTransaction(
    const URLRequest& request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!RequestSchemeIsHTTPOrHTTPS(request))
    return;

  // TODO(tbansal): Refactor this to a separate method.
  if (request.load_flags() & LOAD_MAIN_FRAME_DEPRECATED) {
    ComputeEffectiveConnectionType();
  } else {
    MaybeComputeEffectiveConnectionType();
  }
  throughput_analyzer_->NotifyStartTransaction(request);
}

bool NetworkQualityEstimator::IsHangingRequest(
    base::TimeDelta observed_http_rtt) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If there are sufficient number of end to end RTT samples available, use
  // the end to end RTT estimate to determine if the request is hanging.
  // If |observed_http_rtt| is within a fixed multiplier of |end_to_end_rtt_|,
  // then |observed_http_rtt| is determined to be not a hanging-request RTT.
  if (params_->use_end_to_end_rtt() && end_to_end_rtt_.has_value() &&
      end_to_end_rtt_observation_count_at_last_ect_computation_ >=
          params_->http_rtt_transport_rtt_min_count() &&
      params_->hanging_request_http_rtt_upper_bound_transport_rtt_multiplier() >
          0 &&
      observed_http_rtt <
          params_->hanging_request_http_rtt_upper_bound_transport_rtt_multiplier() *
              end_to_end_rtt_.value()) {
    return false;
  }

  DCHECK_LT(
      0,
      params_->hanging_request_http_rtt_upper_bound_transport_rtt_multiplier());

  if (transport_rtt_observation_count_last_ect_computation_ >=
          params_->http_rtt_transport_rtt_min_count() &&
      (observed_http_rtt <
       params_->hanging_request_http_rtt_upper_bound_transport_rtt_multiplier() *
           GetTransportRTT().value_or(base::Seconds(10)))) {
    // If there are sufficient number of transport RTT samples available, use
    // the transport RTT estimate to determine if the request is hanging.
    return false;
  }

  DCHECK_LT(
      0, params_->hanging_request_http_rtt_upper_bound_http_rtt_multiplier());

  if (observed_http_rtt <
      params_->hanging_request_http_rtt_upper_bound_http_rtt_multiplier() *
          GetHttpRTT().value_or(base::Seconds(10))) {
    // Use the HTTP RTT estimate to determine if the request is hanging.
    return false;
  }

  if (observed_http_rtt <=
      params_->hanging_request_upper_bound_min_http_rtt()) {
    return false;
  }
  return true;
}

void NetworkQualityEstimator::NotifyHeadersReceived(
    const URLRequest& request,
    int64_t prefilter_total_bytes_read) {
  TRACE_EVENT0(NetTracingCategory(),
               "NetworkQualityEstimator::NotifyHeadersReceived");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!RequestSchemeIsHTTPOrHTTPS(request) ||
      !RequestProvidesRTTObservation(request)) {
    return;
  }

  if (request.load_flags() & LOAD_MAIN_FRAME_DEPRECATED) {
    ComputeEffectiveConnectionType();
  }

  LoadTimingInfo load_timing_info;
  request.GetLoadTimingInfo(&load_timing_info);

  // If the load timing info is unavailable, it probably means that the request
  // did not go over the network.
  if (load_timing_info.send_start.is_null() ||
      load_timing_info.receive_headers_end.is_null()) {
    return;
  }
  DCHECK(!request.response_info().was_cached);

  // Duration between when the resource was requested and when the response
  // headers were received.
  const base::TimeDelta observed_http_rtt =
      load_timing_info.receive_headers_end - load_timing_info.send_start;
  if (observed_http_rtt <= base::TimeDelta())
    return;
  DCHECK_GE(observed_http_rtt, base::TimeDelta());

  if (IsHangingRequest(observed_http_rtt))
    return;

  // Metrics on estimation errors.
  const auto& estimated_rtt = GetHttpRTT();
  if (estimated_rtt) {
    const base::TimeDelta estimation_error = observed_http_rtt - *estimated_rtt;
    if (estimation_error.is_zero()) {
      base::UmaHistogramBoolean("NQE.RTT.Error.IsZero", true);
      base::UmaHistogramTimes("NQE.RTT.Error.Absolute", estimation_error);
    } else {
      base::UmaHistogramBoolean("NQE.RTT.Error.IsZero", false);
      if (estimation_error.is_positive()) {
        base::UmaHistogramTimes("NQE.RTT.Error.Positive", estimation_error);
        base::UmaHistogramTimes("NQE.RTT.Error.Absolute", estimation_error);
      } else {  // Negative.
        base::UmaHistogramTimes("NQE.RTT.Error.Negative", -estimation_error);
        base::UmaHistogramTimes("NQE.RTT.Error.Absolute", -estimation_error);
      }
    }
  }

  Observation http_rtt_observation(observed_http_rtt.InMilliseconds(),
                                   tick_clock_->NowTicks(),
                                   current_network_id_.signal_strength,
                                   NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP);
  AddAndNotifyObserversOfRTT(http_rtt_observation);
  throughput_analyzer_->NotifyBytesRead(request);
  throughput_analyzer_->NotifyExpectedResponseContentSize(
      request, request.GetExpectedContentSize());
}

void NetworkQualityEstimator::NotifyBytesRead(
    const URLRequest& request,
    int64_t prefilter_total_bytes_read) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  throughput_analyzer_->NotifyBytesRead(request);
}

void NetworkQualityEstimator::NotifyRequestCompleted(
    const URLRequest& request) {
  TRACE_EVENT0(NetTracingCategory(),
               "NetworkQualityEstimator::NotifyRequestCompleted");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!RequestSchemeIsHTTPOrHTTPS(request))
    return;

  throughput_analyzer_->NotifyRequestCompleted(request);
}

void NetworkQualityEstimator::NotifyURLRequestDestroyed(
    const URLRequest& request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!RequestSchemeIsHTTPOrHTTPS(request))
    return;

  throughput_analyzer_->NotifyRequestCompleted(request);
}

void NetworkQualityEstimator::AddRTTObserver(RTTObserver* rtt_observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  rtt_observer_list_.AddObserver(rtt_observer);
}

void NetworkQualityEstimator::RemoveRTTObserver(RTTObserver* rtt_observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  rtt_observer_list_.RemoveObserver(rtt_observer);
}

void NetworkQualityEstimator::AddThroughputObserver(
    ThroughputObserver* throughput_observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  throughput_observer_list_.AddObserver(throughput_observer);
}

void NetworkQualityEstimator::RemoveThroughputObserver(
    ThroughputObserver* throughput_observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  throughput_observer_list_.RemoveObserver(throughput_observer);
}

SocketPerformanceWatcherFactory*
NetworkQualityEstimator::GetSocketPerformanceWatcherFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return watcher_factory_.get();
}

void NetworkQualityEstimator::SetUseLocalHostRequestsForTesting(
    bool use_localhost_requests) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  use_localhost_requests_ = use_localhost_requests;
  watcher_factory_->SetUseLocalHostRequestsForTesting(use_localhost_requests_);
  throughput_analyzer_->SetUseLocalHostRequestsForTesting(
      use_localhost_requests_);
}

void NetworkQualityEstimator::SetUseSmallResponsesForTesting(
    bool use_small_responses) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  params_->SetUseSmallResponsesForTesting(use_small_responses);
}

void NetworkQualityEstimator::DisableOfflineCheckForTesting(
    bool disable_offline_check) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  disable_offline_check_ = disable_offline_check;
}

void NetworkQualityEstimator::ReportEffectiveConnectionTypeForTesting(
    EffectiveConnectionType effective_connection_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  event_creator_.MaybeAddNetworkQualityChangedEventToNetLog(
      effective_connection_type_,
      params_->TypicalNetworkQuality(effective_connection_type));

  for (auto& observer : effective_connection_type_observer_list_)
    observer.OnEffectiveConnectionTypeChanged(effective_connection_type);

  network_quality_store_->Add(current_network_id_,
                              nqe::internal::CachedNetworkQuality(
                                  tick_clock_->NowTicks(), network_quality_,
                                  effective_connection_type));
}

void NetworkQualityEstimator::ReportRTTsAndThroughputForTesting(
    base::TimeDelta http_rtt,
    base::TimeDelta transport_rtt,
    int32_t downstream_throughput_kbps) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& observer : rtt_and_throughput_estimates_observer_list_)
    observer.OnRTTOrThroughputEstimatesComputed(http_rtt, transport_rtt,
                                                downstream_throughput_kbps);
}

bool NetworkQualityEstimator::RequestProvidesRTTObservation(
    const URLRequest& request) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool private_network_request =
      nqe::internal::IsRequestForPrivateHost(request, net_log_);

  return (use_localhost_requests_ || !private_network_request) &&
         // Verify that response headers are received, so it can be ensured that
         // response is not cached.
         !request.response_info().response_time.is_null() &&
         !request.was_cached() &&
         request.creation_time() >= last_connection_change_ &&
         request.method() == "GET";
}

void NetworkQualityEstimator::OnConnectionTypeChanged(
    NetworkChangeNotifier::ConnectionType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // It's possible that |type| has the same value as |current_network_id_.type|.
  // This can happen if the device switches from one WiFi SSID to another.

  DCHECK_EQ(nqe::internal::OBSERVATION_CATEGORY_COUNT,
            std::size(rtt_ms_observations_));

  // Write the estimates of the previous network to the cache.
  network_quality_store_->Add(
      current_network_id_, nqe::internal::CachedNetworkQuality(
                               last_effective_connection_type_computation_,
                               network_quality_, effective_connection_type_));

  // Clear the local state.
  last_connection_change_ = tick_clock_->NowTicks();
  http_downstream_throughput_kbps_observations_.Clear();
  for (auto& rtt_ms_observation : rtt_ms_observations_)
    rtt_ms_observation.Clear();

  current_network_id_.signal_strength = INT32_MIN;
  network_quality_ = nqe::internal::NetworkQuality();
  end_to_end_rtt_ = std::nullopt;
  effective_connection_type_ = EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
  rtt_observations_size_at_last_ect_computation_ = 0;
  throughput_observations_size_at_last_ect_computation_ = 0;
  new_rtt_observations_since_last_ect_computation_ = 0;
  new_throughput_observations_since_last_ect_computation_ = 0;
  transport_rtt_observation_count_last_ect_computation_ = 0;
  end_to_end_rtt_observation_count_at_last_ect_computation_ = 0;
  last_socket_watcher_rtt_notification_ = base::TimeTicks();
  cached_estimate_applied_ = false;

  GatherEstimatesForNextConnectionType();
  throughput_analyzer_->OnConnectionTypeChanged();
}

void NetworkQualityEstimator::GatherEstimatesForNextConnectionType() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (get_network_id_asynchronously_) {
    // Doing PostTaskAndReplyWithResult by handle because it requires the result
    // type have a default constructor and nqe::internal::NetworkID does not
    // have that.
    g_get_network_id_task_runner.Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](scoped_refptr<base::TaskRunner> reply_task_runner,
               base::OnceCallback<void(const nqe::internal::NetworkID&)>
                   reply_callback) {
              reply_task_runner->PostTask(
                  FROM_HERE, base::BindOnce(std::move(reply_callback),
                                            DoGetCurrentNetworkID(nullptr)));
            },
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            base::BindOnce(&NetworkQualityEstimator::
                               ContinueGatherEstimatesForNextConnectionType,
                           weak_ptr_factory_.GetWeakPtr())));
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  ContinueGatherEstimatesForNextConnectionType(GetCurrentNetworkID());
}

void NetworkQualityEstimator::ContinueGatherEstimatesForNextConnectionType(
    const nqe::internal::NetworkID& network_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Update the local state as part of preparation for the new connection.
  current_network_id_ = network_id;

  // Read any cached estimates for the new network. If cached estimates are
  // unavailable, add the default estimates.
  if (!ReadCachedNetworkQualityEstimate())
    AddDefaultEstimates();

  ComputeEffectiveConnectionType();
}

void NetworkQualityEstimator::ComputeEffectiveConnectionType() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::TimeTicks now = tick_clock_->NowTicks();

  const EffectiveConnectionType past_type = effective_connection_type_;
  last_effective_connection_type_computation_ = now;

  base::TimeDelta http_rtt = nqe::internal::InvalidRTT();
  base::TimeDelta transport_rtt = nqe::internal::InvalidRTT();
  base::TimeDelta end_to_end_rtt = nqe::internal::InvalidRTT();
  int32_t downstream_throughput_kbps = nqe::internal::INVALID_RTT_THROUGHPUT;

  effective_connection_type_ = GetRecentEffectiveConnectionTypeUsingMetrics(
      &http_rtt, &transport_rtt, &end_to_end_rtt, &downstream_throughput_kbps,
      &transport_rtt_observation_count_last_ect_computation_,
      &end_to_end_rtt_observation_count_at_last_ect_computation_);

  network_quality_ = nqe::internal::NetworkQuality(http_rtt, transport_rtt,
                                                   downstream_throughput_kbps);
  ClampKbpsBasedOnEct();
  if (network_quality_.http_rtt() != nqe::internal::InvalidRTT()) {
    UMA_HISTOGRAM_TIMES("NQE.RTT.OnECTComputation",
                        network_quality_.http_rtt());
  }
  if (network_quality_.transport_rtt() != nqe::internal::InvalidRTT()) {
    base::UmaHistogramTimes("NQE.TransportRTT.OnECTComputation",
                            network_quality_.transport_rtt());
  }

  end_to_end_rtt_ = std::nullopt;
  if (end_to_end_rtt != nqe::internal::InvalidRTT()) {
    end_to_end_rtt_ = end_to_end_rtt;
  }

  NotifyObserversOfRTTOrThroughputComputed();

  if (past_type != effective_connection_type_)
    NotifyObserversOfEffectiveConnectionTypeChanged();

  event_creator_.MaybeAddNetworkQualityChangedEventToNetLog(
      effective_connection_type_, network_quality_);

  rtt_observations_size_at_last_ect_computation_ =
      rtt_ms_observations_[nqe::internal::OBSERVATION_CATEGORY_HTTP].Size() +
      rtt_ms_observations_[nqe::internal::OBSERVATION_CATEGORY_TRANSPORT]
          .Size();
  throughput_observations_size_at_last_ect_computation_ =
      http_downstream_throughput_kbps_observations_.Size();
  new_rtt_observations_since_last_ect_computation_ = 0;
  new_throughput_observations_since_last_ect_computation_ = 0;
}

std::optional<net::EffectiveConnectionType>
NetworkQualityEstimator::GetOverrideECT() const {
  return std::nullopt;
}

void NetworkQualityEstimator::ClampKbpsBasedOnEct() {
  // No need to clamp when ECT is unknown or if the connection speed is fast.
  if (effective_connection_type_ == EFFECTIVE_CONNECTION_TYPE_UNKNOWN ||
      effective_connection_type_ == EFFECTIVE_CONNECTION_TYPE_OFFLINE ||
      effective_connection_type_ == EFFECTIVE_CONNECTION_TYPE_4G) {
    return;
  }

  if (params_->upper_bound_typical_kbps_multiplier() <= 0.0)
    return;

  DCHECK_LT(0, params_->TypicalNetworkQuality(effective_connection_type_)
                   .downstream_throughput_kbps());
  // For a given ECT, upper bound on Kbps can't be less than the typical Kbps
  // for that ECT.
  DCHECK_LE(1.0, params_->upper_bound_typical_kbps_multiplier());

  DCHECK(effective_connection_type_ == EFFECTIVE_CONNECTION_TYPE_SLOW_2G ||
         effective_connection_type_ == EFFECTIVE_CONNECTION_TYPE_2G ||
         effective_connection_type_ == EFFECTIVE_CONNECTION_TYPE_3G);

  // Put an upper bound on Kbps.
  network_quality_.set_downstream_throughput_kbps(
      std::min(network_quality_.downstream_throughput_kbps(),
               static_cast<int>(
                   params_->TypicalNetworkQuality(effective_connection_type_)
                       .downstream_throughput_kbps() *
                   params_->upper_bound_typical_kbps_multiplier())));
}

void NetworkQualityEstimator::AdjustHttpRttBasedOnRTTCounts(
    base::TimeDelta* http_rtt) const {
  if (!params_->adjust_rtt_based_on_rtt_counts())
    return;

  // This is needed only when RTT from TCP sockets or
  // QUIC/H2 connections is unavailable.
  if (transport_rtt_observation_count_last_ect_computation_ >=
          params_->http_rtt_transport_rtt_min_count() ||
      end_to_end_rtt_observation_count_at_last_ect_computation_ >=
          params_->http_rtt_transport_rtt_min_count()) {
    return;
  }

  // We prefer to use the cached value if it's available and the network change
  // happened recently.
  base::TimeDelta time_since_connection_change =
      tick_clock_->NowTicks() - last_connection_change_;
  if (cached_estimate_applied_ &&
      time_since_connection_change <= base::Minutes(1)) {
    return;
  }

  // If there are not enough transport RTT samples, end-to-end RTT samples and
  // the cached estimates are unavailble/too stale, then the computed value of
  // HTTP RTT can't be trusted due to hanging GETs. In that case, return the
  // typical HTTP RTT for a fast connection.
  if (current_network_id_.type == net::NetworkChangeNotifier::CONNECTION_NONE) {
    return;
  }

  base::TimeDelta upper_bound_http_rtt =
      params_->TypicalNetworkQuality(net::EFFECTIVE_CONNECTION_TYPE_4G)
          .http_rtt();
  if (upper_bound_http_rtt > *http_rtt) {
    return;
  }

  DCHECK_LE(upper_bound_http_rtt, *http_rtt);
  *http_rtt = upper_bound_http_rtt;
}

EffectiveConnectionType NetworkQualityEstimator::GetEffectiveConnectionType()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::optional<net::EffectiveConnectionType> override_ect = GetOverrideECT();
  if (override_ect) {
    return override_ect.value();
  }
  return effective_connection_type_;
}

void NetworkQualityEstimator::UpdateHttpRttUsingAllRttValues(
    base::TimeDelta* http_rtt,
    const base::TimeDelta transport_rtt,
    const base::TimeDelta end_to_end_rtt) const {
  DCHECK(http_rtt);

  // Use transport RTT to clamp the lower bound on HTTP RTT.
  // To improve accuracy, the transport RTT estimate is used only when the
  // transport RTT estimate was computed using at least
  // |params_->http_rtt_transport_rtt_min_count()| observations.
  if (*http_rtt != nqe::internal::InvalidRTT() &&
      transport_rtt != nqe::internal::InvalidRTT() &&
      transport_rtt_observation_count_last_ect_computation_ >=
          params_->http_rtt_transport_rtt_min_count() &&
      params_->lower_bound_http_rtt_transport_rtt_multiplier() > 0) {
    *http_rtt =
        std::max(*http_rtt,
                 transport_rtt *
                     params_->lower_bound_http_rtt_transport_rtt_multiplier());
  }

  // Put lower bound on |http_rtt| using |end_to_end_rtt|.
  if (*http_rtt != nqe::internal::InvalidRTT() &&
      params_->use_end_to_end_rtt() &&
      end_to_end_rtt != nqe::internal::InvalidRTT() &&
      end_to_end_rtt_observation_count_at_last_ect_computation_ >=
          params_->http_rtt_transport_rtt_min_count() &&
      params_->lower_bound_http_rtt_transport_rtt_multiplier() > 0) {
    *http_rtt =
        std::max(*http_rtt,
                 end_to_end_rtt *
                     params_->lower_bound_http_rtt_transport_rtt_multiplier());
  }

  // Put upper bound on |http_rtt| using |end_to_end_rtt|.
  if (*http_rtt != nqe::internal::InvalidRTT() &&
      params_->use_end_to_end_rtt() &&
      end_to_end_rtt != nqe::internal::InvalidRTT() &&
      end_to_end_rtt_observation_count_at_last_ect_computation_ >=
          params_->http_rtt_transport_rtt_min_count() &&
      params_->upper_bound_http_rtt_endtoend_rtt_multiplier() > 0) {
    *http_rtt = std::min(
        *http_rtt, end_to_end_rtt *
                       params_->upper_bound_http_rtt_endtoend_rtt_multiplier());
  }

  // Put upper bound on |http_rtt| if there is not enough HTTP RTT samples
  // available.
  AdjustHttpRttBasedOnRTTCounts(http_rtt);
}

EffectiveConnectionType
NetworkQualityEstimator::GetRecentEffectiveConnectionTypeUsingMetrics(
    base::TimeDelta* http_rtt,
    base::TimeDelta* transport_rtt,
    base::TimeDelta* end_to_end_rtt,
    int32_t* downstream_throughput_kbps,
    size_t* transport_rtt_observation_count,
    size_t* end_to_end_rtt_observation_count) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  *http_rtt = nqe::internal::InvalidRTT();
  *transport_rtt = nqe::internal::InvalidRTT();
  *end_to_end_rtt = nqe::internal::InvalidRTT();
  *downstream_throughput_kbps = nqe::internal::INVALID_RTT_THROUGHPUT;

  auto forced_ect =
      params_->GetForcedEffectiveConnectionType(current_network_id_.type);
  if (forced_ect) {
    *http_rtt = params_->TypicalNetworkQuality(forced_ect.value()).http_rtt();
    *transport_rtt =
        params_->TypicalNetworkQuality(forced_ect.value()).transport_rtt();
    *downstream_throughput_kbps =
        params_->TypicalNetworkQuality(forced_ect.value())
            .downstream_throughput_kbps();
    return forced_ect.value();
  }

  // If the device is currently offline, then return
  // EFFECTIVE_CONNECTION_TYPE_OFFLINE.
  if (current_network_id_.type == NetworkChangeNotifier::CONNECTION_NONE &&
      !disable_offline_check_) {
    return EFFECTIVE_CONNECTION_TYPE_OFFLINE;
  }

  if (force_report_wifi_as_slow_2g_for_testing_ &&
      current_network_id_.type == NetworkChangeNotifier::CONNECTION_WIFI) {
    return EFFECTIVE_CONNECTION_TYPE_SLOW_2G;
  }

  if (!GetRecentRTT(nqe::internal::OBSERVATION_CATEGORY_HTTP,
                    GetHTTPStartTime(), http_rtt, nullptr)) {
    bool fallback_success = true;
    if (!GetRecentRTT(nqe::internal::OBSERVATION_CATEGORY_HTTP,
                      base::TimeTicks(), http_rtt, nullptr)) {
      *http_rtt = nqe::internal::InvalidRTT();
      fallback_success = false;
    }
    RecordFallbackSuccess("HTTP", fallback_success);
  }

  if (!GetRecentRTT(nqe::internal::OBSERVATION_CATEGORY_TRANSPORT,
                    GetTransportStartTime(), transport_rtt,
                    transport_rtt_observation_count)) {
    bool fallback_success = true;
    if (!GetRecentRTT(nqe::internal::OBSERVATION_CATEGORY_TRANSPORT,
                      base::TimeTicks(), transport_rtt,
                      transport_rtt_observation_count)) {
      *transport_rtt = nqe::internal::InvalidRTT();
      fallback_success = false;
    }
    RecordFallbackSuccess("Transport", fallback_success);
  }

  if (!GetRecentRTT(nqe::internal::OBSERVATION_CATEGORY_END_TO_END,
                    GetEndToEndStartTime(), end_to_end_rtt,
                    end_to_end_rtt_observation_count)) {
    bool fallback_success = true;
    if (!GetRecentRTT(nqe::internal::OBSERVATION_CATEGORY_END_TO_END,
                      base::TimeTicks(), end_to_end_rtt,
                      end_to_end_rtt_observation_count)) {
      *end_to_end_rtt = nqe::internal::InvalidRTT();
      fallback_success = false;
    }
    RecordFallbackSuccess("EndToEnd", fallback_success);
  }

  UpdateHttpRttUsingAllRttValues(http_rtt, *transport_rtt, *end_to_end_rtt);

  if (!GetRecentDownlinkThroughputKbps(base::TimeTicks(),
                                       downstream_throughput_kbps)) {
    *downstream_throughput_kbps = nqe::internal::INVALID_RTT_THROUGHPUT;
  }

  if (*http_rtt == nqe::internal::InvalidRTT()) {
    return EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
  }

  if (*http_rtt == nqe::internal::InvalidRTT() &&
      *transport_rtt == nqe::internal::InvalidRTT() &&
      *downstream_throughput_kbps == nqe::internal::INVALID_RTT_THROUGHPUT) {
    // None of the metrics are available.
    return EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
  }

  // Search from the slowest connection type to the fastest to find the
  // EffectiveConnectionType that best matches the current connection's
  // performance. The match is done by comparing RTT and throughput.
  for (size_t i = 0; i < EFFECTIVE_CONNECTION_TYPE_LAST; ++i) {
    EffectiveConnectionType type = static_cast<EffectiveConnectionType>(i);
    if (i == EFFECTIVE_CONNECTION_TYPE_UNKNOWN)
      continue;

    const bool estimated_http_rtt_is_higher_than_threshold =
        *http_rtt != nqe::internal::InvalidRTT() &&
        params_->ConnectionThreshold(type).http_rtt() !=
            nqe::internal::InvalidRTT() &&
        *http_rtt >= params_->ConnectionThreshold(type).http_rtt();

    if (estimated_http_rtt_is_higher_than_threshold)
      return type;
  }
  // Return the fastest connection type.
  return static_cast<EffectiveConnectionType>(EFFECTIVE_CONNECTION_TYPE_LAST -
                                              1);
}

void NetworkQualityEstimator::AddEffectiveConnectionTypeObserver(
    EffectiveConnectionTypeObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observer);
  effective_connection_type_observer_list_.AddObserver(observer);

  // Notify the |observer| on the next message pump since |observer| may not
  // be completely set up for receiving the callbacks.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&NetworkQualityEstimator::
                         NotifyEffectiveConnectionTypeObserverIfPresent,
                     weak_ptr_factory_.GetWeakPtr(),
                     // This is safe as `handle` is checked against a map to
                     // verify it hasn't been removed before dereferencing.
                     base::UnsafeDangling(observer)));
}

void NetworkQualityEstimator::RemoveEffectiveConnectionTypeObserver(
    EffectiveConnectionTypeObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  effective_connection_type_observer_list_.RemoveObserver(observer);
}

void NetworkQualityEstimator::AddPeerToPeerConnectionsCountObserver(
    PeerToPeerConnectionsCountObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observer);
  peer_to_peer_type_observer_list_.AddObserver(observer);

  // Notify the |observer| on the next message pump since |observer| may not
  // be completely set up for receiving the callbacks.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&NetworkQualityEstimator::
                         NotifyPeerToPeerConnectionsCountObserverIfPresent,
                     weak_ptr_factory_.GetWeakPtr(),
                     // This is safe as `handle` is checked against a map to
                     // verify it hasn't been removed before dereferencing.
                     base::UnsafeDangling(observer)));
}

void NetworkQualityEstimator::RemovePeerToPeerConnectionsCountObserver(
    PeerToPeerConnectionsCountObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  peer_to_peer_type_observer_list_.RemoveObserver(observer);
}

void NetworkQualityEstimator::AddRTTAndThroughputEstimatesObserver(
    RTTAndThroughputEstimatesObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observer);
  rtt_and_throughput_estimates_observer_list_.AddObserver(observer);

  // Notify the |observer| on the next message pump since |observer| may not
  // be completely set up for receiving the callbacks.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&NetworkQualityEstimator::
                         NotifyRTTAndThroughputEstimatesObserverIfPresent,
                     weak_ptr_factory_.GetWeakPtr(), observer));
}

void NetworkQualityEstimator::RemoveRTTAndThroughputEstimatesObserver(
    RTTAndThroughputEstimatesObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  rtt_and_throughput_estimates_observer_list_.RemoveObserver(observer);
}

bool NetworkQualityEstimator::GetRecentRTT(
    nqe::internal::ObservationCategory observation_category,
    const base::TimeTicks& start_time,
    base::TimeDelta* rtt,
    size_t* observations_count) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  *rtt = GetRTTEstimateInternal(start_time, observation_category, 50,
                                observations_count);
  return (*rtt != nqe::internal::InvalidRTT());
}

bool NetworkQualityEstimator::GetRecentDownlinkThroughputKbps(
    const base::TimeTicks& start_time,
    int32_t* kbps) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  *kbps = GetDownlinkThroughputKbpsEstimateInternal(start_time, 50);
  return (*kbps != nqe::internal::INVALID_RTT_THROUGHPUT);
}

base::TimeDelta NetworkQualityEstimator::GetRTTEstimateInternal(
    base::TimeTicks start_time,
    nqe::internal::ObservationCategory observation_category,
    int percentile,
    size_t* observations_count) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(nqe::internal::OBSERVATION_CATEGORY_COUNT,
            std::size(rtt_ms_observations_));

  // RTT observations are sorted by duration from shortest to longest, thus
  // a higher percentile RTT will have a longer RTT than a lower percentile.
  switch (observation_category) {
    case nqe::internal::OBSERVATION_CATEGORY_HTTP:
    case nqe::internal::OBSERVATION_CATEGORY_TRANSPORT:
    case nqe::internal::OBSERVATION_CATEGORY_END_TO_END:
      return base::Milliseconds(
          rtt_ms_observations_[observation_category]
              .GetPercentile(start_time, current_network_id_.signal_strength,
                             percentile, observations_count)
              .value_or(nqe::internal::INVALID_RTT_THROUGHPUT));
    case nqe::internal::OBSERVATION_CATEGORY_COUNT:
      NOTREACHED_IN_MIGRATION();
      return base::TimeDelta();
  }
}

int32_t NetworkQualityEstimator::GetDownlinkThroughputKbpsEstimateInternal(
    const base::TimeTicks& start_time,
    int percentile) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Throughput observations are sorted by kbps from slowest to fastest,
  // thus a higher percentile throughput will be faster than a lower one.
  return http_downstream_throughput_kbps_observations_
      .GetPercentile(start_time, current_network_id_.signal_strength,
                     100 - percentile, nullptr)
      .value_or(nqe::internal::INVALID_RTT_THROUGHPUT);
}

nqe::internal::NetworkID NetworkQualityEstimator::GetCurrentNetworkID() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(tbansal): crbug.com/498068 Add NetworkQualityEstimatorAndroid class
  // that overrides this method on the Android platform.

  return DoGetCurrentNetworkID(params_.get());
}

bool NetworkQualityEstimator::ReadCachedNetworkQualityEstimate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!params_->persistent_cache_reading_enabled())
    return false;

  nqe::internal::CachedNetworkQuality cached_network_quality;

  const bool cached_estimate_available = network_quality_store_->GetById(
      current_network_id_, &cached_network_quality);

  if (!cached_estimate_available) {
    return false;
  }

  EffectiveConnectionType effective_connection_type =
      cached_network_quality.effective_connection_type();

  if (effective_connection_type == EFFECTIVE_CONNECTION_TYPE_UNKNOWN ||
      effective_connection_type == EFFECTIVE_CONNECTION_TYPE_OFFLINE ||
      effective_connection_type == EFFECTIVE_CONNECTION_TYPE_LAST) {
    return false;
  }

  nqe::internal::NetworkQuality network_quality =
      cached_network_quality.network_quality();

  bool update_network_quality_store = false;

  // Populate |network_quality| with synthetic RTT and throughput observations
  // if they are missing.
  if (network_quality.http_rtt().InMilliseconds() ==
      nqe::internal::INVALID_RTT_THROUGHPUT) {
    network_quality.set_http_rtt(
        params_->TypicalNetworkQuality(effective_connection_type).http_rtt());
    update_network_quality_store = true;
  }

  if (network_quality.transport_rtt().InMilliseconds() ==
      nqe::internal::INVALID_RTT_THROUGHPUT) {
    network_quality.set_transport_rtt(
        params_->TypicalNetworkQuality(effective_connection_type)
            .transport_rtt());
    update_network_quality_store = true;
  }

  if (network_quality.downstream_throughput_kbps() ==
      nqe::internal::INVALID_RTT_THROUGHPUT) {
    network_quality.set_downstream_throughput_kbps(
        params_->TypicalNetworkQuality(effective_connection_type)
            .downstream_throughput_kbps());
    update_network_quality_store = true;
  }

  if (update_network_quality_store) {
    network_quality_store_->Add(current_network_id_,
                                nqe::internal::CachedNetworkQuality(
                                    tick_clock_->NowTicks(), network_quality,
                                    effective_connection_type));
  }

  Observation http_rtt_observation(
      network_quality.http_rtt().InMilliseconds(), tick_clock_->NowTicks(),
      INT32_MIN, NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP_CACHED_ESTIMATE);
  AddAndNotifyObserversOfRTT(http_rtt_observation);

  Observation transport_rtt_observation(
      network_quality.transport_rtt().InMilliseconds(), tick_clock_->NowTicks(),
      INT32_MIN, NETWORK_QUALITY_OBSERVATION_SOURCE_TRANSPORT_CACHED_ESTIMATE);
  AddAndNotifyObserversOfRTT(transport_rtt_observation);

  Observation througphput_observation(
      network_quality.downstream_throughput_kbps(), tick_clock_->NowTicks(),
      INT32_MIN, NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP_CACHED_ESTIMATE);
  AddAndNotifyObserversOfThroughput(througphput_observation);

  ComputeEffectiveConnectionType();
  return true;
}

void NetworkQualityEstimator::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  tick_clock_ = tick_clock;
  for (auto& rtt_ms_observation : rtt_ms_observations_)
    rtt_ms_observation.SetTickClockForTesting(tick_clock_);  // IN-TEST
  http_downstream_throughput_kbps_observations_.SetTickClockForTesting(
      tick_clock_);
  throughput_analyzer_->SetTickClockForTesting(tick_clock_);
  watcher_factory_->SetTickClockForTesting(tick_clock_);
}

void NetworkQualityEstimator::OnUpdatedTransportRTTAvailable(
    SocketPerformanceWatcherFactory::Protocol protocol,
    const base::TimeDelta& rtt,
    const std::optional<nqe::internal::IPHash>& host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_LT(nqe::internal::INVALID_RTT_THROUGHPUT, rtt.InMilliseconds());
  Observation observation(rtt.InMilliseconds(), tick_clock_->NowTicks(),
                          current_network_id_.signal_strength,
                          ProtocolSourceToObservationSource(protocol), host);
  AddAndNotifyObserversOfRTT(observation);
}

void NetworkQualityEstimator::AddAndNotifyObserversOfRTT(
    const Observation& observation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(nqe::internal::InvalidRTT(),
            base::Milliseconds(observation.value()));
  DCHECK_GT(NETWORK_QUALITY_OBSERVATION_SOURCE_MAX, observation.source());

  if (!ShouldAddObservation(observation))
    return;

  MaybeUpdateCachedEstimateApplied(
      observation,
      &rtt_ms_observations_[nqe::internal::OBSERVATION_CATEGORY_HTTP]);
  MaybeUpdateCachedEstimateApplied(
      observation,
      &rtt_ms_observations_[nqe::internal::OBSERVATION_CATEGORY_TRANSPORT]);
  ++new_rtt_observations_since_last_ect_computation_;

  std::vector<nqe::internal::ObservationCategory> observation_categories =
      observation.GetObservationCategories();
  for (nqe::internal::ObservationCategory observation_category :
       observation_categories) {
    auto evicted =
        rtt_ms_observations_[observation_category].AddObservation(observation);
    if (evicted) {
      auto delta = base::TimeTicks::Now() - evicted->timestamp();
      base::UmaHistogramLongTimes100(
          base::StrCat({"NQE.RTT.ObservationBufferLifeTime2.",
                        CategoryToString(observation_category)}),
          delta);
      base::UmaHistogramLongTimes100("NQE.RTT.ObservationBufferLifeTime2.All",
                                     delta);
    }
  }

  if (observation.source() == NETWORK_QUALITY_OBSERVATION_SOURCE_TCP ||
      observation.source() == NETWORK_QUALITY_OBSERVATION_SOURCE_QUIC) {
    last_socket_watcher_rtt_notification_ = tick_clock_->NowTicks();
  }

  UMA_HISTOGRAM_ENUMERATION("NQE.RTT.ObservationSource", observation.source(),
                            NETWORK_QUALITY_OBSERVATION_SOURCE_MAX);

  // Maybe recompute the effective connection type since a new RTT observation
  // is available.
  if (observation.source() !=
          NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP_CACHED_ESTIMATE &&
      observation.source() !=
          NETWORK_QUALITY_OBSERVATION_SOURCE_TRANSPORT_CACHED_ESTIMATE) {
    MaybeComputeEffectiveConnectionType();
  }
  for (auto& observer : rtt_observer_list_) {
    observer.OnRTTObservation(observation.value(), observation.timestamp(),
                              observation.source());
  }
}

void NetworkQualityEstimator::AddAndNotifyObserversOfThroughput(
    const Observation& observation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(nqe::internal::INVALID_RTT_THROUGHPUT, observation.value());
  DCHECK_GT(NETWORK_QUALITY_OBSERVATION_SOURCE_MAX, observation.source());
  DCHECK_EQ(1u, observation.GetObservationCategories().size());
  DCHECK_EQ(nqe::internal::OBSERVATION_CATEGORY_HTTP,
            observation.GetObservationCategories().front());

  if (!ShouldAddObservation(observation))
    return;

  MaybeUpdateCachedEstimateApplied(
      observation, &http_downstream_throughput_kbps_observations_);
  ++new_throughput_observations_since_last_ect_computation_;
  http_downstream_throughput_kbps_observations_.AddObservation(observation);

  // Maybe recompute the effective connection type since a new throughput
  // observation is available.
  if (observation.source() !=
          NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP_CACHED_ESTIMATE &&
      observation.source() !=
          NETWORK_QUALITY_OBSERVATION_SOURCE_TRANSPORT_CACHED_ESTIMATE) {
    MaybeComputeEffectiveConnectionType();
  }
  for (auto& observer : throughput_observer_list_) {
    observer.OnThroughputObservation(
        observation.value(), observation.timestamp(), observation.source());
  }
}

void NetworkQualityEstimator::OnNewThroughputObservationAvailable(
    int32_t downstream_kbps) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (downstream_kbps <= 0)
    return;

  DCHECK_NE(nqe::internal::INVALID_RTT_THROUGHPUT, downstream_kbps);

  Observation throughput_observation(downstream_kbps, tick_clock_->NowTicks(),
                                     current_network_id_.signal_strength,
                                     NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP);
  AddAndNotifyObserversOfThroughput(throughput_observation);
}

bool NetworkQualityEstimator::ShouldComputeEffectiveConnectionType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(nqe::internal::OBSERVATION_CATEGORY_COUNT,
            std::size(rtt_ms_observations_));

  const base::TimeTicks now = tick_clock_->NowTicks();
  // Recompute effective connection type only if
  // |effective_connection_type_recomputation_interval_| has passed since it was
  // last computed or a connection change event was observed since the last
  // computation. Strict inequalities are used to ensure that effective
  // connection type is recomputed on connection change events even if the clock
  // has not updated.
  if (now - last_effective_connection_type_computation_ >=
      effective_connection_type_recomputation_interval_) {
    return true;
  }

  if (last_connection_change_ >= last_effective_connection_type_computation_) {
    return true;
  }

  // Recompute the effective connection type if the previously computed
  // effective connection type was unknown.
  if (effective_connection_type_ == EFFECTIVE_CONNECTION_TYPE_UNKNOWN) {
    return true;
  }

  // Recompute the effective connection type if the number of samples
  // available now are 50% more than the number of samples that were
  // available when the effective connection type was last computed.
  if (rtt_observations_size_at_last_ect_computation_ * 1.5 <
      (rtt_ms_observations_[nqe::internal::OBSERVATION_CATEGORY_HTTP].Size() +
       rtt_ms_observations_[nqe::internal::OBSERVATION_CATEGORY_TRANSPORT]
           .Size())) {
    return true;
  }

  if (throughput_observations_size_at_last_ect_computation_ * 1.5 <
      http_downstream_throughput_kbps_observations_.Size()) {
    return true;
  }

  if ((new_rtt_observations_since_last_ect_computation_ +
       new_throughput_observations_since_last_ect_computation_) >=
      params_->count_new_observations_received_compute_ect()) {
    return true;
  }
  return false;
}

void NetworkQualityEstimator::MaybeComputeEffectiveConnectionType() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!ShouldComputeEffectiveConnectionType())
    return;
  ComputeEffectiveConnectionType();
}

void NetworkQualityEstimator::
    NotifyObserversOfEffectiveConnectionTypeChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(EFFECTIVE_CONNECTION_TYPE_LAST, effective_connection_type_);

  std::optional<net::EffectiveConnectionType> override_ect = GetOverrideECT();

  // TODO(tbansal): Add hysteresis in the notification.
  for (auto& observer : effective_connection_type_observer_list_)
    observer.OnEffectiveConnectionTypeChanged(
        override_ect ? override_ect.value() : effective_connection_type_);
  // Add the estimates of the current network to the cache store.
  network_quality_store_->Add(current_network_id_,
                              nqe::internal::CachedNetworkQuality(
                                  tick_clock_->NowTicks(), network_quality_,
                                  effective_connection_type_));
}

void NetworkQualityEstimator::NotifyObserversOfRTTOrThroughputComputed() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(tbansal): Add hysteresis in the notification.
  for (auto& observer : rtt_and_throughput_estimates_observer_list_) {
    observer.OnRTTOrThroughputEstimatesComputed(
        network_quality_.http_rtt(), network_quality_.transport_rtt(),
        network_quality_.downstream_throughput_kbps());
  }
}

void NetworkQualityEstimator::NotifyEffectiveConnectionTypeObserverIfPresent(
    MayBeDangling<EffectiveConnectionTypeObserver> observer) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!effective_connection_type_observer_list_.HasObserver(observer))
    return;

  std::optional<net::EffectiveConnectionType> override_ect = GetOverrideECT();
  if (override_ect) {
    observer->OnEffectiveConnectionTypeChanged(override_ect.value());
    return;
  }
  if (effective_connection_type_ == EFFECTIVE_CONNECTION_TYPE_UNKNOWN)
    return;
  observer->OnEffectiveConnectionTypeChanged(effective_connection_type_);
}

void NetworkQualityEstimator::NotifyPeerToPeerConnectionsCountObserverIfPresent(
    MayBeDangling<PeerToPeerConnectionsCountObserver> observer) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!peer_to_peer_type_observer_list_.HasObserver(observer))
    return;
  observer->OnPeerToPeerConnectionsCountChange(p2p_connections_count_);
}

void NetworkQualityEstimator::NotifyRTTAndThroughputEstimatesObserverIfPresent(
    RTTAndThroughputEstimatesObserver* observer) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!rtt_and_throughput_estimates_observer_list_.HasObserver(observer))
    return;
  observer->OnRTTOrThroughputEstimatesComputed(
      network_quality_.http_rtt(), network_quality_.transport_rtt(),
      network_quality_.downstream_throughput_kbps());
}

void NetworkQualityEstimator::AddNetworkQualitiesCacheObserver(
    nqe::internal::NetworkQualityStore::NetworkQualitiesCacheObserver*
        observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  network_quality_store_->AddNetworkQualitiesCacheObserver(observer);
}

void NetworkQualityEstimator::RemoveNetworkQualitiesCacheObserver(
    nqe::internal::NetworkQualityStore::NetworkQualitiesCacheObserver*
        observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  network_quality_store_->RemoveNetworkQualitiesCacheObserver(observer);
}

void NetworkQualityEstimator::OnPrefsRead(
    const std::map<nqe::internal::NetworkID,
                   nqe::internal::CachedNetworkQuality> read_prefs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& it : read_prefs) {
    EffectiveConnectionType effective_connection_type =
        it.second.effective_connection_type();
    if (effective_connection_type == EFFECTIVE_CONNECTION_TYPE_UNKNOWN ||
        effective_connection_type == EFFECTIVE_CONNECTION_TYPE_OFFLINE) {
      continue;
    }

    // RTT and throughput values are not set in the prefs.
    DCHECK_EQ(nqe::internal::InvalidRTT(),
              it.second.network_quality().http_rtt());
    DCHECK_EQ(nqe::internal::InvalidRTT(),
              it.second.network_quality().transport_rtt());
    DCHECK_EQ(nqe::internal::INVALID_RTT_THROUGHPUT,
              it.second.network_quality().downstream_throughput_kbps());

    nqe::internal::CachedNetworkQuality cached_network_quality(
        tick_clock_->NowTicks(),
        params_->TypicalNetworkQuality(effective_connection_type),
        effective_connection_type);

    network_quality_store_->Add(it.first, cached_network_quality);
  }
  ReadCachedNetworkQualityEstimate();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void NetworkQualityEstimator::EnableGetNetworkIdAsynchronously() {
  get_network_id_asynchronously_ = true;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

std::optional<base::TimeDelta> NetworkQualityEstimator::GetHttpRTT() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (network_quality_.http_rtt() == nqe::internal::InvalidRTT())
    return std::optional<base::TimeDelta>();
  return network_quality_.http_rtt();
}

std::optional<base::TimeDelta> NetworkQualityEstimator::GetTransportRTT()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (network_quality_.transport_rtt() == nqe::internal::InvalidRTT())
    return std::optional<base::TimeDelta>();
  return network_quality_.transport_rtt();
}

std::optional<int32_t> NetworkQualityEstimator::GetDownstreamThroughputKbps()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (network_quality_.downstream_throughput_kbps() ==
      nqe::internal::INVALID_RTT_THROUGHPUT) {
    return std::optional<int32_t>();
  }
  return network_quality_.downstream_throughput_kbps();
}

void NetworkQualityEstimator::MaybeUpdateCachedEstimateApplied(
    const Observation& observation,
    ObservationBuffer* buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (observation.source() !=
          NETWORK_QUALITY_OBSERVATION_SOURCE_HTTP_CACHED_ESTIMATE &&
      observation.source() !=
          NETWORK_QUALITY_OBSERVATION_SOURCE_TRANSPORT_CACHED_ESTIMATE) {
    return;
  }

  cached_estimate_applied_ = true;
  bool deleted_observation_sources[NETWORK_QUALITY_OBSERVATION_SOURCE_MAX] = {
      false};
  deleted_observation_sources
      [NETWORK_QUALITY_OBSERVATION_SOURCE_DEFAULT_HTTP_FROM_PLATFORM] = true;
  deleted_observation_sources
      [NETWORK_QUALITY_OBSERVATION_SOURCE_DEFAULT_TRANSPORT_FROM_PLATFORM] =
          true;

  buffer->RemoveObservationsWithSource(deleted_observation_sources);
}

bool NetworkQualityEstimator::ShouldAddObservation(
    const Observation& observation) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (cached_estimate_applied_ &&
      (observation.source() ==
           NETWORK_QUALITY_OBSERVATION_SOURCE_DEFAULT_HTTP_FROM_PLATFORM ||
       observation.source() ==
           NETWORK_QUALITY_OBSERVATION_SOURCE_DEFAULT_TRANSPORT_FROM_PLATFORM)) {
    return false;
  }
  return true;
}

bool NetworkQualityEstimator::ShouldSocketWatcherNotifyRTT(
    base::TimeTicks now) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return (now - last_socket_watcher_rtt_notification_ >=
          params_->socket_watchers_min_notification_interval());
}

void NetworkQualityEstimator::SimulateNetworkQualityChangeForTesting(
    net::EffectiveConnectionType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  params_->SetForcedEffectiveConnectionTypeForTesting(type);
  ComputeEffectiveConnectionType();
}

void NetworkQualityEstimator::ForceReportWifiAsSlow2GForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  force_report_wifi_as_slow_2g_for_testing_ = true;
}

void NetworkQualityEstimator::RecordSpdyPingLatency(
    const HostPortPair& host_port_pair,
    base::TimeDelta rtt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_LT(nqe::internal::INVALID_RTT_THROUGHPUT, rtt.InMilliseconds());

  Observation observation(rtt.InMilliseconds(), tick_clock_->NowTicks(),
                          current_network_id_.signal_strength,
                          NETWORK_QUALITY_OBSERVATION_SOURCE_H2_PINGS);
  AddAndNotifyObserversOfRTT(observation);
}

void NetworkQualityEstimator::OnPeerToPeerConnectionsCountChange(
    uint32_t count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (p2p_connections_count_ == count)
    return;

  p2p_connections_count_ = count;

  for (auto& observer : peer_to_peer_type_observer_list_) {
    observer.OnPeerToPeerConnectionsCountChange(p2p_connections_count_);
  }
}

uint32_t NetworkQualityEstimator::GetPeerToPeerConnectionsCountChange() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return p2p_connections_count_;
}

}  // namespace net
