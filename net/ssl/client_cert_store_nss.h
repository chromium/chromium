// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_CLIENT_CERT_STORE_NSS_H_
#define NET_SSL_CLIENT_CERT_STORE_NSS_H_

#include "base/callback.h"
#include "base/macros.h"
#include "net/base/net_export.h"
#include "net/ssl/client_cert_store.h"

typedef struct CERTCertListStr CERTCertList;
typedef struct CERTCertificateStr CERTCertificate;

namespace crypto {
class CryptoModuleBlockingPasswordDelegate;
}

namespace net {
class HostPortPair;
class SSLCertRequestInfo;

class NET_EXPORT ClientCertStoreNSS : public ClientCertStore {
 public:
  typedef base::RepeatingCallback<crypto::CryptoModuleBlockingPasswordDelegate*(
      const HostPortPair& /* server */)>
      PasswordDelegateFactory;

  using CertFilter = base::RepeatingCallback<bool(CERTCertificate*)>;

  explicit ClientCertStoreNSS(
      const PasswordDelegateFactory& password_delegate_factory);
  ~ClientCertStoreNSS() override;

  // ClientCertStore:
  void GetClientCerts(const SSLCertRequestInfo& cert_request_info,
                      ClientCertListCallback callback) override;

  // Examines the certificates in |identities| to find all certificates that
  // match the client certificate request in |request|, removing any that don't.
  // The remaining certs will be updated to include intermediates.
  // Must be called from a worker thread.
  static void FilterCertsOnWorkerThread(ClientCertIdentityList* identities,
                                        const SSLCertRequestInfo& request);

  // Retrieves all client certificates that are stored by NSS and adds them to
  // |identities|. |password_delegate| is used to unlock slots if required. If
  // |cert_filter| is not null, only certificates that it returns true on will
  // be added.
  // Must be called from a worker thread.
  static void GetPlatformCertsOnWorkerThread(
      scoped_refptr<crypto::CryptoModuleBlockingPasswordDelegate>
          password_delegate,
      const CertFilter& cert_filter,
      ClientCertIdentityList* identities);

 private:
  ClientCertIdentityList GetAndFilterCertsOnWorkerThread(
      scoped_refptr<crypto::CryptoModuleBlockingPasswordDelegate>
          password_delegate,
      const SSLCertRequestInfo* request);

  // The factory for creating the delegate for requesting a password to a
  // PKCS#11 token. May be null.
  PasswordDelegateFactory password_delegate_factory_;

  DISALLOW_COPY_AND_ASSIGN(ClientCertStoreNSS);
};

}  // namespace net

#endif  // NET_SSL_CLIENT_CERT_STORE_NSS_H_
