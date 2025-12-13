// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_CLIENT_CERT_STORE_EMPTY_H_
#define NET_SSL_CLIENT_CERT_STORE_EMPTY_H_

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "net/base/net_export.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_cert_request_info.h"

namespace net {

// ClientCertStore implementation that always returns an empty list. The
// CertificateProvisioningService implementation expects to wrap a platform
// cert store, but sometimes we only want to get results from the provisioning
// service itself, so instead of a platform cert store we pass an
// implementation that always returns an empty result when queried.
class NET_EXPORT ClientCertStoreEmpty : public ClientCertStore {
 public:
  ClientCertStoreEmpty();

  ClientCertStoreEmpty(const ClientCertStoreEmpty&) = delete;
  ClientCertStoreEmpty& operator=(const ClientCertStoreEmpty&) = delete;

  ~ClientCertStoreEmpty() override;

  // ClientCertStore:
  void GetClientCerts(scoped_refptr<const SSLCertRequestInfo> cert_request_info,
                      ClientCertListCallback callback) override;
};

}  // namespace net

#endif  // NET_SSL_CLIENT_CERT_STORE_EMPTY_H_
