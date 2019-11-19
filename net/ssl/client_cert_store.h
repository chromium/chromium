// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_CLIENT_CERT_STORE_H_
#define NET_SSL_CLIENT_CERT_STORE_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "net/base/net_export.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/client_cert_identity.h"

namespace net {

class SSLCertRequestInfo;

// A handle to a client certificate store to query matching certificates when a
// server requests client auth. Note that there may be multiple ClientCertStore
// objects corresponding to the same platform certificate store; each request
// gets its own uniquely owned handle.
class NET_EXPORT ClientCertStore {
 public:
  virtual ~ClientCertStore() {}

  using ClientCertListCallback =
      base::OnceCallback<void(ClientCertIdentityList)>;

  // Get client certs matching the |cert_request_info| and pass them to the
  // |callback|.  The |callback| may be called sychronously. The caller must
  // ensure the ClientCertStore and |cert_request_info| remain alive until the
  // callback has been run.
  virtual void GetClientCerts(const SSLCertRequestInfo& cert_request_info,
                              ClientCertListCallback callback) = 0;

 protected:
  ClientCertStore() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ClientCertStore);
};

}  // namespace net

#endif  // NET_SSL_CLIENT_CERT_STORE_H_
