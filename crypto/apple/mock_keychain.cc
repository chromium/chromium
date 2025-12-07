// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/apple/mock_keychain.h"

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"

namespace {

constexpr char kPassword[] = "mock_password";

// Adds an entry to a local histogram to indicate that the Keychain would have
// been accessed, if this class were not a mock of the Keychain.
void IncrementKeychainAccessHistogram() {
  // This local histogram is accessed by Telemetry to track the number of times
  // the keychain is accessed, since keychain access is known to be synchronous
  // and slow.
  LOCAL_HISTOGRAM_BOOLEAN("OSX.Keychain.Access", true);
}

}  // namespace

namespace crypto::apple {

MockKeychain::MockKeychain() = default;
MockKeychain::~MockKeychain() = default;

base::expected<std::vector<uint8_t>, OSStatus>
MockKeychain::FindGenericPassword(std::string_view service_name,
                                  std::string_view account_name) const {
  IncrementKeychainAccessHistogram();

  // When simulating |noErr|, return canned |passwordData| and
  // |passwordLength|.  Otherwise, just return given code.
  if (find_generic_result_ == noErr) {
    return base::ToVector(base::byte_span_from_cstring(kPassword));
  }

  return base::unexpected(find_generic_result_);
}

OSStatus MockKeychain::AddGenericPassword(
    std::string_view service_name,
    std::string_view account_name,
    base::span<const uint8_t> password) const {
  IncrementKeychainAccessHistogram();

  called_add_generic_ = true;

  DCHECK(!password.empty());
  return noErr;
}

std::string MockKeychain::GetEncryptionPassword() const {
  IncrementKeychainAccessHistogram();
  return kPassword;
}

}  // namespace crypto::apple
