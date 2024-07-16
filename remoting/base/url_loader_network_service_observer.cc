// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/url_loader_network_service_observer.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/net_errors.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_private_key.h"
#include "net/url_request/url_request.h"
#include "remoting/base/certificate_helpers.h"
#include "remoting/base/logging.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace remoting {

namespace {

constexpr char kCertIssuerWildCard[] = "*";

class SSLPrivateKeyWrapper : public network::mojom::SSLPrivateKey {
 public:
  explicit SSLPrivateKeyWrapper(
      scoped_refptr<net::SSLPrivateKey> ssl_private_key)
      : ssl_private_key_(std::move(ssl_private_key)) {}

  SSLPrivateKeyWrapper(const SSLPrivateKeyWrapper&) = delete;
  SSLPrivateKeyWrapper& operator=(const SSLPrivateKeyWrapper&) = delete;

  ~SSLPrivateKeyWrapper() override = default;

  // network::mojom::SSLPrivateKey implementation.
  void Sign(uint16_t algorithm,
            const std::vector<uint8_t>& input,
            network::mojom::SSLPrivateKey::SignCallback callback) override {
    base::span<const uint8_t> input_span(input);
    ssl_private_key_->Sign(
        algorithm, input_span,
        base::BindOnce(&SSLPrivateKeyWrapper::Callback,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

 private:
  void Callback(network::mojom::SSLPrivateKey::SignCallback callback,
                net::Error net_error,
                const std::vector<uint8_t>& signature) {
    std::move(callback).Run(static_cast<int32_t>(net_error), signature);
  }

  scoped_refptr<net::SSLPrivateKey> ssl_private_key_;
  base::WeakPtrFactory<SSLPrivateKeyWrapper> weak_ptr_factory_{this};
};

void PrintCertificateDetails(const net::X509Certificate& cert) {
  // Formatted to make log output more readable.
  HOST_LOG << "\n  Client certificate details:\n"
           << "    issued by: " << GetPreferredIssuerFieldValue(cert) << "\n"
           << "    with start date: " << cert.valid_start() << "\n"
           << "    and expiry date: " << cert.valid_expiry();
}

}  // namespace

UrlLoaderNetworkServiceObserver::UrlLoaderNetworkServiceObserver() = default;
UrlLoaderNetworkServiceObserver::~UrlLoaderNetworkServiceObserver() = default;

mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
UrlLoaderNetworkServiceObserver::Bind() {
  mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
      pending_remote;
  receivers_.Add(this, pending_remote.InitWithNewPipeAndPassReceiver());
  return pending_remote;
}

void UrlLoaderNetworkServiceObserver::OnSSLCertificateError(
    const GURL& url,
    int net_error,
    const net::SSLInfo& ssl_info,
    bool fatal,
    OnSSLCertificateErrorCallback response) {
  std::move(response).Run(net_error);
}

void UrlLoaderNetworkServiceObserver::OnCertificateRequested(
    const std::optional<base::UnguessableToken>& window_id,
    const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
    mojo::PendingRemote<network::mojom::ClientCertificateResponder>
        client_cert_responder) {
  std::unique_ptr<net::ClientCertStore> client_cert_store =
      CreateClientCertStoreInstance();
  net::ClientCertStore* const temp_client_cert_store = client_cert_store.get();

  // Note: |client_cert_store| and |cert_info| are bound to the callback as it
  // is the caller's responsibility to keep them alive until the callback has
  // been run by ClientCertStore.
  temp_client_cert_store->GetClientCerts(
      cert_info,
      base::BindOnce(&UrlLoaderNetworkServiceObserver::OnCertificatesSelected,
                     weak_factory_.GetWeakPtr(),
                     std::move(client_cert_responder),
                     std::move(client_cert_store), cert_info));
}

void UrlLoaderNetworkServiceObserver::OnAuthRequired(
    const std::optional<base::UnguessableToken>& window_id,
    int32_t request_id,
    const GURL& url,
    bool first_auth_attempt,
    const net::AuthChallengeInfo& auth_info,
    const scoped_refptr<net::HttpResponseHeaders>& head_headers,
    mojo::PendingRemote<network::mojom::AuthChallengeResponder>
        auth_challenge_responder) {}

void UrlLoaderNetworkServiceObserver::OnPrivateNetworkAccessPermissionRequired(
    const GURL& url,
    const net::IPAddress& ip_address,
    const std::optional<std::string>& private_network_device_id,
    const std::optional<std::string>& private_network_device_name,
    OnPrivateNetworkAccessPermissionRequiredCallback callback) {}

void UrlLoaderNetworkServiceObserver::OnClearSiteData(
    const GURL& url,
    const std::string& header_value,
    int32_t load_flags,
    const std::optional<net::CookiePartitionKey>& cookie_partition_key,
    bool partitioned_state_allowed_only,
    OnClearSiteDataCallback callback) {
  std::move(callback).Run();
}

void UrlLoaderNetworkServiceObserver::OnLoadingStateUpdate(
    network::mojom::LoadInfoPtr info,
    OnLoadingStateUpdateCallback callback) {
  std::move(callback).Run();
}

void UrlLoaderNetworkServiceObserver::OnDataUseUpdate(
    int32_t network_traffic_annotation_id_hash,
    int64_t recv_bytes,
    int64_t sent_bytes) {}

void UrlLoaderNetworkServiceObserver::OnSharedStorageHeaderReceived(
    const url::Origin& request_origin,
    std::vector<network::mojom::SharedStorageOperationPtr> operations,
    OnSharedStorageHeaderReceivedCallback callback) {
  std::move(callback).Run();
}

void UrlLoaderNetworkServiceObserver::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderNetworkServiceObserver>
        observer) {
  receivers_.Add(this, std::move(observer));
}

void UrlLoaderNetworkServiceObserver::OnWebSocketConnectedToPrivateNetwork(
    network::mojom::IPAddressSpace ip_address_space) {}

void UrlLoaderNetworkServiceObserver::OnCertificatesSelected(
    mojo::PendingRemote<network::mojom::ClientCertificateResponder>
        client_cert_responder,
    std::unique_ptr<net::ClientCertStore> cert_store,
    const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
    net::ClientCertIdentityList selected_certs) {
  base::Time now = base::Time::Now();
  auto best_match =
      GetBestMatchFromCertificateList(kCertIssuerWildCard, now, selected_certs);
  if (!best_match) {
    ContinueWithCertificate(std::move(client_cert_responder), nullptr, nullptr);
    return;
  }

  scoped_refptr<net::X509Certificate> cert = best_match->certificate();
  if (!IsCertificateValid(kCertIssuerWildCard, now, *cert.get())) {
    LOG(ERROR) << "Best client certificate match was not valid.";
    PrintCertificateDetails(*cert.get());
    ContinueWithCertificate(std::move(client_cert_responder), nullptr, nullptr);
    return;
  }

  net::ClientCertIdentity::SelfOwningAcquirePrivateKey(
      std::move(best_match),
      base::BindOnce(&UrlLoaderNetworkServiceObserver::ContinueWithCertificate,
                     weak_factory_.GetWeakPtr(),
                     std::move(client_cert_responder), std::move(cert)));
}

void UrlLoaderNetworkServiceObserver::ContinueWithCertificate(
    mojo::PendingRemote<network::mojom::ClientCertificateResponder>
        client_cert_responder,
    scoped_refptr<net::X509Certificate> client_cert,
    scoped_refptr<net::SSLPrivateKey> private_key) {
  if (client_cert) {
    PrintCertificateDetails(*client_cert);
  }

  mojo::Remote<network::mojom::ClientCertificateResponder> responder(
      std::move(client_cert_responder));

  if (!client_cert || !private_key) {
    responder->ContinueWithoutCertificate();
    return;
  }

  mojo::PendingRemote<network::mojom::SSLPrivateKey> ssl_private_key;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<SSLPrivateKeyWrapper>(private_key),
      ssl_private_key.InitWithNewPipeAndPassReceiver());

  responder->ContinueWithCertificate(
      client_cert, private_key->GetProviderName(),
      private_key->GetAlgorithmPreferences(), std::move(ssl_private_key));
}

}  // namespace remoting
