// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remote_client_cert_store.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
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

RemoteClientCertStore::CertDetails::CertDetails() = default;
RemoteClientCertStore::CertDetails::CertDetails(CertDetails&& other) = default;
RemoteClientCertStore::CertDetails&
RemoteClientCertStore::CertDetails::operator=(CertDetails&& other) = default;
RemoteClientCertStore::CertDetails::~CertDetails() = default;

RemoteClientCertStore::PendingCertRequest::PendingCertRequest(
    scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
    ClientCertListCallback callback)
    : cert_request_info(std::move(cert_request_info)),
      callback(std::move(callback)) {}

RemoteClientCertStore::PendingCertRequest::PendingCertRequest(
    PendingCertRequest&& other) = default;

RemoteClientCertStore::PendingCertRequest&
RemoteClientCertStore::PendingCertRequest::operator=(
    PendingCertRequest&& other) = default;

RemoteClientCertStore::PendingCertRequest::~PendingCertRequest() = default;

RemoteClientCertStore::RemoteClientCertStore() = default;

RemoteClientCertStore::RemoteClientCertStore(
    GetCertificatesCallback get_certificates_callback)
    : get_certificates_callback_(std::move(get_certificates_callback)) {
  DCHECK(get_certificates_callback_);
}

RemoteClientCertStore::~RemoteClientCertStore() = default;

void RemoteClientCertStore::GetClientCerts(
    scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
    ClientCertListCallback callback) {
  if (!get_certificates_callback_) {
    std::move(callback).Run({});
    return;
  }

  if (fetch_pending_) {
    pending_requests_.emplace_back(std::move(cert_request_info),
                                   std::move(callback));
    return;
  }

  fetch_pending_ = true;
  get_certificates_callback_.Run(base::BindOnce(
      [](base::WeakPtr<RemoteClientCertStore> self,
         scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
         ClientCertListCallback callback, std::vector<CertDetails> certs) {
        if (!self) {
          std::move(callback).Run({});
          return;
        }
        self->fetch_pending_ = false;
        std::vector<PendingCertRequest> pending =
            std::move(self->pending_requests_);
        self->OnCertificatesReceived(std::move(cert_request_info),
                                     std::move(callback), std::move(certs));
        if (!self) {
          return;
        }
        for (auto& request : pending) {
          if (!self) {
            break;
          }
          self->GetClientCerts(std::move(request.cert_request_info),
                               std::move(request.callback));
        }
      },
      weak_factory_.GetWeakPtr(), std::move(cert_request_info),
      std::move(callback)));
}

void RemoteClientCertStore::OnCertificatesReceived(
    scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
    ClientCertListCallback callback,
    std::vector<CertDetails> certs) {
  net::ClientCertIdentityList identities;
  for (auto& cert_details : certs) {
    if (!cert_details.certificate || !cert_details.private_key.is_valid()) {
      continue;
    }

    // Filter by cert_authorities if provided.
    if (cert_request_info && !cert_request_info->cert_authorities.empty()) {
      if (!cert_details.certificate->IsIssuedByEncoded(
              cert_request_info->cert_authorities)) {
        continue;
      }
    }

    identities.push_back(std::make_unique<RemoteClientCertIdentity>(
        std::move(cert_details.certificate),
        std::move(cert_details.provider_name),
        std::move(cert_details.algorithm_preferences),
        std::move(cert_details.private_key)));
  }

  std::sort(identities.begin(), identities.end(),
            net::ClientCertIdentitySorter());
  std::move(callback).Run(std::move(identities));
}

}  // namespace remoting
