// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/test_network_service_client.h"

#include <utility>

#include "base/optional.h"
#include "base/task/post_task.h"
#include "base/unguessable_token.h"

namespace network {

TestNetworkServiceClient::TestNetworkServiceClient() : receiver_(nullptr) {}

TestNetworkServiceClient::TestNetworkServiceClient(
    mojo::PendingReceiver<mojom::NetworkServiceClient> receiver)
    : receiver_(this, std::move(receiver)) {}

TestNetworkServiceClient::~TestNetworkServiceClient() {}

void TestNetworkServiceClient::OnLoadingStateUpdate(
    std::vector<mojom::LoadInfoPtr> infos,
    OnLoadingStateUpdateCallback callback) {}

void TestNetworkServiceClient::OnDataUseUpdate(
    int32_t network_traffic_annotation_id_hash,
    int64_t recv_bytes,
    int64_t sent_bytes) {}

void TestNetworkServiceClient::OnRawRequest(
    int32_t process_id,
    int32_t routing_id,
    const std::string& devtools_request_id,
    const net::CookieAccessResultList& cookies_with_access_result,
    std::vector<network::mojom::HttpRawHeaderPairPtr> headers,
    network::mojom::ClientSecurityStatePtr client_security_state) {}

void TestNetworkServiceClient::OnRawResponse(
    int32_t process_id,
    int32_t routing_id,
    const std::string& devtools_request_id,
    const net::CookieAndLineAccessResultList& cookies_with_access_result,
    std::vector<network::mojom::HttpRawHeaderPairPtr> headers,
    const base::Optional<std::string>& raw_response_headers,
    network::mojom::IPAddressSpace resource_address_space) {}

void TestNetworkServiceClient::OnPrivateNetworkRequest(
    int32_t process_id,
    int32_t routing_id,
    const base::Optional<std::string>& devtools_request_id,
    const GURL& url,
    bool is_warning,
    network::mojom::IPAddressSpace resource_address_space,
    network::mojom::ClientSecurityStatePtr client_security_state) {}

void TestNetworkServiceClient::OnCorsPreflightRequest(
    int32_t process_id,
    int32_t routing_id,
    const base::UnguessableToken& devtools_request_id,
    const network::ResourceRequest& request,
    const GURL& initiator_url,
    const std::string& initiator_devtools_request_id) {}

void TestNetworkServiceClient::OnCorsPreflightResponse(
    int32_t process_id,
    int32_t routing_id,
    const base::UnguessableToken& devtools_request_id,
    const GURL& url,
    network::mojom::URLResponseHeadPtr head) {}

void TestNetworkServiceClient::OnCorsPreflightRequestCompleted(
    int32_t process_id,
    int32_t routing_id,
    const base::UnguessableToken& devtool_request_id,
    const network::URLLoaderCompletionStatus& status) {}

void TestNetworkServiceClient::OnTrustTokenOperationDone(
    int32_t process_id,
    int32_t routing_id,
    const std::string& devtool_request_id,
    network::mojom::TrustTokenOperationResultPtr result) {}

}  // namespace network
