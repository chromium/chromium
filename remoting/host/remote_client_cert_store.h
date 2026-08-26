// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_REMOTE_CLIENT_CERT_STORE_H_
#define REMOTING_HOST_REMOTE_CLIENT_CERT_STORE_H_

#include "net/ssl/client_cert_store.h"

namespace remoting {

// Implements net::ClientCertStore by querying client certificates and
// delegating SSL private key signing operations to the elevated daemon process.
class RemoteClientCertStore : public net::ClientCertStore {
 public:
  RemoteClientCertStore();

  RemoteClientCertStore(const RemoteClientCertStore&) = delete;
  RemoteClientCertStore& operator=(const RemoteClientCertStore&) = delete;

  ~RemoteClientCertStore() override;

  // net::ClientCertStore implementation:
  void GetClientCerts(
      scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
      ClientCertListCallback callback) override;
};

}  // namespace remoting

#endif  // REMOTING_HOST_REMOTE_CLIENT_CERT_STORE_H_
