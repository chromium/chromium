// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_REVOCATION_BUILDER_H_
#define NET_TEST_REVOCATION_BUILDER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/pki/ocsp.h"
#include "third_party/boringssl/src/pki/ocsp_revocation_status.h"
#include "third_party/boringssl/src/pki/signature_algorithm.h"

namespace net {

struct OCSPBuilderSingleResponse {
  // OCSP allows the OCSP responder and certificate issuer to be different,
  // but this implementation currently assumes they are the same, thus issuer
  // is not specified here.
  //
  // This implementation currently requires serial to be an unsigned 64 bit
  // integer.
  uint64_t serial;
  bssl::OCSPRevocationStatus cert_status;
  base::Time revocation_time;  // Only used if |cert_status|==REVOKED.
  base::Time this_update;
  // nextUpdate is optional, but this implementation currently always encodes
  // it.
  base::Time next_update;
  // singleExtensions not currently supported.
};

// Creates an bssl::OCSPResponse indicating a |response_status| error, which
// must not be ResponseStatus::SUCCESSFUL.
std::string BuildOCSPResponseError(
    bssl::OCSPResponse::ResponseStatus response_status);

// Creates an bssl::OCSPResponse from responder with DER subject
// |responder_subject| and public key |responder_key|, containing |responses|.
std::string BuildOCSPResponse(
    const std::string& responder_subject,
    EVP_PKEY* responder_key,
    base::Time produced_at,
    const std::vector<OCSPBuilderSingleResponse>& responses);

// Creates an bssl::OCSPResponse signed by |responder_key| with
// |tbs_response_data| as the to-be-signed ResponseData. If
// |signature_algorithm| is nullopt, a default algorithm will be chosen based on
// the key type.
std::string BuildOCSPResponseWithResponseData(
    EVP_PKEY* responder_key,
    const std::string& response_data,
    std::optional<bssl::SignatureAlgorithm> signature_algorithm = std::nullopt);

// Creates a CRL issued by |crl_issuer_subject| and signed by |crl_issuer_key|,
// marking |revoked_serials| as revoked. If |signature_algorithm| is nullopt, a
// default algorithm will be chosen based on the key type.
// Returns the DER-encoded CRL.
std::string BuildCrl(
    const std::string& crl_issuer_subject,
    EVP_PKEY* crl_issuer_key,
    const std::vector<uint64_t>& revoked_serials,
    std::optional<bssl::SignatureAlgorithm> signature_algorithm = std::nullopt);

std::string BuildCrlWithAlgorithmTlvAndDigest(
    const std::string& crl_issuer_subject,
    EVP_PKEY* crl_issuer_key,
    const std::vector<uint64_t>& revoked_serials,
    const std::string& signature_algorithm_tlv,
    const EVP_MD* digest);

}  // namespace net

#endif  // NET_TEST_REVOCATION_BUILDER_H_
