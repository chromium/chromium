// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_SIGNAL_API_UTILS_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_SIGNAL_API_UTILS_H_

#include <string_view>
#include <vector>

#include "base/containers/span.h"

namespace url {
class Origin;
}

namespace webauthn {
class PasskeyModel;

// LINT.IfChange(SignalUnknownCredentialResult)
enum class SignalUnknownCredentialResult {
  kPasskeyNotFound = 0,
  kPasskeyRemoved = 1,
  kPasskeyHidden = 2,
  kQuotaExceeded = 3,
  kPasskeyAlreadyHidden = 4,
  kMaxValue = kPasskeyAlreadyHidden,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/webauthn/enums.xml:SignalUnknownCredentialResultEnum)

// LINT.IfChange(SignalCurrentUserDetailsResult)
enum class SignalCurrentUserDetailsResult {
  kQuotaExceeded = 0,
  kPasskeyUpdated = 1,
  kPasskeyNotUpdated = 2,
  kMaxValue = kPasskeyNotUpdated,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/webauthn/enums.xml:SignalCurrentUserDetailsResultEnum)

// LINT.IfChange(SignalAllAcceptedCredentialsResult)
enum class SignalAllAcceptedCredentialsResult {
  kNoPasskeyChanged = 0,
  kPasskeyRemoved = 1,
  kPasskeyHidden = 2,
  kPasskeyRestored = 3,
  kQuotaExceeded = 4,
  kMaxValue = kQuotaExceeded,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/webauthn/enums.xml:SignalAllAcceptedCredentialsResultEnum)

// Updates the passkey model and quota tracker for a SignalUnknownCredential
// request. Returns true if the passkey was hidden, false otherwise.
bool UpdatePasskeyModelForSignalUnknownCredential(
    const url::Origin& origin,
    std::string_view rp_id,
    base::span<const uint8_t> credential_id,
    PasskeyModel& passkey_model);

// Updates the passkey model and quota tracker for a
// SignalAllAcceptedCredentials request. Returns the result of the operation.
SignalAllAcceptedCredentialsResult
UpdatePasskeyModelForSignalAllAcceptedCredentials(
    const url::Origin& origin,
    std::string_view rp_id,
    base::span<const uint8_t> user_id,
    const std::vector<std::vector<uint8_t>>& all_accepted_credentials_ids,
    PasskeyModel& passkey_model);

// Updates the passkey model and quota tracker for a SignalCurrentUserDetails
// request. Returns true if the passkey details were updated, false otherwise.
bool UpdatePasskeyModelForSignalCurrentUserDetails(
    const url::Origin& origin,
    std::string_view rp_id,
    base::span<const uint8_t> user_id,
    std::string_view name,
    std::string_view display_name,
    PasskeyModel& passkey_model);

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_SIGNAL_API_UTILS_H_
