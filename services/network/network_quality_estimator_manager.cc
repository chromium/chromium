// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_quality_estimator_manager.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "net/base/features.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "services/network/public/cpp/network_switches.h"

namespace {

// Returns true if |past_value| is significantly different from |current_value|.
// Metric changed meaningfully only if (i) the difference between the two
// values exceed the threshold |min_difference_in_metrics|; and,
// (ii) the ratio of the values also exceeds the threshold |min_ratio|.
bool MetricChangedMeaningfully(int32_t past_value,
                               int32_t current_value,
                               int min_difference_in_metrics,
                               float min_ratio) {
  // A negative value indicates that the value of the corresponding metric is
  // unavailable. A difference in signature between the |past_value| and
  // |current_value| indicates change in the availability of the value of that
  // metric.
  if (past_value < 0 && current_value >= 0)
    return true;

  if (past_value >= 0 && current_value < 0)
    return true;

  if (past_value < 0 && current_value < 0)
    return false;

  if (std::abs(past_value - current_value) < min_difference_in_metrics) {
    // The absolute change in the value is not sufficient enough.
    return false;
  }

  if (past_value < (min_ratio * current_value) &&
      current_value < (min_ratio * past_value)) {
    // The relative change in the value is not sufficient enough.
    return false;
  }

  return true;
}

bool LatencyMetricChangedMeaningfully(int32_t past_value,
                                      int32_t current_value) {
  return MetricChangedMeaningfully(past_value, current_value,
                                   /*min_difference_in_metrics=*/20,
                                   /*min_ratio=*/1.2f);
}

bool BandwidthMetricChangedMeaningfully(int32_t past_value,
                                        int32_t current_value) {
  return MetricChangedMeaningfully(past_value, current_value,
                                   /*min_difference_in_metrics=*/100,
                                   /*min_ratio=*/1.2f);
}
}  // namespace

namespace network {

NetworkQualityEstimatorManager::NetworkQualityEstimatorManager(
    net::NetLog* net_log) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  std::map<std::string, std::string> network_quality_estimator_params;
  base::GetFieldTrialParamsByFeature(net::features::kNetworkQualityEstimator,
                                     &network_quality_estimator_params);

  if (command_line->HasSwitch(switches::kForceEffectiveConnectionType)) {
    const std::string force_ect_value = command_line->GetSwitchValueASCII(
        switches::kForceEffectiveConnectionType);

    if (!force_ect_value.empty()) {
      // If the effective connection type is forced using command line switch,
      // it overrides the one set by field trial.
      network_quality_estimator_params[net::kForceEffectiveConnectionType] =
          force_ect_value;
    }
  }

  network_quality_estimator_ = std::make_unique<net::NetworkQualityEstimator>(
      std::make_unique<net::NetworkQualityEstimatorParams>(
          network_quality_estimator_params),
      net_log);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Get network id asynchronously to workaround https://crbug.com/821607 where
  // AddressTrackerLinux stucks with a recv() call and blocks IO thread.
  // TODO(crbug.com/41376341): Remove after the bug is resolved.
  network_quality_estimator_->EnableGetNetworkIdAsynchronously();
#endif

  network_quality_estimator_->AddEffectiveConnectionTypeObserver(this);
  network_quality_estimator_->AddRTTAndThroughputEstimatesObserver(this);
  effective_connection_type_ =
      network_quality_estimator_->GetEffectiveConnectionType();
  http_rtt_ =
      network_quality_estimator_->GetHttpRTT().value_or(base::TimeDelta());
  transport_rtt_ =
      network_quality_estimator_->GetTransportRTT().value_or(base::TimeDelta());
  downstream_throughput_kbps_ =
      network_quality_estimator_->GetDownstreamThroughputKbps().value_or(
          INT32_MAX);
}

NetworkQualityEstimatorManager::~NetworkQualityEstimatorManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  network_quality_estimator_->RemoveEffectiveConnectionTypeObserver(this);
  network_quality_estimator_->RemoveRTTAndThroughputEstimatesObserver(this);
}

void NetworkQualityEstimatorManager::AddReceiver(
    mojo::PendingReceiver<mojom::NetworkQualityEstimatorManager> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  receivers_.Add(this, std::move(receiver));
}

void NetworkQualityEstimatorManager::RequestNotifications(
    mojo::PendingRemote<mojom::NetworkQualityEstimatorManagerClient>
        pending_client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mojo::Remote<mojom::NetworkQualityEstimatorManagerClient> client(
      std::move(pending_client));
  client->OnNetworkQualityChanged(effective_connection_type_, http_rtt_,
                                  transport_rtt_, downstream_throughput_kbps_);
  clients_.Add(std::move(client));
}

void NetworkQualityEstimatorManager::OnEffectiveConnectionTypeChanged(
    net::EffectiveConnectionType effective_connection_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::TimeDelta http_rtt = http_rtt_;
  base::TimeDelta transport_rtt = transport_rtt_;
  int32_t downstream_throughput_kbps = downstream_throughput_kbps_;

  if (effective_connection_type == effective_connection_type_)
    return;

  effective_connection_type_ = effective_connection_type;
  for (auto& client : clients_) {
    client->OnNetworkQualityChanged(effective_connection_type, http_rtt,
                                    transport_rtt, downstream_throughput_kbps);
  }
}

void NetworkQualityEstimatorManager::OnRTTOrThroughputEstimatesComputed(
    base::TimeDelta http_rtt,
    base::TimeDelta transport_rtt,
    int32_t downstream_throughput_kbps) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool http_rtt_changed_meaningfully = LatencyMetricChangedMeaningfully(
      http_rtt.InMilliseconds(), http_rtt_.InMilliseconds());
  bool transport_rtt_changed_meaningfully = LatencyMetricChangedMeaningfully(
      transport_rtt.InMilliseconds(), transport_rtt_.InMilliseconds());
  bool downlink_changed_meaningfully = BandwidthMetricChangedMeaningfully(
      downstream_throughput_kbps, downstream_throughput_kbps_);

  if (!http_rtt_changed_meaningfully && !transport_rtt_changed_meaningfully &&
      !downlink_changed_meaningfully) {
    return;
  }

  net::EffectiveConnectionType effective_connection_type =
      effective_connection_type_;
  http_rtt_ = http_rtt;
  transport_rtt_ = transport_rtt;
  downstream_throughput_kbps_ = downstream_throughput_kbps;

  for (auto& client : clients_) {
    client->OnNetworkQualityChanged(effective_connection_type, http_rtt,
                                    transport_rtt, downstream_throughput_kbps);
  }
}

net::NetworkQualityEstimator*
NetworkQualityEstimatorManager::GetNetworkQualityEstimator() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return network_quality_estimator_.get();
}

}  // namespace network
