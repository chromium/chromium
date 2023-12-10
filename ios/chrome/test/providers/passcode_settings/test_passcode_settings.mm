// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/passcode_settings/passcode_settings_api.h"

namespace ios::provider {

// Dummy implementations for passcode_settings_api.h.

BOOL SupportsPasscodeSettings() {
  return NO;
}

void OpenPasscodeSettings() {
  // No-op.
}

}  // namespace ios::provider
