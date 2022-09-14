// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_CLIENT_CERT_STORE_WIN_H_
#define NET_SSL_CLIENT_CERT_STORE_WIN_H_

#include "base/callback.h"
#include "base/win/wincrypt_shim.h"
#include "crypto/scoped_capi_types.h"
#include "net/base/net_export.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_cert_request_info.h"

namespace net {

class NET_EXPORT ClientCertStoreWin : public ClientCertStore {
 public:
  // Uses the "MY" current user system certificate store.
  ClientCertStoreWin();

  // Calls |cert_store_callback| on the platform key thread to determine the
  // certificate store.
  explicit ClientCertStoreWin(
      base::RepeatingCallback<crypto::ScopedHCERTSTORE()> cert_store_callback);

  ClientCertStoreWin(const ClientCertStoreWin&) = delete;
  ClientCertStoreWin& operator=(const ClientCertStoreWin&) = delete;

  ~ClientCertStoreWin() override;

  // If a cert store has been provided at construction time GetClientCerts
  // will use that. Otherwise it will use the current user's "MY" cert store
  // instead.
  void GetClientCerts(const SSLCertRequestInfo& cert_request_info,
                      ClientCertListCallback callback) override;

 private:
  friend class ClientCertStoreWinTestDelegate;

  // Opens the cert store and uses it to lookup the client certs.
  static ClientCertIdentityList GetClientCertsWithCertStore(
      const SSLCertRequestInfo& request,
      const base::RepeatingCallback<crypto::ScopedHCERTSTORE()>&
          cert_store_callback);

  // A hook for testing. Filters |input_certs| using the logic being used to
  // filter the system store when GetClientCerts() is called.
  // Implemented by creating a temporary in-memory store and filtering it
  // using the common logic.
  bool SelectClientCertsForTesting(const CertificateList& input_certs,
                                   const SSLCertRequestInfo& cert_request_info,
                                   ClientCertIdentityList* selected_identities);

  base::RepeatingCallback<crypto::ScopedHCERTSTORE()> cert_store_callback_;
};

}  // namespace net

#endif  // NET_SSL_CLIENT_CERT_STORE_WIN_H_
