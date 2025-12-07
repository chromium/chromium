// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_RSA_KEY_PAIR_H_
#define REMOTING_BASE_RSA_KEY_PAIR_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "crypto/keypair.h"

namespace remoting {

class RsaKeyPair : public base::RefCountedThreadSafe<RsaKeyPair> {
 public:
  // Generates a new (random) private key.
  static scoped_refptr<RsaKeyPair> Generate();

  // Loads a private key from a base64-encoded string. Returns true on success.
  static scoped_refptr<RsaKeyPair> FromString(const std::string& key_base64);

  RsaKeyPair(const RsaKeyPair&) = delete;
  RsaKeyPair& operator=(const RsaKeyPair&) = delete;

  // Returns a base64 encoded string representing the private key.
  std::string ToString() const;

  // Generates a DER-encoded self-signed certificate using the key pair. Returns
  // empty string if cert generation fails (e.g. it may happen when the system
  // clock is off).
  std::string GenerateCertificate();

  // Returns a base64-encoded string representing the public key.
  std::string GetPublicKey() const;

  EVP_PKEY* private_key() { return key_.key(); }

 private:
  friend class base::RefCountedThreadSafe<RsaKeyPair>;
  explicit RsaKeyPair(crypto::keypair::PrivateKey&& key);
  virtual ~RsaKeyPair();

  crypto::keypair::PrivateKey key_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_RSA_KEY_PAIR_H_
