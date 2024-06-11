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

// Takes wifi data and sends it to a server to get a position fix.
// It performs formatting of the request and interpretation of the response.
class NetworkLocationRequest {
 public:
  // Called when a new geo position is available. The second argument indicates
  // whether there was a server error or not. It is true when there was a
  // server or network error - either no response or a 500 error code.
  using LocationResponseCallback = base::RepeatingCallback<void(
      mojom::GeopositionResultPtr /* result */,
      bool /* server_error */,
      const WifiData& /* wifi_data */,
      mojom::NetworkLocationResponsePtr /* response data */)>;

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
