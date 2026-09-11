// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remote_client_cert_store.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/client_cert_identity.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "remoting/host/remote_ssl_private_key.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"

namespace remoting {

namespace {

// Implements net::ClientCertIdentity wrapping a certificate and private key
// remote.
class RemoteClientCertIdentity : public net::ClientCertIdentity {
 public:
  RemoteClientCertIdentity(
      scoped_refptr<net::X509Certificate> cert,
      std::string provider_name,
      std::vector<uint16_t> algorithm_preferences,
      mojo::PendingRemote<network::mojom::SSLPrivateKey> private_key)
      : net::ClientCertIdentity(std::move(cert)),
        provider_name_(std::move(provider_name)),
        algorithm_preferences_(std::move(algorithm_preferences)),
        private_key_(std::move(private_key)) {}

  RemoteClientCertIdentity(const RemoteClientCertIdentity&) = delete;
  RemoteClientCertIdentity& operator=(const RemoteClientCertIdentity&) = delete;

  ~RemoteClientCertIdentity() override = default;

  void AcquirePrivateKey(
      base::OnceCallback<void(scoped_refptr<net::SSLPrivateKey>)>
          private_key_callback) override {
    if (!private_key_.is_valid()) {
      LOG(ERROR) << "Private key remote was already acquired or is invalid.";
      std::move(private_key_callback).Run(nullptr);
      return;
    }

    auto ssl_key = base::MakeRefCounted<RemoteSSLPrivateKey>(
        provider_name_, algorithm_preferences_, std::move(private_key_));
    std::move(private_key_callback).Run(std::move(ssl_key));
  }

 private:
  std::string provider_name_;
  std::vector<uint16_t> algorithm_preferences_;
  mojo::PendingRemote<network::mojom::SSLPrivateKey> private_key_;
};

}  // namespace

RemoteClientCertStore::RemoteClientCertStore(
    mojo::PendingAssociatedRemote<mojom::CertificateBroker> broker_remote) {
  if (broker_remote.is_valid()) {
    broker_.Bind(std::move(broker_remote));
  }
}

RemoteClientCertStore::~RemoteClientCertStore() = default;

void RemoteClientCertStore::GetClientCerts(
    scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
    ClientCertListCallback callback) {
  if (!broker_.is_bound()) {
    std::move(callback).Run({});
    return;
  }

  // Wrap the callback to return an empty list if the Mojo pipe disconnects
  // while the store is alive. If `this` is destroyed, `weak_factory_`
  // invalidates the WeakPtr so the callback is discarded without running,
  // matching net::ClientCertStore's cancellation contract.
  broker_->GetCertificates(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      base::BindOnce(&RemoteClientCertStore::OnCertificatesReceived,
                     weak_factory_.GetWeakPtr(), std::move(cert_request_info),
                     std::move(callback)),
      std::vector<mojom::ClientCertificateDetailsPtr>()));
}

void RemoteClientCertStore::OnCertificatesReceived(
    scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
    ClientCertListCallback callback,
    std::vector<mojom::ClientCertificateDetailsPtr> certs) {
  net::ClientCertIdentityList identities;
  for (auto& cert_details : certs) {
    if (!cert_details || !cert_details->certificate ||
        !cert_details->private_key.is_valid()) {
      continue;
    }

    // Filter by cert_authorities if provided.
    if (cert_request_info && !cert_request_info->cert_authorities.empty()) {
      if (!cert_details->certificate->IsIssuedByEncoded(
              cert_request_info->cert_authorities)) {
        continue;
      }
    }

    identities.push_back(std::make_unique<RemoteClientCertIdentity>(
        std::move(cert_details->certificate),
        std::move(cert_details->provider_name),
        std::move(cert_details->algorithm_preferences),
        std::move(cert_details->private_key)));
  }

  std::sort(identities.begin(), identities.end(),
            net::ClientCertIdentitySorter());
  std::move(callback).Run(std::move(identities));
}

}  // namespace remoting
