// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/certificate_broker_impl.h"

#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_private_key.h"
#include "remoting/base/certificate_helpers.h"
#include "remoting/base/ssl_private_key_wrapper.h"

namespace remoting {

CertificateBrokerImpl::CertificateBrokerImpl()
    : CertificateBrokerImpl(
          base::BindRepeating(&CreateClientCertStoreInstance)) {}

CertificateBrokerImpl::CertificateBrokerImpl(
    CreateClientCertStoreCallback create_client_cert_store_callback)
    : create_client_cert_store_callback_(
          std::move(create_client_cert_store_callback)) {
  DCHECK(create_client_cert_store_callback_);
}

CertificateBrokerImpl::~CertificateBrokerImpl() = default;

void CertificateBrokerImpl::GetCertificates(GetCertificatesCallback callback) {
  auto client_cert_store = create_client_cert_store_callback_.Run();
  if (!client_cert_store) {
    LOG(ERROR) << "Failed to create ClientCertStore instance.";
    std::move(callback).Run({});
    return;
  }

  auto* store_ptr = client_cert_store.get();
  store_ptr->GetClientCerts(
      base::MakeRefCounted<net::SSLCertRequestInfo>(),
      base::BindOnce(&CertificateBrokerImpl::OnClientCertsRetrieved,
                     std::move(callback))
          .Then(base::OnceClosure(
              base::DoNothingWithBoundArgs(std::move(client_cert_store)))));
}

// static
void CertificateBrokerImpl::OnClientCertsRetrieved(
    GetCertificatesCallback callback,
    net::ClientCertIdentityList client_certs) {
  if (client_certs.empty()) {
    std::move(callback).Run({});
    return;
  }

  auto barrier_callback =
      base::BarrierCallback<mojom::ClientCertificateDetailsPtr>(
          client_certs.size(),
          base::BindOnce(
              [](GetCertificatesCallback callback,
                 std::vector<mojom::ClientCertificateDetailsPtr> results) {
                std::vector<mojom::ClientCertificateDetailsPtr> valid_results;
                for (auto& item : results) {
                  if (item) {
                    valid_results.push_back(std::move(item));
                  }
                }
                std::move(callback).Run(std::move(valid_results));
              },
              std::move(callback)));

  for (auto& cert_identity : client_certs) {
    scoped_refptr<net::X509Certificate> cert = cert_identity->certificate();
    net::ClientCertIdentity::SelfOwningAcquirePrivateKey(
        std::move(cert_identity),
        base::BindOnce(
            [](scoped_refptr<net::X509Certificate> cert,
               base::RepeatingCallback<void(mojom::ClientCertificateDetailsPtr)>
                   barrier_cb,
               scoped_refptr<net::SSLPrivateKey> private_key) {
              if (!private_key) {
                LOG(ERROR) << "Failed to acquire private key for certificate.";
                barrier_cb.Run(nullptr);
                return;
              }
              auto details = mojom::ClientCertificateDetails::New();
              details->certificate = std::move(cert);
              details->provider_name = private_key->GetProviderName();
              details->algorithm_preferences =
                  private_key->GetAlgorithmPreferences();
              mojo::MakeSelfOwnedReceiver(
                  std::make_unique<SSLPrivateKeyWrapper>(
                      std::move(private_key)),
                  details->private_key.InitWithNewPipeAndPassReceiver());
              barrier_cb.Run(std::move(details));
            },
            std::move(cert), barrier_callback));
  }
}

}  // namespace remoting
