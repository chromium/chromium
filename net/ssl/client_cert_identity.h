// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_CLIENT_CERT_IDENTITY_H_
#define NET_SSL_CLIENT_CERT_IDENTITY_H_

#include "base/callback.h"
#include "net/base/net_export.h"
#include "net/cert/x509_certificate.h"

#if defined(OS_MACOSX)
#include <Security/SecBase.h>
#endif

namespace base {
class Time;
}

namespace net {

class SSLPrivateKey;

// Represents a client certificate and a promise to retrieve the associated
// private key.
class NET_EXPORT ClientCertIdentity {
 public:
  explicit ClientCertIdentity(scoped_refptr<net::X509Certificate> cert);
  virtual ~ClientCertIdentity();

  // Returns the certificate.
  X509Certificate* certificate() const { return cert_.get(); }

  // Passes the private key to |private_key_callback| on the same sequence
  // AcquirePrivateKey is called on, or nullptr on error. The callback may be
  // run synchronously or asynchronously.  The caller is responsible for
  // keeping the ClientCertIdentity alive until the callback is run.
  virtual void AcquirePrivateKey(
      base::OnceCallback<void(scoped_refptr<SSLPrivateKey>)>
          private_key_callback) = 0;

#if defined(OS_MACOSX)
  // Returns the SecIdentityRef for this identity.
  virtual SecIdentityRef sec_identity_ref() const = 0;
#endif

  // Acquires the private key for |identity|, taking ownership of |identity| so
  // that the caller does not need to manage its lifetime. The other semantics
  // are the same as for AcquirePrivateKey above.
  static void SelfOwningAcquirePrivateKey(
      std::unique_ptr<ClientCertIdentity> identity,
      base::OnceCallback<void(scoped_refptr<SSLPrivateKey>)>
          private_key_callback);

  // Sets the intermediates of |certificate()| to |intermediates|. Note that
  // this will change the value of |certificate()|, and any references that
  // were retained to the previous value will not reflect the updated
  // intermediates list.
  void SetIntermediates(
      std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates);

 private:
  scoped_refptr<net::X509Certificate> cert_;
};

// Comparator for use in STL algorithms that will sort client certificates by
// order of preference.
// Returns true if |a| is more preferable than |b|, allowing it to be used
// with any algorithm that compares according to strict weak ordering.
//
// Criteria include:
// - Prefer certificates that have a longer validity period (later
//   expiration dates)
// - If equal, prefer certificates that were issued more recently
// - If equal, prefer shorter chains (if available)
class NET_EXPORT_PRIVATE ClientCertIdentitySorter {
 public:
  ClientCertIdentitySorter();

  bool operator()(const std::unique_ptr<ClientCertIdentity>& a,
                  const std::unique_ptr<ClientCertIdentity>& b) const;

 private:
  base::Time now_;
};

using ClientCertIdentityList = std::vector<std::unique_ptr<ClientCertIdentity>>;

}  // namespace net

#endif  // NET_SSL_CLIENT_CERT_IDENTITY_H_
