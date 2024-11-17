// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_NETWORK_LOCATION_REQUEST_H_
#define SERVICES_DEVICE_GEOLOCATION_NETWORK_LOCATION_REQUEST_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "services/device/geolocation/wifi_data_provider.h"
#include "services/device/public/cpp/geolocation/network_location_request_source.h"
#include "services/device/public/mojom/geolocation_internals.mojom.h"
#include "services/device/public/mojom/geoposition.mojom.h"
#include "url/gurl.h"

namespace net {
struct PartialNetworkTrafficAnnotationTag;
}  // namespace net

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace device {

// Defines the possible outcomes of a network location request. NOTE: Do not
// renumber these as that would confuse interpretation of previously logged
// data. When making changes, also update the enum list in
// tools/metrics/histograms/metadata/geolocation/enums.xml to keep it in sync.
enum class NetworkLocationRequestResult {
  kSuccess = 0,
  kCanceled = 1,
  kNetworkError = 2,
  kResponseNotOk = 3,
  kResponseEmpty = 4,
  kResponseMalformed = 5,
  kInvalidPosition = 6,
  kMaxValue = kInvalidPosition,
};

// Holds the result of a location request, including the position,
// the request status, and the raw response from the network.
struct LocationResponseResult {
  LocationResponseResult(mojom::GeopositionResultPtr position,
                         NetworkLocationRequestResult result_code,
                         mojom::NetworkLocationResponsePtr raw_response);

  LocationResponseResult(LocationResponseResult&& other);
  LocationResponseResult& operator=(LocationResponseResult&& other);

  ~LocationResponseResult();

  mojom::GeopositionResultPtr position;
  NetworkLocationRequestResult result_code;
  mojom::NetworkLocationResponsePtr raw_response;
};

// Takes wifi data and sends it to a server to get a position fix.
// It performs formatting of the request and interpretation of the response.
class NetworkLocationRequest {
 public:
  // Called when a new geo position is available. The first argument includes
  // the collection of information (position / request result code / raw
  // response) from a network request. The second argument provides the wifi
  // access point data that can be used to create a position cache to prevent
  // unnecessary network requests.
  using LocationResponseCallback =
      base::RepeatingCallback<void(LocationResponseResult /* result */,
                                   const WifiData& /* wifi_data */)>;

  NetworkLocationRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& api_key,
      LocationResponseCallback callback);

  NetworkLocationRequest(const NetworkLocationRequest&) = delete;
  NetworkLocationRequest& operator=(const NetworkLocationRequest&) = delete;

  ~NetworkLocationRequest();

  // Makes a new request using the specified |wifi_data|. Any currently pending
  // request will be canceled. The specified |wifi_data| and |wifi_timestamp|
  // are passed back to the client upon completion, via
  // LocationResponseCallback's |wifi_data| and |position.timestamp|
  // respectively.
  void MakeRequest(
      const WifiData& wifi_data,
      const base::Time& wifi_timestamp,
      const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation,
      NetworkLocationRequestSource network_location_request_source);

  bool is_request_pending() const { return bool(url_loader_); }

  std::vector<mojom::AccessPointDataPtr> GetRequestDataForDiagnostics() const;

 private:
  void OnRequestComplete(std::unique_ptr<std::string> data);

  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const std::string api_key_;
  const LocationResponseCallback location_response_callback_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // Keep a copy of the data sent in the request, so we can refer back to it
  // when the response arrives.
  WifiData wifi_data_;
  base::Value::Dict request_data_;
  base::Time wifi_timestamp_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_NETWORK_LOCATION_REQUEST_H_
