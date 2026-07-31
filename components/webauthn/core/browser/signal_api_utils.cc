// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/signal_api_utils.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/device_event_log/device_event_log.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/webauthn/core/browser/passkey_change_quota_tracker.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "url/origin.h"

namespace webauthn {

namespace {

void LogSignalUnknownCredential(SignalUnknownCredentialResult result) {
  base::UmaHistogramEnumeration(
      "WebAuthentication.SignalUnknownCredentialRemovedGPMPasskey", result);
}

void LogSignalAllAcceptedCredentials(
    SignalAllAcceptedCredentialsResult result) {
  base::UmaHistogramEnumeration(
      "WebAuthentication.SignalAllAcceptedCredentialsRemovedGPMPasskey",
      result);
}

void LogSignalCurrentUserDetails(SignalCurrentUserDetailsResult result) {
  base::UmaHistogramEnumeration(
      "WebAuthentication.SignalCurrentUserDetailsUpdatedGPMPasskey", result);
}

SignalUnknownCredentialResult UpdatePasskeyModelForSignalUnknownCredentialImpl(
    const url::Origin& origin,
    std::string_view rp_id,
    base::span<const uint8_t> credential_id,
    PasskeyModel& passkey_model) {
  PasskeyChangeQuotaTracker* quota_tracker =
      PasskeyChangeQuotaTracker::GetInstance();
  if (!quota_tracker->CanMakeChange(origin)) {
    FIDO_LOG(ERROR) << "Dropping removal request from " << origin
                    << ": quota exceeded.";
    return SignalUnknownCredentialResult::kQuotaExceeded;
  }

  std::string credential_id_str(credential_id.begin(), credential_id.end());
  std::optional<sync_pb::WebauthnCredentialSpecifics> credential_specifics =
      passkey_model.GetPasskey(
          rp_id, credential_id_str,
          webauthn::PasskeyModel::ShadowedCredentials::kExclude);
  if (!credential_specifics) {
    return SignalUnknownCredentialResult::kPasskeyNotFound;
  }

  if (credential_specifics->hidden()) {
    return SignalUnknownCredentialResult::kPasskeyAlreadyHidden;
  }

  quota_tracker->TrackChange(origin);
  passkey_model.HidePasskey(std::move(credential_id_str), base::Time::Now());
  return SignalUnknownCredentialResult::kPasskeyHidden;
}

SignalAllAcceptedCredentialsResult
UpdatePasskeyModelForSignalAllAcceptedCredentialsImpl(
    const url::Origin& origin,
    std::string_view rp_id,
    base::span<const uint8_t> user_id,
    const std::vector<std::vector<uint8_t>>& all_accepted_credentials_ids,
    PasskeyModel& passkey_model) {
  PasskeyChangeQuotaTracker* quota_tracker =
      PasskeyChangeQuotaTracker::GetInstance();
  if (!quota_tracker->CanMakeChange(origin)) {
    FIDO_LOG(ERROR) << "Dropping all accepted credentials request from "
                    << origin << ": quota exceeded.";
    return SignalAllAcceptedCredentialsResult::kQuotaExceeded;
  }

  std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys =
      passkey_model.GetPasskeys(
          rp_id, webauthn::PasskeyModel::ShadowedCredentials::kExclude);
  const auto passkey_it =
      std::ranges::find_if(passkeys, [&user_id](const auto& passkey) {
        return base::as_byte_span(passkey.user_id()) == user_id;
      });
  if (passkey_it == passkeys.end()) {
    return SignalAllAcceptedCredentialsResult::kNoPasskeyChanged;
  }

  bool passkey_in_list =
      std::ranges::contains(all_accepted_credentials_ids,
                            base::as_byte_span(passkey_it->credential_id()));
  if ((passkey_in_list && !passkey_it->hidden()) ||
      (!passkey_in_list && passkey_it->hidden())) {
    return SignalAllAcceptedCredentialsResult::kNoPasskeyChanged;
  }

  quota_tracker->TrackChange(origin);

  if (passkey_in_list) {
    passkey_model.UnhidePasskey(passkey_it->credential_id());
    return SignalAllAcceptedCredentialsResult::kPasskeyRestored;
  }

  passkey_model.HidePasskey(passkey_it->credential_id(), base::Time::Now());
  return SignalAllAcceptedCredentialsResult::kPasskeyHidden;
}

SignalCurrentUserDetailsResult
UpdatePasskeyModelForSignalCurrentUserDetailsImpl(
    const url::Origin& origin,
    std::string_view rp_id,
    base::span<const uint8_t> user_id,
    std::string_view name,
    std::string_view display_name,
    PasskeyModel& passkey_model) {
  PasskeyChangeQuotaTracker* quota_tracker =
      PasskeyChangeQuotaTracker::GetInstance();
  if (!quota_tracker->CanMakeChange(origin)) {
    FIDO_LOG(ERROR) << "Dropping update request from " << origin
                    << ": quota exceeded.";
    return SignalCurrentUserDetailsResult::kQuotaExceeded;
  }

  bool is_passkey_updated = false;
  for (const auto& passkey : passkey_model.GetPasskeys(
           rp_id, webauthn::PasskeyModel::ShadowedCredentials::kExclude)) {
    if (base::as_byte_span(passkey.user_id()) == user_id &&
        (passkey.user_name() != name ||
         passkey.user_display_name() != display_name)) {
      if (passkey_model.UpdatePasskey(
              passkey.credential_id(),
              {.user_name = std::string(name),
               .user_display_name = std::string(display_name)},
              /*updated_by_user=*/false)) {
        is_passkey_updated = true;
      }
    }
  }

  if (is_passkey_updated) {
    quota_tracker->TrackChange(origin);
    FIDO_LOG(EVENT) << "Updating passkey user details for " << origin;
    return SignalCurrentUserDetailsResult::kPasskeyUpdated;
  }

  return SignalCurrentUserDetailsResult::kPasskeyNotUpdated;
}

}  // namespace

bool UpdatePasskeyModelForSignalUnknownCredential(
    const url::Origin& origin,
    std::string_view rp_id,
    base::span<const uint8_t> credential_id,
    PasskeyModel& passkey_model) {
  SignalUnknownCredentialResult result =
      UpdatePasskeyModelForSignalUnknownCredentialImpl(
          origin, rp_id, credential_id, passkey_model);
  LogSignalUnknownCredential(result);
  return result == SignalUnknownCredentialResult::kPasskeyHidden;
}

SignalAllAcceptedCredentialsResult
UpdatePasskeyModelForSignalAllAcceptedCredentials(
    const url::Origin& origin,
    std::string_view rp_id,
    base::span<const uint8_t> user_id,
    const std::vector<std::vector<uint8_t>>& all_accepted_credentials_ids,
    PasskeyModel& passkey_model) {
  SignalAllAcceptedCredentialsResult result =
      UpdatePasskeyModelForSignalAllAcceptedCredentialsImpl(
          origin, rp_id, user_id, all_accepted_credentials_ids, passkey_model);
  LogSignalAllAcceptedCredentials(result);
  return result;
}

bool UpdatePasskeyModelForSignalCurrentUserDetails(
    const url::Origin& origin,
    std::string_view rp_id,
    base::span<const uint8_t> user_id,
    std::string_view name,
    std::string_view display_name,
    PasskeyModel& passkey_model) {
  SignalCurrentUserDetailsResult result =
      UpdatePasskeyModelForSignalCurrentUserDetailsImpl(
          origin, rp_id, user_id, name, display_name, passkey_model);
  LogSignalCurrentUserDetails(result);
  return result == SignalCurrentUserDetailsResult::kPasskeyUpdated;
}

}  // namespace webauthn
