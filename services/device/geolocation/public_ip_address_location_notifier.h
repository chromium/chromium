// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_PUBLIC_IP_ADDRESS_LOCATION_NOTIFIER_H_
#define SERVICES_DEVICE_GEOLOCATION_PUBLIC_IP_ADDRESS_LOCATION_NOTIFIER_H_

#include <memory>

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/device/geolocation/geolocation_provider.h"
#include "services/device/geolocation/network_location_request.h"
#include "services/device/public/mojom/geoposition.mojom.h"
#include "services/network/public/cpp/network_connection_tracker.h"

namespace device {

class NetworkLocationRequest;
struct WifiData;

// Provides subscribers with updates of the device's approximate geographic
// location inferred from its publicly-visible IP address.
// Sequencing:
// * Must be created, used, and destroyed on the same sequence.
class PublicIpAddressLocationNotifier
    : public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  // Creates a notifier that uses the specified Google |api_key| and
  // |url_loader_factory| for network location requests.
  PublicIpAddressLocationNotifier(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      network::NetworkConnectionTracker* network_connection_tracker,
      const std::string& api_key);
  ~PublicIpAddressLocationNotifier() override;

  using QueryNextPositionCallback =
      base::OnceCallback<void(const mojom::Geoposition&)>;

  // Requests a callback with the next Geoposition obtained later than
  // |time_of_prev_position|.
  // Specifically:
  // * If a position has been obtained subsequent to |time_of_prev_position|,
  // returns it.
  // * Otherwise, returns an updated position once a network change has
  // occurred.
  // * Note that it is possible for |callback| to never be called if no network
  // change ever occurs after |time_of_prev_position|.
  void QueryNextPosition(base::Time time_of_prev_position,
                         const net::PartialNetworkTrafficAnnotationTag& tag,
                         QueryNextPositionCallback callback);

 private:
  // Sequence checker for all methods.
  SEQUENCE_CHECKER(sequence_checker_);

  // NetworkConnectionTracker::NetworkConnectionObserver:
  // Network change notifications tend to come in a cluster in a short time, so
  // this just sets a task to run ReactToNetworkChange after a short time.
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  // Actually react to a network change, starting a network geolocation request
  // if any clients are waiting.
  void ReactToNetworkChange();

  // Creates |network_location_request_| and starts the network request, which
  // will invoke OnNetworkLocationResponse when done.
  void MakeNetworkLocationRequest();

  // Completion callback for network_location_request_.
  void OnNetworkLocationResponse(const mojom::Geoposition& position,
                                 bool server_error,
                                 const WifiData& wifi_data);

  // Cancelable closure to absorb overlapping delayed calls to
  // ReactToNetworkChange.
  base::CancelableClosure react_to_network_change_closure_;

  // Whether we have been notified of a network change since the last network
  // location request was sent.
  bool network_changed_since_last_request_;

  // The geoposition as of the latest network change, if it has been obtained.
  base::Optional<mojom::Geoposition> latest_geoposition_;

  // Google API key for network geolocation requests.
  const std::string api_key_;

  // SharedURLLoaderFactory for network geolocation requests.
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Used to listen to network connection changes.
  // Must outlive this object.
  network::NetworkConnectionTracker* network_connection_tracker_;

  // Used to make calls to the Maps geolocate API.
  // Empty unless a call is currently in progress.
  std::unique_ptr<NetworkLocationRequest> network_location_request_;

  // Clients waiting for an updated geoposition.
  std::vector<QueryNextPositionCallback> callbacks_;

  // The most recent PartialNetworkTrafficAnnotationTag provided by a client.
  std::unique_ptr<const net::PartialNetworkTrafficAnnotationTag>
      network_traffic_annotation_tag_;

  // Weak references to |this| for posted tasks.
  base::WeakPtrFactory<PublicIpAddressLocationNotifier> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PublicIpAddressLocationNotifier);
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_PUBLIC_IP_ADDRESS_LOCATION_NOTIFIER_H_
