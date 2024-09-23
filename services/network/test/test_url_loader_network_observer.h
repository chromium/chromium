// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TEST_TEST_URL_LOADER_NETWORK_OBSERVER_H_
#define SERVICES_NETWORK_TEST_TEST_URL_LOADER_NETWORK_OBSERVER_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"

namespace network {

// A helper class with a basic URLLoaderNetworkServiceObserver
// implementation for use in unittests, so they can just override the parts they
// need.
class TestURLLoaderNetworkObserver
    : public mojom::URLLoaderNetworkServiceObserver {
 public:
  TestURLLoaderNetworkObserver();
  ~TestURLLoaderNetworkObserver() override;

  mojo::PendingRemote<mojom::URLLoaderNetworkServiceObserver> Bind();

  void set_ignore_certificate_errors(bool ignore_certificate_errors) {
    ignore_certificate_errors_ = ignore_certificate_errors;
  }

  void FlushReceivers();

  // mojom::URLLoaderNetworkServiceObserver overrides:
  void OnSSLCertificateError(const GURL& url,
                             int net_error,
                             const net::SSLInfo& ssl_info,
                             bool fatal,
                             OnSSLCertificateErrorCallback response) override;
  void OnCertificateRequested(
      const std::optional<base::UnguessableToken>& window_id,
      const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
      mojo::PendingRemote<mojom::ClientCertificateResponder>
          client_cert_responder) override;
  void OnAuthRequired(
      const std::optional<base::UnguessableToken>& window_id,
      int32_t request_id,
      const GURL& url,
      bool first_auth_attempt,
      const net::AuthChallengeInfo& auth_info,
      const scoped_refptr<net::HttpResponseHeaders>& head_headers,
      mojo::PendingRemote<mojom::AuthChallengeResponder>
          auth_challenge_responder) override;
  void OnPrivateNetworkAccessPermissionRequired(
      const GURL& url,
      const net::IPAddress& ip_address,
      const std::optional<std::string>& private_network_device_id,
      const std::optional<std::string>& private_network_device_name,
      OnPrivateNetworkAccessPermissionRequiredCallback callback) override;
  void OnClearSiteData(
      const GURL& url,
      const std::string& header_value,
      int32_t load_flags,
      const std::optional<net::CookiePartitionKey>& cookie_partition_key,
      bool partitioned_state_allowed_only,
      OnClearSiteDataCallback callback) override;
  void OnLoadingStateUpdate(mojom::LoadInfoPtr info,
                            OnLoadingStateUpdateCallback callback) override;
  void OnDataUseUpdate(int32_t network_traffic_annotation_id_hash,
                       int64_t recv_bytes,
                       int64_t sent_bytes) override;
  void OnSharedStorageHeaderReceived(
      const url::Origin& request_origin,
      std::vector<network::mojom::SharedStorageOperationPtr> operations,
      OnSharedStorageHeaderReceivedCallback callback) override;
  void Clone(
      mojo::PendingReceiver<URLLoaderNetworkServiceObserver> observer) override;
  void OnWebSocketConnectedToPrivateNetwork(
      network::mojom::IPAddressSpace ip_address_space) override;

 private:
  mojo::ReceiverSet<mojom::URLLoaderNetworkServiceObserver> receivers_;
  bool ignore_certificate_errors_ = false;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TEST_TEST_URL_LOADER_NETWORK_OBSERVER_H_
