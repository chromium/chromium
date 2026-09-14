// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/model/ios_device_authorization_util.h"

#import <algorithm>
#import <cstddef>
#import <string>
#import <string_view>
#import <vector>

#import "base/base64.h"
#import "base/containers/flat_map.h"
#import "base/strings/strcat.h"
#import "base/strings/stringprintf.h"
#import "components/webauthn/core/browser/device_authorization/device_authorization_types.h"
#import "components/webauthn/core/browser/device_authorization/proto/device_authorization_key.pb.h"
#import "crypto/hash.h"

namespace {

constexpr char kRaptTokenFieldName[] = "rapt_token";
constexpr char kSaltFieldName[] = "salt";
constexpr char kTrustedVaultKeyAvailabilityFormat[] =
    "trusted_vault_key_availability_%zu";

}  // namespace

base::flat_map<std::string, std::string> BuildDeviceIntegrityContentBindings(
    const sync_pb::GetDeviceAuthorizationKeyRequest& request,
    std::string_view salt) {
  base::flat_map<std::string, std::string> bindings;

  if (request.has_reauth_proof_token()) {
    bindings.emplace(kRaptTokenFieldName,
                     base::Base64Encode(crypto::hash::Sha256(
                         base::StrCat({request.reauth_proof_token(), salt}))));
  }

  std::vector<webauthn::TrustedVaultKeyAvailability> availabilities(
      request.trusted_vault_key_availability().begin(),
      request.trusted_vault_key_availability().end());
  std::sort(availabilities.begin(), availabilities.end(),
            [](const webauthn::TrustedVaultKeyAvailability& a,
               const webauthn::TrustedVaultKeyAvailability& b) {
              return a.security_domain() < b.security_domain();
            });

  for (size_t i = 0; i < availabilities.size(); ++i) {
    const webauthn::TrustedVaultKeyAvailability& entry = availabilities[i];
    const char available_byte = entry.key_available() ? 1 : 0;
    bindings.emplace(base::StringPrintf(kTrustedVaultKeyAvailabilityFormat, i),
                     base::Base64Encode(crypto::hash::Sha256(base::StrCat({
                         entry.security_domain(),
                         std::string_view(&available_byte, 1),
                         salt,
                     }))));
  }

  bindings.emplace(kSaltFieldName,
                   base::Base64Encode(crypto::hash::Sha256(salt)));
  return bindings;
}
