// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utility functions for dealing with RFC 1421 PEM messages.

#ifndef CRYPTO_PEM_H_
#define CRYPTO_PEM_H_

#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "crypto/crypto_export.h"

namespace base {
class FilePath;
}

namespace bssl {
struct PEMToken;
}

namespace crypto::pem {

// These functions are wrappers for bssl::PEMDecode and bssl::PEMDecodeSingle
// that decode the contents of a named file. MessageFromFile() returns a vector
// of all the messages in the named file with one of the allowed types. If file
// IO fails for some reason, this function returns an empty vector, so that case
// is not distinguishable from an invalid PEM file, or a valid PEM file with no
// messages of the named types.
//
// SingleMessageFromFile() does similar, but returns the body of the single
// decoded message, if there is one. For any other case (multiple valid
// messages, an invalid message, an unreadable file, etc) it returns nullopt.
CRYPTO_EXPORT std::vector<bssl::PEMToken> MessagesFromFile(
    const base::FilePath& path,
    std::initializer_list<const std::string_view> allowed_types);
CRYPTO_EXPORT std::optional<std::vector<uint8_t>> SingleMessageFromFile(
    const base::FilePath& path,
    std::string_view allowed_type);

}  // namespace crypto::pem

#endif  // CRYPTO_PEM_H_
