// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/webauthn/ios/fake_ios_passkey_client.h"

#include "base/functional/callback.h"

namespace webauthn {

FakeIOSPasskeyClient::FakeIOSPasskeyClient() {}

FakeIOSPasskeyClient::~FakeIOSPasskeyClient() = default;

void FakeIOSPasskeyClient::SetIOSPasskeyClientCommandsHandler(
    id<IOSPasskeyClientCommands> handler) {}

bool FakeIOSPasskeyClient::PerformUserVerification() {
  return false;
}

void FakeIOSPasskeyClient::FetchKeys(ReauthenticatePurpose purpose,
                                     KeysFetchedCallback callback) {
  static const size_t kKeyLength = 32u;
  fetch_keys_called_ = true;
  if (!callback.is_null()) {
    // Return a single 32 bytes key (zeroed out).
    std::move(callback).Run({std::vector<uint8_t>(kKeyLength, 0)}, nil);
  }
}

void FakeIOSPasskeyClient::ShowSuggestionBottomSheet(RequestInfo request_info) {
  show_suggestion_bottom_sheet_called_ = true;
}

void FakeIOSPasskeyClient::ShowCreationBottomSheet(RequestInfo request_info) {
  show_creation_bottom_sheet_called_ = true;
}

void FakeIOSPasskeyClient::ShowInterstitial(InterstitialCallback callback) {
  show_interstitial_called_ = true;
  std::move(callback).Run(interstitial_proceeds_);
}

void FakeIOSPasskeyClient::AllowPasskeyCreationInfobar(bool allowed) {}

bool FakeIOSPasskeyClient::DidShowSuggestionBottomSheet() const {
  return show_suggestion_bottom_sheet_called_;
}

bool FakeIOSPasskeyClient::DidShowCreationBottomSheet() const {
  return show_creation_bottom_sheet_called_;
}

bool FakeIOSPasskeyClient::DidFetchKeys() const {
  return fetch_keys_called_;
}

bool FakeIOSPasskeyClient::DidShowInterstitial() const {
  return show_interstitial_called_;
}

void FakeIOSPasskeyClient::SetInterstitialProceeds(bool proceeds) {
  interstitial_proceeds_ = proceeds;
}

void FakeIOSPasskeyClient::CancelPasskeyRequest(RequestInfo request_info) {}

bool FakeIOSPasskeyClient::IsGpmPasskeySavingEnabled() const {
  return gpm_passkey_saving_enabled_;
}

void FakeIOSPasskeyClient::SetGpmPasskeySavingEnabled(bool enabled) {
  gpm_passkey_saving_enabled_ = enabled;
}

}  // namespace webauthn
