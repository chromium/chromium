// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_CHROME_ROOT_STORE_TEST_UTIL_H_
#define NET_TEST_CHROME_ROOT_STORE_TEST_UTIL_H_

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "base/containers/span.h"

namespace chrome_root_store {
class Signer;
class SignerSet;
}  // namespace chrome_root_store

namespace net::test {

// Adds a mirror signer to the `signer_set` proto and sets the specified
// fields. Other fields will be initialized to defaults that are sufficient for
// the signer to be considered usable. Tests can further modify the returned
// object if needed.
chrome_root_store::Signer* AddSignerSetMirror(
    chrome_root_store::SignerSet& signer_set,
    base::span<const uint8_t> log_id,
    std::string_view operator_name);

// Adds an issuer  signer to the `signer_set` proto and sets the specified
// fields. Other fields will be initialized to defaults that are sufficient for
// the signer to be considered usable. Tests can further modify the returned
// object if needed.
chrome_root_store::Signer* AddSignerSetIssuer(
    chrome_root_store::SignerSet& signer_set,
    base::span<const uint8_t> log_id,
    std::string_view operator_name,
    std::optional<int32_t> crs_root_id);

// Returns the DER-encoded SubjectPublicKeyInfo of a newly generated ML-DSA-44
// key, for tests that need a well-formed MTC CA key but do not verify
// signatures with it.
std::vector<uint8_t> MakeTestMtcCaKey();

}  // namespace net::test

#endif  // NET_TEST_CHROME_ROOT_STORE_TEST_UTIL_H_
