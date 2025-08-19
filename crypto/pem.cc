// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/pem.h"

#include "base/containers/to_vector.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string_view_util.h"
#include "third_party/boringssl/src/pki/pem.h"

namespace crypto::pem {

std::vector<bssl::PEMToken> MessagesFromFile(
    const base::FilePath& path,
    std::initializer_list<const std::string_view> allowed_types) {
  std::optional<std::vector<uint8_t>> contents = base::ReadFileToBytes(path);
  if (!contents.has_value()) {
    return {};
  }

  base::span<const std::string_view> types(allowed_types);
  return bssl::PEMDecode(base::as_string_view(*contents),
                         bssl::Span<const std::string_view>(types));
}

std::optional<std::vector<uint8_t>> SingleMessageFromFile(
    const base::FilePath& path,
    std::string_view allowed_type) {
  std::optional<std::vector<uint8_t>> contents = base::ReadFileToBytes(path);
  if (!contents.has_value()) {
    return std::nullopt;
  }

  std::optional<std::string> body =
      bssl::PEMDecodeSingle(base::as_string_view(*contents), allowed_type);
  if (!body.has_value()) {
    return std::nullopt;
  }
  return base::ToVector(base::as_byte_span(*body));
}

}  // namespace crypto::pem
