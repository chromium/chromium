// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/test_url_loader_network_observer.h"

#include "net/base/net_errors.h"

namespace network {

TestURLLoaderNetworkObserver::TestURLLoaderNetworkObserver() = default;
TestURLLoaderNetworkObserver::~TestURLLoaderNetworkObserver() = default;

mojo::PendingRemote<mojom::URLLoaderNetworkServiceObserver>
TestURLLoaderNetworkObserver::Bind() {
  mojo::PendingRemote<mojom::URLLoaderNetworkServiceObserver> remote;
  receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void TestURLLoaderNetworkObserver::FlushReceivers() {
  receivers_.FlushForTesting();
}

void TestURLLoaderNetworkObserver::OnSSLCertificateError(
    const GURL& url,
    int net_error,
    const net::SSLInfo& ssl_info,
    bool fatal,
    OnSSLCertificateErrorCallback response) {
  std::move(response).Run(ignore_certificate_errors_ ? net::OK : net_error);
}

void TestURLLoaderNetworkObserver::OnCertificateRequested(
    const std::optional<base::UnguessableToken>& window_id,
    const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
    mojo::PendingRemote<mojom::ClientCertificateResponder>
        client_cert_responder) {}

void TestURLLoaderNetworkObserver::OnAuthRequired(
    const std::optional<base::UnguessableToken>& window_id,
    int32_t request_id,
    const GURL& url,
    bool first_auth_attempt,
    const net::AuthChallengeInfo& auth_info,
    const scoped_refptr<net::HttpResponseHeaders>& head_headers,
    mojo::PendingRemote<mojom::AuthChallengeResponder>
        auth_challenge_responder) {}

void TestURLLoaderNetworkObserver::OnPrivateNetworkAccessPermissionRequired(
    const GURL& url,
    const net::IPAddress& ip_address,
    const std::optional<std::string>& private_network_device_id,
    const std::optional<std::string>& private_network_device_name,
    OnPrivateNetworkAccessPermissionRequiredCallback callback) {
  std::move(callback).Run(false);
}

void TestURLLoaderNetworkObserver::OnClearSiteData(
    const GURL& url,
    const std::string& header_value,
    int32_t load_flags,
    const std::optional<net::CookiePartitionKey>& cookie_partition_key,
    bool partitioned_state_allowed_only,
    OnClearSiteDataCallback callback) {
  std::move(callback).Run();
}

void TestURLLoaderNetworkObserver::OnLoadingStateUpdate(
    mojom::LoadInfoPtr info,
    OnLoadingStateUpdateCallback callback) {
  std::move(callback).Run();
}

void TestURLLoaderNetworkObserver::OnDataUseUpdate(
    int32_t network_traffic_annotation_id_hash,
    int64_t recv_bytes,
    int64_t sent_bytes) {}

void TestURLLoaderNetworkObserver::OnSharedStorageHeaderReceived(
    const url::Origin& request_origin,
    std::vector<network::mojom::SharedStorageOperationPtr> operations,
    OnSharedStorageHeaderReceivedCallback callback) {
  std::move(callback).Run();
}

void TestURLLoaderNetworkObserver::Clone(
    mojo::PendingReceiver<URLLoaderNetworkServiceObserver> observer) {
  receivers_.Add(this, std::move(observer));
}

void TestURLLoaderNetworkObserver::OnWebSocketConnectedToPrivateNetwork(
    network::mojom::IPAddressSpace ip_address_space) {}

}  // namespace network
