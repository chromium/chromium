// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_URL_LOADER_NETWORK_SERVICE_OBSERVER_H_
#define REMOTING_BASE_URL_LOADER_NETWORK_SERVICE_OBSERVER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/ssl/client_cert_store.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "url/gurl.h"

namespace remoting {

// Helper class for providing a certificate for HTTPS requests.
class UrlLoaderNetworkServiceObserver
    : public network::mojom::URLLoaderNetworkServiceObserver {
 public:
  UrlLoaderNetworkServiceObserver();
  ~UrlLoaderNetworkServiceObserver() override;

  UrlLoaderNetworkServiceObserver(const UrlLoaderNetworkServiceObserver&) =
      delete;
  UrlLoaderNetworkServiceObserver& operator=(
      const UrlLoaderNetworkServiceObserver&) = delete;

  mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver> Bind();

 private:
  // network::mojom::URLLoaderNetworkServiceObserver overrides.
  void OnSSLCertificateError(const GURL& url,
                             int net_error,
                             const net::SSLInfo& ssl_info,
                             bool fatal,
                             OnSSLCertificateErrorCallback response) override;
  void OnCertificateRequested(
      const std::optional<base::UnguessableToken>& window_id,
      const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
      mojo::PendingRemote<network::mojom::ClientCertificateResponder>
          cert_responder) override;
  void OnAuthRequired(
      const std::optional<base::UnguessableToken>& window_id,
      int32_t request_id,
      const GURL& url,
      bool first_auth_attempt,
      const net::AuthChallengeInfo& auth_info,
      const scoped_refptr<net::HttpResponseHeaders>& head_headers,
      mojo::PendingRemote<network::mojom::AuthChallengeResponder>
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
      int load_flags,
      const std::optional<net::CookiePartitionKey>& cookie_partition_key,
      bool partitioned_state_allowed_only,
      OnClearSiteDataCallback callback) override;
  void OnLoadingStateUpdate(network::mojom::LoadInfoPtr info,
                            OnLoadingStateUpdateCallback callback) override;
  void OnDataUseUpdate(int32_t network_traffic_annotation_id_hash,
                       int64_t recv_bytes,
                       int64_t sent_bytes) override;
  void OnSharedStorageHeaderReceived(
      const url::Origin& request_origin,
      std::vector<network::mojom::SharedStorageOperationPtr> operations,
      OnSharedStorageHeaderReceivedCallback callback) override;
  void Clone(
      mojo::PendingReceiver<network::mojom::URLLoaderNetworkServiceObserver>
          listener) override;
  void OnWebSocketConnectedToPrivateNetwork(
      network::mojom::IPAddressSpace ip_address_space) override;

  void OnCertificatesSelected(
      mojo::PendingRemote<network::mojom::ClientCertificateResponder>
          client_cert_responder,
      std::unique_ptr<net::ClientCertStore> cert_store,
      const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
      net::ClientCertIdentityList selected_certs);

  void ContinueWithCertificate(
      mojo::PendingRemote<network::mojom::ClientCertificateResponder>
          client_cert_responder,
      scoped_refptr<net::X509Certificate> client_cert,
      scoped_refptr<net::SSLPrivateKey> client_private_key);

  SEQUENCE_CHECKER(sequence_checker_);

  mojo::ReceiverSet<network::mojom::URLLoaderNetworkServiceObserver> receivers_;
  base::WeakPtrFactory<UrlLoaderNetworkServiceObserver> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_BASE_URL_LOADER_NETWORK_SERVICE_OBSERVER_H_
