// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/public_ip_address_geolocator.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/device/geolocation/public_ip_address_location_notifier.h"

namespace device {

namespace {
void RecordUmaPublicIpAddressGeolocatorClientId(
    mojom::GeolocationClientId client_id) {
  base::UmaHistogramEnumeration(
      "Geolocation.PublicIpAddressGeolocator.ClientId", client_id);
}
}  // namespace

PublicIpAddressGeolocator::PublicIpAddressGeolocator(
    const net::PartialNetworkTrafficAnnotationTag tag,
    PublicIpAddressLocationNotifier* const notifier,
    mojom::GeolocationClientId client_id,
    BadMessageCallback callback)
    : last_updated_timestamp_(),
      notifier_(notifier),
      client_id_(client_id),
      network_traffic_annotation_tag_(
          std::make_unique<const net::PartialNetworkTrafficAnnotationTag>(tag)),
      bad_message_callback_(callback) {}

PublicIpAddressGeolocator::~PublicIpAddressGeolocator() {}

void PublicIpAddressGeolocator::QueryNextPosition(
    QueryNextPositionCallback callback) {
  if (query_next_position_callback_) {
    bad_message_callback_.Run(
        "Overlapping calls to QueryNextPosition are prohibited.");
    return;
  }

  DCHECK(notifier_);
  // Request the next position after the latest one we received.
  notifier_->QueryNextPosition(
      last_updated_timestamp_, *network_traffic_annotation_tag_,
      base::BindOnce(&PublicIpAddressGeolocator::OnPositionUpdate,
                     base::Unretained(this)));
  RecordUmaPublicIpAddressGeolocatorClientId(client_id_);

  // Retain the callback to use if/when we get a new position.
  query_next_position_callback_ = std::move(callback);
}

// Low/high accuracy toggle is ignored by this implementation.
void PublicIpAddressGeolocator::SetHighAccuracy(bool /* high_accuracy */) {}

void PublicIpAddressGeolocator::OnPositionUpdate(
    mojom::GeopositionResultPtr result) {
  DCHECK(result);
  if (result->is_position()) {
    last_updated_timestamp_ = result->get_position()->timestamp;
  }
  // Use Clone since query_next_position_callback_ needs an
  // device::mojom::GeopositionResultPtr.
  std::move(query_next_position_callback_).Run(std::move(result));
}

}  // namespace device
