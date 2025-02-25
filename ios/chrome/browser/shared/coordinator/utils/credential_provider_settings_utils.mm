// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/utils/credential_provider_settings_utils.h"

#import <AuthenticationServices/AuthenticationServices.h>

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/public/provider/chrome/browser/password_auto_fill/password_auto_fill_api.h"

void OpenIOSCredentialProviderSettings() {
  if (!IOSPasskeysM2Enabled()) {
    ios::provider::PasswordsInOtherAppsOpensSettings();
    return;
  }

  // If available, use the API that allows to directly open the iOS credential
  // provider settings.
  if (@available(iOS 17.0, *)) {
    [ASSettingsHelper openCredentialProviderAppSettingsWithCompletionHandler:^(
                          NSError* error) {
      if (error) {
        ios::provider::PasswordsInOtherAppsOpensSettings();
      }
    }];
  } else {
    ios::provider::PasswordsInOtherAppsOpensSettings();
  }
}
