// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_REMOTE_CLIENT_CERT_STORE_H_
#define REMOTING_HOST_REMOTE_CLIENT_CERT_STORE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/ssl/client_cert_store.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom-forward.h"

namespace remoting {

// Implements net::ClientCertStore by querying client certificates and
// delegating SSL private key signing operations via
// network::mojom::SSLPrivateKey.
class RemoteClientCertStore : public net::ClientCertStore {
 public:
  struct CertDetails {
    CertDetails();
    CertDetails(CertDetails&& other);
    CertDetails& operator=(CertDetails&& other);
    ~CertDetails();

    scoped_refptr<net::X509Certificate> certificate;
    std::string provider_name;
    std::vector<uint16_t> algorithm_preferences;
    mojo::PendingRemote<network::mojom::SSLPrivateKey> private_key;
  };

  using GetCertificatesCallback = base::RepeatingCallback<void(
      base::OnceCallback<void(std::vector<CertDetails>)>)>;

  RemoteClientCertStore();
  explicit RemoteClientCertStore(
      GetCertificatesCallback get_certificates_callback);

  RemoteClientCertStore(const RemoteClientCertStore&) = delete;
  RemoteClientCertStore& operator=(const RemoteClientCertStore&) = delete;

  ~RemoteClientCertStore() override;

  // net::ClientCertStore implementation:
  void GetClientCerts(
      scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
      ClientCertListCallback callback) override;

 private:
  struct PendingCertRequest {
    PendingCertRequest(
        scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
        ClientCertListCallback callback);
    PendingCertRequest(PendingCertRequest&& other);
    PendingCertRequest& operator=(PendingCertRequest&& other);
    ~PendingCertRequest();

    scoped_refptr<const net::SSLCertRequestInfo> cert_request_info;
    ClientCertListCallback callback;
  };

  void OnCertificatesReceived(
      scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
      ClientCertListCallback callback,
      std::vector<CertDetails> certs);

  GetCertificatesCallback get_certificates_callback_;
  bool fetch_pending_ = false;
  std::vector<PendingCertRequest> pending_requests_;
  base::WeakPtrFactory<RemoteClientCertStore> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_REMOTE_CLIENT_CERT_STORE_H_
