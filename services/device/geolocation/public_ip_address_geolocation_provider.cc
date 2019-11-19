// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/public_ip_address_geolocation_provider.h"

#include "base/bind.h"
#include "services/device/geolocation/public_ip_address_geolocator.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace device {

PublicIpAddressGeolocationProvider::PublicIpAddressGeolocationProvider(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    network::NetworkConnectionTracker* network_connection_tracker,
    const std::string& api_key) {
  // Bind sequence_checker_ to the initialization sequence.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  public_ip_address_location_notifier_ =
      std::make_unique<PublicIpAddressLocationNotifier>(
          std::move(url_loader_factory), network_connection_tracker, api_key);
}

PublicIpAddressGeolocationProvider::~PublicIpAddressGeolocationProvider() {}

void PublicIpAddressGeolocationProvider::Bind(
    mojo::PendingReceiver<mojom::PublicIpAddressGeolocationProvider> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(public_ip_address_location_notifier_);
  provider_receiver_set_.Add(this, std::move(receiver));
}

void PublicIpAddressGeolocationProvider::CreateGeolocation(
    const net::MutablePartialNetworkTrafficAnnotationTag& tag,
    mojo::PendingReceiver<mojom::Geolocation> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(public_ip_address_location_notifier_);
  geolocation_receiver_set_.Add(
      std::make_unique<PublicIpAddressGeolocator>(
          static_cast<net::PartialNetworkTrafficAnnotationTag>(tag),
          public_ip_address_location_notifier_.get(),
          base::Bind(
              &mojo::UniqueReceiverSet<mojom::Geolocation>::ReportBadMessage,
              base::Unretained(&geolocation_receiver_set_))),
      std::move(receiver));
}

}  // namespace device
