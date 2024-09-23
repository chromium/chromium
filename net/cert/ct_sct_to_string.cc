// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ct_sct_to_string.h"

#include "base/logging.h"
#include "base/notreached.h"

namespace net::ct {

const std::string HashAlgorithmToString(
    DigitallySigned::HashAlgorithm hashAlgorithm) {
  switch (hashAlgorithm) {
    case DigitallySigned::HASH_ALGO_NONE:
      return "None / invalid";
    case DigitallySigned::HASH_ALGO_MD5:
      return "MD5";
    case DigitallySigned::HASH_ALGO_SHA1:
      return "SHA-1";
    case DigitallySigned::HASH_ALGO_SHA224:
      return "SHA-224";
    case DigitallySigned::HASH_ALGO_SHA256:
      return "SHA-256";
    case DigitallySigned::HASH_ALGO_SHA384:
      return "SHA-384";
    case DigitallySigned::HASH_ALGO_SHA512:
      return "SHA-512";
  }
  return "Unknown";
}

const std::string SignatureAlgorithmToString(
    DigitallySigned::SignatureAlgorithm signatureAlgorithm) {
  switch (signatureAlgorithm) {
    case DigitallySigned::SIG_ALGO_ANONYMOUS:
      return "Anonymous";
    case DigitallySigned::SIG_ALGO_RSA:
      return "RSA";
    case DigitallySigned::SIG_ALGO_DSA:
      return "DSA";
    case DigitallySigned::SIG_ALGO_ECDSA:
      return "ECDSA";
  }
  return "Unknown";
}

const std::string OriginToString(SignedCertificateTimestamp::Origin origin) {
  switch (origin) {
    case SignedCertificateTimestamp::SCT_EMBEDDED:
      return "Embedded in certificate";
    case SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION:
      return "TLS extension";
    case SignedCertificateTimestamp::SCT_FROM_OCSP_RESPONSE:
      return "OCSP";
    case SignedCertificateTimestamp::SCT_ORIGIN_MAX:
      NOTREACHED_IN_MIGRATION();
  }
  return "Unknown";
}

const std::string StatusToString(SCTVerifyStatus status) {
  switch (status) {
    case SCT_STATUS_LOG_UNKNOWN:
      return "From unknown log";
    case SCT_STATUS_INVALID_SIGNATURE:
      return "Invalid signature";
    case SCT_STATUS_OK:
      return "Verified";
    case SCT_STATUS_NONE:
      return "None";
    case SCT_STATUS_INVALID_TIMESTAMP:
      return "Invalid timestamp";
  }
  return "Unknown";
}

}  // namespace net::ct
