// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_CLIENT_CERT_STORE_MAC_H_
#define NET_SSL_CLIENT_CERT_STORE_MAC_H_

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "net/base/net_export.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_cert_request_info.h"

namespace net {

class ClientCertIdentityMac;

class NET_EXPORT ClientCertStoreMac : public ClientCertStore {
 public:
  ClientCertStoreMac();

  ClientCertStoreMac(const ClientCertStoreMac&) = delete;
  ClientCertStoreMac& operator=(const ClientCertStoreMac&) = delete;

  ~ClientCertStoreMac() override;

  // ClientCertStore:
  void GetClientCerts(scoped_refptr<const SSLCertRequestInfo> cert_request_info,
                      ClientCertListCallback callback) override;

 private:
  // TODO(crbug.com/40825523): Improve test coverage and remove/reduce
  // the friend tests and ForTesting methods.
  friend class ClientCertStoreMacTest;
  friend class ClientCertStoreMacTestDelegate;

  void OnClientCertsResponse(ClientCertListCallback callback,
                             ClientCertIdentityList identities);

  // A hook for testing. Filters |input_identities| using the logic being used
  // to filter the system store when GetClientCerts() is called. Implemented by
  // creating a list of certificates that otherwise would be extracted from the
  // system store and filtering it using the common logic (less adequate than
  // the approach used on Windows).
  bool SelectClientCertsForTesting(
      std::vector<std::unique_ptr<ClientCertIdentityMac>> input_identities,
      const SSLCertRequestInfo& cert_request_info,
      ClientCertIdentityList* selected_identities);

  // Testing hook specific to Mac, where the internal logic recognizes preferred
  // certificates for particular domains. If the preferred certificate is
  // present in the output list (i.e. it doesn't get filtered out), it should
  // always come first.
  bool SelectClientCertsGivenPreferredForTesting(
      std::unique_ptr<ClientCertIdentityMac> preferred_identity,
      std::vector<std::unique_ptr<ClientCertIdentityMac>> regular_identities,
      const SSLCertRequestInfo& request,
      ClientCertIdentityList* selected_identities);

  base::WeakPtrFactory<ClientCertStoreMac> weak_factory_{this};
};

}  // namespace net

#endif  // NET_SSL_CLIENT_CERT_STORE_MAC_H_
