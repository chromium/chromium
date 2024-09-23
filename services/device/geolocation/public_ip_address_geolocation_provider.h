// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_PUBLIC_IP_ADDRESS_GEOLOCATION_PROVIDER_H_
#define SERVICES_DEVICE_GEOLOCATION_PUBLIC_IP_ADDRESS_GEOLOCATION_PROVIDER_H_

#include <string>

#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/device/geolocation/public_ip_address_geolocator.h"
#include "services/device/geolocation/public_ip_address_location_notifier.h"
#include "services/device/public/mojom/geolocation.mojom.h"
#include "services/device/public/mojom/geolocation_client_id.mojom.h"
#include "services/device/public/mojom/public_ip_address_geolocation_provider.mojom.h"

namespace network {
class NetworkConnectionTracker;
}

namespace device {

// Implementation of PublicIpAddressGeolocationProvider Mojo interface that will
// provide mojom::Geolocation implementations that use IP-only geolocation.
// Binds multiple PublicIpAddressGeolocationProvider requests.
//
// Sequencing:
// * Must be used and destroyed on the same sequence.
// * Provides mojom::Geolocation instances that are bound on the same sequence.
class PublicIpAddressGeolocationProvider
    : public mojom::PublicIpAddressGeolocationProvider {
 public:
  // Initialize PublicIpAddressGeolocationProvider using the specified Google
  // |api_key| and |url_loader_factory| for network location requests.
  PublicIpAddressGeolocationProvider(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      network::NetworkConnectionTracker* network_connection_tracker,
      const std::string& api_key);

  PublicIpAddressGeolocationProvider(
      const PublicIpAddressGeolocationProvider&) = delete;
  PublicIpAddressGeolocationProvider& operator=(
      const PublicIpAddressGeolocationProvider&) = delete;

  ~PublicIpAddressGeolocationProvider() override;

  // Binds a PublicIpAddressGeolocationProvider request to this instance.
  void Bind(mojo::PendingReceiver<mojom::PublicIpAddressGeolocationProvider>
                receiver);

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // mojom::PublicIpAddressGeolocationProvider implementation:
  // Provides a Geolocation instance that performs IP-geolocation only.
  void CreateGeolocation(
      const net::MutablePartialNetworkTrafficAnnotationTag& tag,
      mojo::PendingReceiver<mojom::Geolocation> receiver,
      mojom::GeolocationClientId client_id) override;

  // Central PublicIpAddressLocationNotifier for use by all implementations of
  // mojom::Geolocation provided by the CreateGeolocation method.
  // Note that this must be before the UniqueReceiverSet<mojom::Geolocation> as
  // it must outlive the Geolocation implementations.
  std::unique_ptr<PublicIpAddressLocationNotifier>
      public_ip_address_location_notifier_;

  mojo::ReceiverSet<mojom::PublicIpAddressGeolocationProvider>
      provider_receiver_set_;

  mojo::UniqueReceiverSet<mojom::Geolocation> geolocation_receiver_set_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_PUBLIC_IP_ADDRESS_GEOLOCATION_PROVIDER_H_
