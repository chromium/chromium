// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/rsa_key_pair.h"

#include <stdint.h>

#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "crypto/sign.h"
#include "net/cert/x509_util.h"

namespace remoting {

RsaKeyPair::RsaKeyPair(crypto::keypair::PrivateKey&& key)
    : key_(std::move(key)) {}

RsaKeyPair::~RsaKeyPair() = default;

// static
scoped_refptr<RsaKeyPair> RsaKeyPair::Generate() {
  return new RsaKeyPair(crypto::keypair::PrivateKey::GenerateRsa2048());
}

// static
scoped_refptr<RsaKeyPair> RsaKeyPair::FromString(
    const std::string& key_base64) {
  std::optional<std::vector<uint8_t>> key_bytes =
      base::Base64Decode(key_base64);
  if (!key_bytes.has_value()) {
    LOG(ERROR) << "Failed to decode private key.";
    return nullptr;
  }

  auto key = crypto::keypair::PrivateKey::FromPrivateKeyInfo(*key_bytes);
  if (!key.has_value()) {
    LOG(ERROR) << "Invalid private key.";
    return nullptr;
  }

  return new RsaKeyPair(std::move(*key));
}

std::string RsaKeyPair::ToString() const {
  return base::Base64Encode(key_.ToPrivateKeyInfo());
}

std::string RsaKeyPair::GetPublicKey() const {
  return base::Base64Encode(key_.ToSubjectPublicKeyInfo());
}

std::string RsaKeyPair::GenerateCertificate() {
  std::string der_cert;
  net::x509_util::CreateSelfSignedCert(
      key_.key(), net::x509_util::DIGEST_SHA256, "CN=chromoting",
      base::RandInt(1, std::numeric_limits<int>::max()), base::Time::Now(),
      base::Time::Now() + base::Days(1), {}, &der_cert);
  return der_cert;
}

}  // namespace remoting
