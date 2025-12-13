// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/signed_tree_head.h"

#include <algorithm>
#include <array>
#include <ostream>
#include <string>
#include <tuple>

#include "base/containers/span.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"

namespace net::ct {

SignedTreeHead::SignedTreeHead() = default;

SignedTreeHead::SignedTreeHead(
    Version version,
    base::Time timestamp,
    uint64_t tree_size,
    base::span<const uint8_t, kSthRootHashLength> sha256_root_hash,
    const DigitallySigned& signature,
    const std::string& log_id)
    : version(version),
      timestamp(timestamp),
      tree_size(tree_size),
      signature(signature),
      log_id(log_id) {
  base::as_writable_byte_span(this->sha256_root_hash)
      .copy_from(sha256_root_hash);
}

SignedTreeHead::SignedTreeHead(const SignedTreeHead& other) = default;

SignedTreeHead::~SignedTreeHead() = default;

void PrintTo(const SignedTreeHead& sth, std::ostream* os) {
  (*os) << "{\n"
        << "\t\"version\": " << sth.version << ",\n"
        << "\t\"timestamp\": " << sth.timestamp << ",\n"
        << "\t\"tree_size\": " << sth.tree_size << ",\n"
        << "\t\"sha256_root_hash\": \"" << base::HexEncode(sth.sha256_root_hash)
        << "\",\n\t\"log_id\": \"" << base::HexEncode(sth.log_id) << "\"\n"
        << "}";
}

bool operator==(const SignedTreeHead& lhs, const SignedTreeHead& rhs) {
  return std::tie(lhs.version, lhs.timestamp, lhs.tree_size, lhs.log_id) ==
             std::tie(rhs.version, rhs.timestamp, rhs.tree_size, rhs.log_id) &&
         std::ranges::equal(base::as_byte_span(lhs.sha256_root_hash),
                            base::as_byte_span(rhs.sha256_root_hash)) &&
         lhs.signature.SignatureParametersMatch(
             rhs.signature.hash_algorithm, rhs.signature.signature_algorithm) &&
         lhs.signature.signature_data == rhs.signature.signature_data;
}

}  // namespace net::ct
