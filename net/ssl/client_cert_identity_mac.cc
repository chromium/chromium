// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/client_cert_identity_mac.h"

#include "net/ssl/ssl_platform_key_mac.h"
#include "net/ssl/ssl_private_key.h"

namespace net {

ClientCertIdentityMac::ClientCertIdentityMac(
    scoped_refptr<net::X509Certificate> cert,
    base::ScopedCFTypeRef<SecIdentityRef> sec_identity)
    : ClientCertIdentity(std::move(cert)), identity_(std::move(sec_identity)) {}

ClientCertIdentityMac::~ClientCertIdentityMac() = default;

void ClientCertIdentityMac::AcquirePrivateKey(
    base::OnceCallback<void(scoped_refptr<SSLPrivateKey>)>
        private_key_callback) {
  // This only adds a ref to and returns the private key from identity_ so it
  // doesn't need to run on a worker thread.
  std::move(private_key_callback)
      .Run(CreateSSLPrivateKeyForSecIdentity(certificate(), identity_.get()));
}

SecIdentityRef ClientCertIdentityMac::sec_identity_ref() const {
  return identity_.get();
}

}  // namespace net
