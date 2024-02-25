// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/signed_tree_head.h"

#include <string.h>

#include <ostream>

#include "base/strings/string_number_conversions.h"

namespace net::ct {

SignedTreeHead::SignedTreeHead() = default;

SignedTreeHead::SignedTreeHead(Version version,
                               const base::Time& timestamp,
                               uint64_t tree_size,
                               const char sha256_root_hash[kSthRootHashLength],
                               const DigitallySigned& signature,
                               const std::string& log_id)
    : version(version),
      timestamp(timestamp),
      tree_size(tree_size),
      signature(signature),
      log_id(log_id) {
  memcpy(this->sha256_root_hash, sha256_root_hash, kSthRootHashLength);
}

SignedTreeHead::SignedTreeHead(const SignedTreeHead& other) = default;

SignedTreeHead::~SignedTreeHead() = default;

void PrintTo(const SignedTreeHead& sth, std::ostream* os) {
  (*os) << "{\n"
        << "\t\"version\": " << sth.version << ",\n"
        << "\t\"timestamp\": " << sth.timestamp << ",\n"
        << "\t\"tree_size\": " << sth.tree_size << ",\n"
        << "\t\"sha256_root_hash\": \""
        << base::HexEncode(sth.sha256_root_hash, kSthRootHashLength)
        << "\",\n\t\"log_id\": \"" << base::HexEncode(sth.log_id) << "\"\n"
        << "}";
}

bool operator==(const SignedTreeHead& lhs, const SignedTreeHead& rhs) {
  return std::tie(lhs.version, lhs.timestamp, lhs.tree_size, lhs.log_id) ==
             std::tie(rhs.version, rhs.timestamp, rhs.tree_size, rhs.log_id) &&
         memcmp(lhs.sha256_root_hash, rhs.sha256_root_hash,
                kSthRootHashLength) == 0 &&
         lhs.signature.SignatureParametersMatch(
             rhs.signature.hash_algorithm, rhs.signature.signature_algorithm) &&
         lhs.signature.signature_data == rhs.signature.signature_data;
}

bool operator!=(const SignedTreeHead& lhs, const SignedTreeHead& rhs) {
  return !(lhs == rhs);
}

}  // namespace net::ct
