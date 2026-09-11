// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_REMOTE_CLIENT_CERT_STORE_H_
#define REMOTING_HOST_REMOTE_CLIENT_CERT_STORE_H_

#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "net/ssl/client_cert_store.h"
#include "remoting/host/mojom/remoting_host.mojom.h"

namespace net {
class SSLCertRequestInfo;
}  // namespace net

namespace remoting {

// Implements net::ClientCertStore by querying client certificates and
// delegating SSL private key signing operations to the elevated daemon process
// over Mojo.
class RemoteClientCertStore : public net::ClientCertStore {
 public:
  explicit RemoteClientCertStore(
      mojo::PendingAssociatedRemote<mojom::CertificateBroker> broker_remote);

  RemoteClientCertStore(const RemoteClientCertStore&) = delete;
  RemoteClientCertStore& operator=(const RemoteClientCertStore&) = delete;

  ~RemoteClientCertStore() override;

  // net::ClientCertStore implementation:
  void GetClientCerts(
      scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
      ClientCertListCallback callback) override;

 private:
  void OnCertificatesReceived(
      scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
      ClientCertListCallback callback,
      std::vector<mojom::ClientCertificateDetailsPtr> certs);

  mojo::AssociatedRemote<mojom::CertificateBroker> broker_;
  base::WeakPtrFactory<RemoteClientCertStore> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_REMOTE_CLIENT_CERT_STORE_H_
