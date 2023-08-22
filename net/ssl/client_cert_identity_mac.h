// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_CLIENT_CERT_IDENTITY_MAC_H_
#define NET_SSL_CLIENT_CERT_IDENTITY_MAC_H_

#include "net/ssl/client_cert_identity.h"

#include <Security/SecBase.h>

#include "base/apple/scoped_cftyperef.h"
#include "net/base/net_export.h"

namespace net {

class NET_EXPORT_PRIVATE ClientCertIdentityMac : public ClientCertIdentity {
 public:
  ClientCertIdentityMac(
      scoped_refptr<net::X509Certificate> cert,
      base::apple::ScopedCFTypeRef<SecIdentityRef> sec_identity);
  ~ClientCertIdentityMac() override;

  SecIdentityRef sec_identity_ref() const { return identity_.get(); }

  void AcquirePrivateKey(base::OnceCallback<void(scoped_refptr<SSLPrivateKey>)>
                             private_key_callback) override;

 private:
  base::apple::ScopedCFTypeRef<SecIdentityRef> identity_;
};

}  // namespace net

#endif  // NET_SSL_CLIENT_CERT_IDENTITY_MAC_H_
