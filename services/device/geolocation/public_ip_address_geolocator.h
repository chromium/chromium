// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_PUBLIC_IP_ADDRESS_GEOLOCATOR_H_
#define SERVICES_DEVICE_GEOLOCATION_PUBLIC_IP_ADDRESS_GEOLOCATOR_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/device/geolocation/public_ip_address_location_notifier.h"
#include "services/device/public/mojom/geolocation.mojom.h"
#include "services/device/public/mojom/geolocation_client_id.mojom.h"
#include "services/device/public/mojom/geoposition.mojom.h"

namespace device {

class PublicIpAddressLocationNotifier;

// An implementation of Geolocation that uses only public IP address-based
// geolocation.
class PublicIpAddressGeolocator : public mojom::Geolocation {
 public:
  using BadMessageCallback =
      base::RepeatingCallback<void(const std::string& message)>;

  // Creates a PublicIpAddressGeolocatorsubscribed to the specified |notifier|.
  // This object will unbind and destroy itself if |notifier| is destroyed.
  // |callback| is a  callback that should be called to signify reception of a
  // bad Mojo message *only while processing that message*.
  PublicIpAddressGeolocator(const net::PartialNetworkTrafficAnnotationTag tag,
                            PublicIpAddressLocationNotifier* notifier,
                            mojom::GeolocationClientId client_id,
                            BadMessageCallback callback);

  PublicIpAddressGeolocator(const PublicIpAddressGeolocator&) = delete;
  PublicIpAddressGeolocator& operator=(const PublicIpAddressGeolocator&) =
      delete;

  ~PublicIpAddressGeolocator() override;

 private:
  // mojom::Geolocation:
  void QueryNextPosition(QueryNextPositionCallback callback) override;
  void SetHighAccuracy(bool high_accuracy) override;

  // Callback to register with PublicIpAddressLocationNotifier.
  void OnPositionUpdate(mojom::GeopositionResultPtr result);

  // The callback passed to QueryNextPosition.
  QueryNextPositionCallback query_next_position_callback_;

  // Timestamp of latest Geoposition this client received.
  base::Time last_updated_timestamp_;

  // Notifier to ask for IP-geolocation updates.
  const raw_ptr<PublicIpAddressLocationNotifier, DanglingUntriaged> notifier_;

  const mojom::GeolocationClientId client_id_;

  // The most recent PartialNetworkTrafficAnnotationTag provided by a client.
  std::unique_ptr<const net::PartialNetworkTrafficAnnotationTag>
      network_traffic_annotation_tag_;

  // Bad message callback.
  BadMessageCallback bad_message_callback_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_PUBLIC_IP_ADDRESS_GEOLOCATOR_H_
