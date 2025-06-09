// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_TWO_QWAC_CERT_BINDING_BUILDER_H_
#define NET_TEST_TWO_QWAC_CERT_BINDING_BUILDER_H_

#include <optional>
#include <vector>

#include "base/values.h"
#include "crypto/hash.h"
#include "net/cert/two_qwac.h"
#include "net/test/cert_builder.h"

namespace net {

// TwoQwacCertBindingBuilder is a helper class to create a 2-QWAC TLS
// Certificate Binding. The builder sets a minimum set of values so that its
// default value will build a valid 2-QWAC JWS. The default instantiation will
// bind dummy cert data, generally the caller should also use `SetBoundCerts`
// to set the TLS certs that are bound.
class TwoQwacCertBindingBuilder {
 public:
  TwoQwacCertBindingBuilder();
  ~TwoQwacCertBindingBuilder();

  void SetJwsSigAlg(JwsSigAlg sig_alg) {
    sig_alg_ = sig_alg;
    GenerateKeyForSigAlg();
  }

  void SetHashAlg(crypto::hash::HashKind hash_alg) {
    hash_alg_ = hash_alg;
    Invalidate();
  }

  // Set the certificates that are bound, as a vector of DER encoded
  // certificates.
  void SetBoundCerts(std::vector<std::string> bound_certs) {
    bound_certs_ = bound_certs;
    Invalidate();
  }

  // Set values to override in the JWS header.
  void SetHeaderOverrides(base::DictValue header_overrides) {
    Invalidate();
    header_overrides_ = std::move(header_overrides);
  }

  // Returns a pointer to the leaf net::CertBuilder. The caller may modify the
  // returned CertBuilder, but only immediately after calling this method and
  // before calling any other methods on the TwoQwacCertBindingBuilder. Once
  // other methods are called on the 2-QWAC builder, do not make further
  // changes to the CertBuilder without calling this method again.
  CertBuilder* GetLeafBuilder() {
    Invalidate();
    return cert_chain_[0].get();
  }

  // Returns a pointer to the root net::CertBuilder. See comment for
  // `GetLeafBuilder` for restrictions on modifying the returned CertBuilder.
  net::CertBuilder* GetRootBuilder() {
    Invalidate();
    return cert_chain_.back().get();
  }

  std::string GetJWS() { return GetHeader() + ".." + GetSignature(); }

  std::string GetJWSWithInvalidSignature() {
    return GetHeader() + ".." + GetInvalidSignature();
  }

  const std::string& GetHeader() {
    if (!header_b64_.has_value()) {
      GenerateHeader();
    }
    return *header_b64_;
  }

  const std::string& GetSignature() {
    if (!signature_b64_.has_value()) {
      GenerateSignature();
    }
    return *signature_b64_;
  }

  const std::string GetInvalidSignature() {
    std::string signature = GetSignature();
    // Mess with the base64url-encoded signature to make it invalid.
    if (signature[0] != 'A') {
      signature[0] = 'A';
    } else {
      signature[0] = 'B';
    }
    return signature;
  }

 private:
  void GenerateKeyForSigAlg();

  std::string SigAlg() const;

  std::string HashAlg() const;

  base::ListValue GenerateX5cHeaderValue();

  base::DictValue GenerateSigDHeaderValue();

  void GenerateHeader();

  void GenerateSignature();

  void Invalidate() {
    header_b64_.reset();
    signature_b64_.reset();
  }

  std::vector<std::unique_ptr<CertBuilder>> cert_chain_;
  std::vector<std::string> bound_certs_;
  base::DictValue header_overrides_;
  JwsSigAlg sig_alg_ = JwsSigAlg::kEcdsaP256Sha256;
  crypto::hash::HashKind hash_alg_ = crypto::hash::kSha256;
  // The header and signature are lazily built, and if any inputs to the builder
  // are possibly modified, then they are cleared.
  std::optional<std::string> header_b64_;
  std::optional<std::string> signature_b64_;
};

}  // namespace net

#endif  // NET_TEST_TWO_QWAC_CERT_BINDING_BUILDER_H_
