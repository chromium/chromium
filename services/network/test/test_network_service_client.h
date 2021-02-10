// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TEST_TEST_NETWORK_SERVICE_CLIENT_H_
#define SERVICES_NETWORK_TEST_TEST_NETWORK_SERVICE_CLIENT_H_

#include <string>
#include <vector>

#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/cookies/canonical_cookie.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace network {

// A helper class with a basic NetworkServiceClient implementation for use in
// unittests, which may need an implementation (for things like file uploads),
// but don't have the real implementation available.
class TestNetworkServiceClient : public network::mojom::NetworkServiceClient {
 public:
  TestNetworkServiceClient();
  explicit TestNetworkServiceClient(
      mojo::PendingReceiver<mojom::NetworkServiceClient> receiver);
  ~TestNetworkServiceClient() override;

  // network::mojom::NetworkServiceClient implementation:
  void OnLoadingStateUpdate(std::vector<mojom::LoadInfoPtr> infos,
                            OnLoadingStateUpdateCallback callback) override;
  void OnDataUseUpdate(int32_t network_traffic_annotation_id_hash,
                       int64_t recv_bytes,
                       int64_t sent_bytes) override;
  void OnRawRequest(
      int32_t process_id,
      int32_t routing_id,
      const std::string& devtools_request_id,
      const net::CookieAccessResultList& cookies_with_access_result,
      std::vector<network::mojom::HttpRawHeaderPairPtr> headers,
      network::mojom::ClientSecurityStatePtr client_security_state) override;
  void OnRawResponse(
      int32_t process_id,
      int32_t routing_id,
      const std::string& devtools_request_id,
      const net::CookieAndLineAccessResultList& cookies_with_access_result,
      std::vector<network::mojom::HttpRawHeaderPairPtr> headers,
      const base::Optional<std::string>& raw_response_headers,
      network::mojom::IPAddressSpace resource_address_space) override;
  void OnPrivateNetworkRequest(
      int32_t process_id,
      int32_t routing_id,
      const base::Optional<std::string>& devtools_request_id,
      const GURL& url,
      bool is_warning,
      network::mojom::IPAddressSpace resource_address_space,
      network::mojom::ClientSecurityStatePtr client_security_state) override;
  void OnCorsPreflightRequest(
      int32_t process_id,
      int32_t routing_id,
      const base::UnguessableToken& devtool_request_id,
      const network::ResourceRequest& request,
      const GURL& initiator_url,
      const std::string& initiator_devtools_request_id) override;
  void OnCorsPreflightResponse(
      int32_t process_id,
      int32_t routing_id,
      const base::UnguessableToken& devtool_request_id,
      const GURL& url,
      network::mojom::URLResponseHeadPtr head) override;
  void OnCorsPreflightRequestCompleted(
      int32_t process_id,
      int32_t routing_id,
      const base::UnguessableToken& devtool_request_id,
      const network::URLLoaderCompletionStatus& status) override;
  void OnTrustTokenOperationDone(
      int32_t process_id,
      int32_t routing_id,
      const std::string& devtool_request_id,
      network::mojom::TrustTokenOperationResultPtr result) override;

 private:
  mojo::Receiver<mojom::NetworkServiceClient> receiver_;

  DISALLOW_COPY_AND_ASSIGN(TestNetworkServiceClient);
};

}  // namespace network

#endif  // SERVICES_NETWORK_TEST_TEST_NETWORK_SERVICE_CLIENT_H_
