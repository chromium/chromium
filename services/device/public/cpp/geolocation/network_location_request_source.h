// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_NETWORK_LOCATION_REQUEST_SOURCE_H_
#define SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_NETWORK_LOCATION_REQUEST_SOURCE_H_

namespace device {

// The location provider that initiated a network geolocation request.
//
// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "NetworkLocationRequestSource" in
// src/tools/metrics/histograms/metadata/geolocation/enums.xml.
enum class NetworkLocationRequestSource {
  kNetworkLocationProvider = 0,
  kPublicIpAddressGeolocator = 1,
  kSimpleGeolocationProvider = 2,
  kMaxValue = kSimpleGeolocationProvider,
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_NETWORK_LOCATION_REQUEST_SOURCE_H_
