// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_CLIENT_CERT_IDENTITY_TEST_UTIL_H_
#define NET_SSL_CLIENT_CERT_IDENTITY_TEST_UTIL_H_

#include "net/ssl/client_cert_identity.h"

namespace base {
class FilePath;
}

namespace net {

// Simple ClientCertIdentity implementation for testing.
// Note: this implementation of AcquirePrivateKey will always call the callback
// synchronously.
class FakeClientCertIdentity : public ClientCertIdentity {
 public:
  FakeClientCertIdentity(scoped_refptr<X509Certificate> cert,
                         scoped_refptr<SSLPrivateKey> key);
  ~FakeClientCertIdentity() override;

  // Creates a FakeClientCertIdentity from a certificate file (DER or PEM) and
  // private key file (unencrypted pkcs8). Returns nullptr on error.
  static std::unique_ptr<FakeClientCertIdentity> CreateFromCertAndKeyFiles(
      const base::FilePath& dir,
      const std::string& cert_filename,
      const std::string& key_filename);

  // Creates a FakeClientCertIdentity from a certificate file (DER or PEM).
  // Signing attempts will fail. Returns nullptr on error.
  static std::unique_ptr<FakeClientCertIdentity> CreateFromCertAndFailSigning(
      const base::FilePath& dir,
      const std::string& cert_filename);

  // Duplicates the FakeClientCertIdentity.
  std::unique_ptr<FakeClientCertIdentity> Copy();

  // Returns the SSLPrivateKey in a more convenient way, for tests.
  SSLPrivateKey* ssl_private_key() const { return key_.get(); }

  // ClientCertIdentity implementation:
  void AcquirePrivateKey(base::OnceCallback<void(scoped_refptr<SSLPrivateKey>)>
                             private_key_callback) override;
#if defined(OS_MACOSX)
  SecIdentityRef sec_identity_ref() const override;
#endif

 private:
  scoped_refptr<SSLPrivateKey> key_;
};

// Converts a CertificateList to a ClientCertIdentityList of
// FakeClientCertIdentity, with null private keys.
ClientCertIdentityList FakeClientCertIdentityListFromCertificateList(
    const CertificateList& certs);

}  // namespace net

#endif  // NET_SSL_CLIENT_CERT_IDENTITY_TEST_UTIL_H_
