// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_quality_tracker.h"

#include <limits>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/observer_list.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace network {

NetworkQualityTracker::NetworkQualityTracker(
    base::RepeatingCallback<network::mojom::NetworkService*()> callback)
    : get_network_service_callback_(callback),
      effective_connection_type_(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN),
      downlink_bandwidth_kbps_(std::numeric_limits<int32_t>::max()),
      network_quality_overridden_for_testing_(false) {
  InitializeMojoChannel();
  DCHECK(receiver_.is_bound());
}

NetworkQualityTracker::~NetworkQualityTracker() {}

net::EffectiveConnectionType NetworkQualityTracker::GetEffectiveConnectionType()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return effective_connection_type_;
}

base::TimeDelta NetworkQualityTracker::GetHttpRTT() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return http_rtt_;
}

base::TimeDelta NetworkQualityTracker::GetTransportRTT() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return transport_rtt_;
}

int32_t NetworkQualityTracker::GetDownstreamThroughputKbps() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return downlink_bandwidth_kbps_;
}

void NetworkQualityTracker::AddEffectiveConnectionTypeObserver(
    EffectiveConnectionTypeObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  effective_connection_type_observer_list_.AddObserver(observer);
  if (effective_connection_type_ != net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN)
    observer->OnEffectiveConnectionTypeChanged(effective_connection_type_);
}

void NetworkQualityTracker::RemoveEffectiveConnectionTypeObserver(
    EffectiveConnectionTypeObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  effective_connection_type_observer_list_.RemoveObserver(observer);
}

void NetworkQualityTracker::AddRTTAndThroughputEstimatesObserver(
    RTTAndThroughputEstimatesObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  rtt_and_throughput_observer_list_.AddObserver(observer);
  observer->OnRTTOrThroughputEstimatesComputed(http_rtt_, transport_rtt_,
                                               downlink_bandwidth_kbps_);
}

void NetworkQualityTracker::RemoveRTTAndThroughputEstimatesObserver(
    RTTAndThroughputEstimatesObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  rtt_and_throughput_observer_list_.RemoveObserver(observer);
}

void NetworkQualityTracker::ReportEffectiveConnectionTypeForTesting(
    net::EffectiveConnectionType effective_connection_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  network_quality_overridden_for_testing_ = true;

  effective_connection_type_ = effective_connection_type;
  for (auto& observer : effective_connection_type_observer_list_) {
    observer.OnEffectiveConnectionTypeChanged(effective_connection_type);
  }
}

void NetworkQualityTracker::ReportRTTsAndThroughputForTesting(
    base::TimeDelta http_rtt,
    int32_t downstream_throughput_kbps) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  network_quality_overridden_for_testing_ = true;

  http_rtt_ = http_rtt;
  downlink_bandwidth_kbps_ = downstream_throughput_kbps;

  for (auto& observer : rtt_and_throughput_observer_list_) {
    observer.OnRTTOrThroughputEstimatesComputed(http_rtt_, http_rtt_,
                                                downlink_bandwidth_kbps_);
  }
}

// For testing only.
NetworkQualityTracker::NetworkQualityTracker()
    : effective_connection_type_(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN),
      downlink_bandwidth_kbps_(std::numeric_limits<int32_t>::max()) {}

void NetworkQualityTracker::OnNetworkQualityChanged(
    net::EffectiveConnectionType effective_connection_type,
    base::TimeDelta http_rtt,
    base::TimeDelta transport_rtt,
    int32_t bandwidth_kbps) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (network_quality_overridden_for_testing_)
    return;

  // If the RTT values are unavailable, set them to value 0.
  if (http_rtt.is_negative())
    http_rtt = base::TimeDelta();
  if (transport_rtt.is_negative())
    transport_rtt = base::TimeDelta();

  // If the bandwidth value is unavailable, set it to the maximum possible
  // value.
  if (bandwidth_kbps < 0)
    bandwidth_kbps = std::numeric_limits<int32_t>::max();

  if (http_rtt_ != http_rtt || transport_rtt_ != transport_rtt ||
      downlink_bandwidth_kbps_ != bandwidth_kbps) {
    http_rtt_ = http_rtt;
    transport_rtt_ = transport_rtt;
    downlink_bandwidth_kbps_ = bandwidth_kbps;

    for (auto& observer : rtt_and_throughput_observer_list_) {
      observer.OnRTTOrThroughputEstimatesComputed(http_rtt_, transport_rtt_,
                                                  downlink_bandwidth_kbps_);
    }
  }

  if (effective_connection_type != effective_connection_type_) {
    effective_connection_type_ = effective_connection_type;
    for (auto& observer : effective_connection_type_observer_list_)
      observer.OnEffectiveConnectionTypeChanged(effective_connection_type_);
  }
}

void NetworkQualityTracker::InitializeMojoChannel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!receiver_.is_bound());

  network::mojom::NetworkService* network_service =
      get_network_service_callback_.Run();
  DCHECK(network_service);

  // Get mojo::Remote<NetworkQualityEstimatorManager>.
  mojo::Remote<network::mojom::NetworkQualityEstimatorManager> manager;
  network_service->GetNetworkQualityEstimatorManager(
      manager.BindNewPipeAndPassReceiver());

  manager->RequestNotifications(receiver_.BindNewPipeAndPassRemote());

  // base::Unretained is safe as destruction of the
  // NetworkQualityTracker will also destroy the |receiver_|.
  receiver_.set_disconnect_handler(
      base::BindOnce(&NetworkQualityTracker::HandleNetworkServicePipeBroken,
                     base::Unretained(this)));
}

void NetworkQualityTracker::HandleNetworkServicePipeBroken() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  receiver_.reset();
  InitializeMojoChannel();
}

}  // namespace network
