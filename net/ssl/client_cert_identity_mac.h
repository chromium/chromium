// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_CLIENT_CERT_IDENTITY_MAC_H_
#define NET_SSL_CLIENT_CERT_IDENTITY_MAC_H_

#include "net/ssl/client_cert_identity.h"

#include <Security/SecBase.h>

#include "base/mac/scoped_cftyperef.h"
#include "net/base/net_export.h"

namespace net {

class NET_EXPORT_PRIVATE ClientCertIdentityMac : public ClientCertIdentity {
 public:
  ClientCertIdentityMac(scoped_refptr<net::X509Certificate> cert,
                        base::ScopedCFTypeRef<SecIdentityRef> sec_identity);
  ~ClientCertIdentityMac() override;

  void AcquirePrivateKey(base::OnceCallback<void(scoped_refptr<SSLPrivateKey>)>
                             private_key_callback) override;
  SecIdentityRef sec_identity_ref() const override;

 private:
  base::ScopedCFTypeRef<SecIdentityRef> identity_;
};

}  // namespace net

#endif  // NET_SSL_CLIENT_CERT_IDENTITY_MAC_H_
