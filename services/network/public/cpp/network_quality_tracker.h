// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_NETWORK_QUALITY_TRACKER_H_
#define SERVICES_NETWORK_PUBLIC_CPP_NETWORK_QUALITY_TRACKER_H_

#include <memory>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/nqe/effective_connection_type.h"
#include "services/network/public/mojom/network_quality_estimator_manager.mojom.h"
#include "services/network/public/mojom/network_service.mojom-forward.h"

namespace network {

// This class subscribes to network quality change events from
// network::mojom::NetworkQualityEstimatorManagerClient and propagates these
// notifications to its list of EffectiveConnectionTypeObserver registered
// through AddObserver() and RemoveObserver().
class COMPONENT_EXPORT(NETWORK_CPP) NetworkQualityTracker
    : public network::mojom::NetworkQualityEstimatorManagerClient {
 public:
  class COMPONENT_EXPORT(NETWORK_CPP) EffectiveConnectionTypeObserver {
   public:
    // Called when there is a change in the effective connection type. The
    // |observer| is notified of the current effective connection type on the
    // same thread on which it was added.
    virtual void OnEffectiveConnectionTypeChanged(
        net::EffectiveConnectionType type) = 0;

   protected:
    EffectiveConnectionTypeObserver() {}
    virtual ~EffectiveConnectionTypeObserver() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(EffectiveConnectionTypeObserver);
  };

  // Observes changes in the HTTP RTT, transport RTT or downstream throughput
  // estimates.
  class COMPONENT_EXPORT(NETWORK_CPP) RTTAndThroughputEstimatesObserver {
   public:
    // Called when there is a substantial change in either HTTP RTT, transport
    // RTT or downstream estimate. If either of the RTT estimates are
    // unavailable, then the value of that estimate is set to base::TimeDelta().
    // If downstream estimate is unavailable, its value is set to INT32_MAX.
    // The |observer| is notified of the current effective connection type on
    // the same thread on which it was added.
    virtual void OnRTTOrThroughputEstimatesComputed(
        base::TimeDelta http_rtt,
        base::TimeDelta transport_rtt,
        int32_t downstream_throughput_kbps) = 0;

    virtual ~RTTAndThroughputEstimatesObserver() {}

   protected:
    RTTAndThroughputEstimatesObserver() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(RTTAndThroughputEstimatesObserver);
  };

  // Running the |callback| returns the network service in use.
  // NetworkQualityTracker does not need to be destroyed before the network
  // service.
  explicit NetworkQualityTracker(
      base::RepeatingCallback<network::mojom::NetworkService*()> callback);

  ~NetworkQualityTracker() override;

  // Returns the current estimate of the effective connection type.
  virtual net::EffectiveConnectionType GetEffectiveConnectionType() const;

  // Returns the current HTTP RTT estimate. The RTT at the HTTP layer measures
  // the time from when the request was sent (this happens after the connection
  // is established) to the time when the response headers were received.
  virtual base::TimeDelta GetHttpRTT() const;

  // Returns the current transport-layer RTT estimate. The RTT at the transport
  // layer provides an aggregate estimate of the transport RTT as computed by
  // various underlying TCP and QUIC connections.
  virtual base::TimeDelta GetTransportRTT() const;

  // Returns the current downstream throughput estimate (in kilobits per
  // second).
  virtual int32_t GetDownstreamThroughputKbps() const;

  // Registers |observer| to receive notifications of network changes. The
  // thread on which this is called is the thread on which |observer| will be
  // called back with notifications. The |observer| is notified of the current
  // effective connection type on the same thread. At the time |observer| is
  // added, if the estimated effective connection type is unknown, then the
  // |observer| is not notified until there is a change in the network quality
  // estimate.
  void AddEffectiveConnectionTypeObserver(
      EffectiveConnectionTypeObserver* observer);

  // Unregisters |observer| from receiving notifications. This must be called
  // on the same thread on which AddObserver() was called.
  // All observers must be unregistered before |this| is destroyed.
  void RemoveEffectiveConnectionTypeObserver(
      EffectiveConnectionTypeObserver* observer);

  // Registers |observer| to receive notifications of RTT or throughput changes.
  // This should be called on the same thread as the thread on which |this| is
  // created. The |observer| would be called back with notifications on that
  // same thread. The |observer| is notified of the current HTTP RTT, transport
  // RTT and downstrean bandwidth estimates on the same thread. If either of the
  // RTT estimate are unavailable, then the value of that estimate is set to
  // base::TimeDelta(). If downstream estimate is unavailable, its value is set
  // to INT32_MAX. The |observer| is notified of the current RTT and
  // throughput estimates synchronously when this method is invoked.
  void AddRTTAndThroughputEstimatesObserver(
      RTTAndThroughputEstimatesObserver* observer);

  // Unregisters |observer| from receiving notifications. This must be called
  // on the same thread on which AddObserver() was called.
  // All observers must be unregistered before |this| is destroyed.
  void RemoveRTTAndThroughputEstimatesObserver(
      RTTAndThroughputEstimatesObserver* observer);

  // Changes effective connection type estimate to the provided value, and
  // reports |effective_connection_type| to all
  // EffectiveConnectionTypeObservers. Calling this also disables all organic
  // notifications sent to observers.
  void ReportEffectiveConnectionTypeForTesting(
      net::EffectiveConnectionType effective_connection_type);

  // Changes RTT and throughput estimate to the provided estimates, and
  // reports it to all RTTAndThroughputEstimatesObservers. Calling this also
  // disables all organic notifications sent to observers.
  void ReportRTTsAndThroughputForTesting(base::TimeDelta http_rtt,
                                         int32_t downstream_throughput_kbps);

 protected:
  // Constructor for testing purposes only without the network service instance.
  NetworkQualityTracker();

  // NetworkQualityEstimatorManagerClient implementation. Protected for testing.
  void OnNetworkQualityChanged(
      net::EffectiveConnectionType effective_connection_type,
      base::TimeDelta http_rtt,
      base::TimeDelta transport_rtt,
      int32_t downlink_bandwidth_kbps) override;

 private:
  // Starts listening for network quality change notifications from
  // network_service. Observers may be added and GetEffectiveConnectionType
  // called, but no network information will be provided until this method is
  // called.
  void InitializeMojoChannel();

  void HandleNetworkServicePipeBroken();

  // Running the |get_network_service_callback_| returns the network service in
  // use.
  const base::RepeatingCallback<network::mojom::NetworkService*()>
      get_network_service_callback_;

  net::EffectiveConnectionType effective_connection_type_;
  base::TimeDelta http_rtt_;
  base::TimeDelta transport_rtt_;
  int32_t downlink_bandwidth_kbps_;

  // True if network quality has been overridden by tests. If set to true, it
  // disables all organic notifications sent to observers.
  bool network_quality_overridden_for_testing_;

  base::ObserverList<EffectiveConnectionTypeObserver>::Unchecked
      effective_connection_type_observer_list_;

  base::ObserverList<RTTAndThroughputEstimatesObserver>::Unchecked
      rtt_and_throughput_observer_list_;

  mojo::Receiver<network::mojom::NetworkQualityEstimatorManagerClient>
      receiver_{this};

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(NetworkQualityTracker);
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_NETWORK_QUALITY_TRACKER_H_
