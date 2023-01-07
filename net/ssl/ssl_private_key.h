// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_SSL_PRIVATE_KEY_H_
#define NET_SSL_SSL_PRIVATE_KEY_H_

#include <stdint.h>

#include <vector>

#include "base/callback_forward.h"
#include "base/containers/span.h"
#include "base/memory/ref_counted.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"

namespace net {

// An interface for a private key for use with SSL client authentication. A
// private key may be used with multiple signature algorithms, so methods use
// |SSL_SIGN_*| constants from BoringSSL, which correspond to TLS 1.3
// SignatureScheme values.
//
// Note that although ECDSA constants are named like
// |SSL_SIGN_ECDSA_SECP256R1_SHA256|, they may be used with any curve for
// purposes of this API. This descrepancy is due to differences between TLS 1.2
// and TLS 1.3.
class NET_EXPORT SSLPrivateKey
    : public base::RefCountedThreadSafe<SSLPrivateKey> {
 public:
  using SignCallback =
      base::OnceCallback<void(Error, const std::vector<uint8_t>&)>;

  SSLPrivateKey() = default;

  SSLPrivateKey(const SSLPrivateKey&) = delete;
  SSLPrivateKey& operator=(const SSLPrivateKey&) = delete;

  // Returns a human-readable name of the provider that backs this
  // SSLPrivateKey, for debugging. If not applicable or available, return the
  // empty string.
  virtual std::string GetProviderName() = 0;

  // Returns the algorithms that are supported by the key in decreasing
  // preference for TLS 1.2 and later. Note that |SSL_SIGN_RSA_PKCS1_MD5_SHA1|
  // is only used by TLS 1.1 and earlier and should not be in this list.
  virtual std::vector<uint16_t> GetAlgorithmPreferences() = 0;

  // Asynchronously signs an |input| with the specified TLS signing algorithm.
  // |input| is an unhashed message to be signed. On completion, it calls
  // |callback| with the signature or an error code if the operation failed.
  virtual void Sign(uint16_t algorithm,
                    base::span<const uint8_t> input,
                    SignCallback callback) = 0;

  // Returns the default signature algorithm preferences for the specified key
  // type, which should be a BoringSSL |EVP_PKEY_*| constant. RSA keys which use
  // this must support PKCS #1 v1.5 signatures with SHA-1, SHA-256, SHA-384, and
  // SHA-512. If |supports_pss| is true, they must additionally support PSS
  // signatures with SHA-256, SHA-384, and SHA-512. ECDSA keys must support
  // SHA-256, SHA-384, SHA-512.
  //
  // Keys with more specific capabilities or preferences should return a custom
  // list.
  static std::vector<uint16_t> DefaultAlgorithmPreferences(int type,
                                                           bool supports_pss);

 protected:
  virtual ~SSLPrivateKey() = default;

 private:
  friend class base::RefCountedThreadSafe<SSLPrivateKey>;
};

}  // namespace net

#endif  // NET_SSL_SSL_PRIVATE_KEY_H_
