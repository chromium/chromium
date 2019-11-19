// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_NETWORK_QUALITY_ESTIMATOR_MANAGER_H_
#define SERVICES_NETWORK_NETWORK_QUALITY_ESTIMATOR_MANAGER_H_

#include <memory>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/effective_connection_type_observer.h"
#include "net/nqe/rtt_throughput_estimates_observer.h"
#include "services/network/public/mojom/network_quality_estimator_manager.mojom.h"

namespace net {
class NetworkQualityEstimator;
class NetLog;
}  // namespace net

namespace network {

// Implementation of mojom::NetworkQualityEstimatorManager. All accesses to this
// class are done through mojo on the main thread. This registers itself to
// receive broadcasts from net::EffectiveConnectionTypeObserver and
// net::RTTAndThroughputEstimatesObserver. NetworkQualityEstimatorManager then
// rebroadcasts the notifications to
// mojom::NetworkQualityEstimatorManagerClients through mojo pipes.
class COMPONENT_EXPORT(NETWORK_SERVICE) NetworkQualityEstimatorManager
    : public mojom::NetworkQualityEstimatorManager,
      public net::EffectiveConnectionTypeObserver,
      public net::RTTAndThroughputEstimatesObserver {
 public:
  explicit NetworkQualityEstimatorManager(net::NetLog* net_log);

  ~NetworkQualityEstimatorManager() override;

  // Binds a NetworkQualityEstimatorManager receiver to this object. Mojo
  // messages coming through the associated pipe will be served by this object.
  void AddReceiver(
      mojo::PendingReceiver<mojom::NetworkQualityEstimatorManager> receiver);

  // mojom::NetworkQualityEstimatorManager implementation:
  void RequestNotifications(
      mojo::PendingRemote<mojom::NetworkQualityEstimatorManagerClient> client)
      override;

  net::NetworkQualityEstimator* GetNetworkQualityEstimator() const;

 private:
  // net::EffectiveConnectionTypeObserver implementation:
  void OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType type) override;
  // net::RTTAndThroughputEstimatesObserver implementation:
  void OnRTTOrThroughputEstimatesComputed(
      base::TimeDelta http_rtt,
      base::TimeDelta transport_rtt,
      int32_t downstream_throughput_kbps) override;

  std::unique_ptr<net::NetworkQualityEstimator> network_quality_estimator_;
  mojo::ReceiverSet<mojom::NetworkQualityEstimatorManager> receivers_;
  mojo::RemoteSet<mojom::NetworkQualityEstimatorManagerClient> clients_;
  net::EffectiveConnectionType effective_connection_type_;
  base::TimeDelta http_rtt_;
  base::TimeDelta transport_rtt_;
  int32_t downstream_throughput_kbps_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(NetworkQualityEstimatorManager);
};

}  // namespace network

#endif  // SERVICES_NETWORK_NETWORK_QUALITY_ESTIMATOR_MANAGER_H_
