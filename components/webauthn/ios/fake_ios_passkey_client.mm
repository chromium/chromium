// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/webauthn/ios/fake_ios_passkey_client.h"

namespace webauthn {

FakeIOSPasskeyClient::FakeIOSPasskeyClient(web::WebState* web_state)
    : delegate_(web_state) {}

FakeIOSPasskeyClient::~FakeIOSPasskeyClient() = default;

void FakeIOSPasskeyClient::SetIOSPasskeyClientCommandsHandler(
    id<IOSPasskeyClientCommands> handler) {}

bool FakeIOSPasskeyClient::PerformUserVerification() {
  return false;
}

void FakeIOSPasskeyClient::FetchKeys(ReauthenticatePurpose purpose,
                                     KeysFetchedCallback callback) {
  if (!callback.is_null()) {
    std::move(callback).Run({});
  }
}

void FakeIOSPasskeyClient::ShowSuggestionBottomSheet(RequestInfo request_info) {
  show_suggestion_bottom_sheet_called_ = true;
}

void FakeIOSPasskeyClient::ShowCreationBottomSheet(RequestInfo request_info) {
  show_creation_bottom_sheet_called_ = true;
}

void FakeIOSPasskeyClient::AllowPasskeyCreationInfobar(bool allowed) {}

password_manager::WebAuthnCredentialsDelegate*
FakeIOSPasskeyClient::GetWebAuthnCredentialsDelegateForDriver(
    IOSPasswordManagerDriver* driver) {
  return &delegate_;
}

bool FakeIOSPasskeyClient::DidShowSuggestionBottomSheet() const {
  return show_suggestion_bottom_sheet_called_;
}

bool FakeIOSPasskeyClient::DidShowCreationBottomSheet() const {
  return show_creation_bottom_sheet_called_;
}

IOSWebAuthnCredentialsDelegate* FakeIOSPasskeyClient::delegate() {
  return &delegate_;
}

}  // namespace webauthn
