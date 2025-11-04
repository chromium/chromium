// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_IOS_IOS_PASSKEY_CLIENT_H_
#define COMPONENTS_WEBAUTHN_IOS_IOS_PASSKEY_CLIENT_H_

#include <cstdint>

#import "components/webauthn/ios/passkey_types.h"

// Virtual class which exposes the API required by the PasskeyTabHelper to
// perform passkey related tasks.
class IOSPasskeyClient {
 public:
  virtual ~IOSPasskeyClient() = default;

  // Returns whether modal passkey login is allowed with the shim.
  virtual bool IsModalLoginWithShimAllowed() const = 0;

  // Returns whether conditional passkey login is allowed with the shim.
  // Ignored if IsModalLoginWithShimAllowed() returns false.
  virtual bool IsConditionalLoginWithShimAllowed() const = 0;

  // Performs user verification and returns whether it was successful.
  virtual bool PerformUserVerification() = 0;

  // Fetches the keys for the provided purpose and calls the callback with the
  // fetched keys as input.
  virtual void FetchKeys(webauthn::ReauthenticatePurpose purpose,
                         webauthn::KeysFetchedCallback callback) = 0;

  // Shows the bottom sheet with passkey suggestions.
  virtual void ShowSuggestionBottomSheet() = 0;

  // Sets whether showing the passkey creation infobar is allowed. Should be
  // enabled before passkey creation happens within the passkey model and
  // disabled after passkey creation is completed.
  virtual void AllowPasskeyCreationInfobar(bool allowed) = 0;

 protected:
  IOSPasskeyClient() = default;
};

#endif  // COMPONENTS_WEBAUTHN_IOS_IOS_PASSKEY_CLIENT_H_
