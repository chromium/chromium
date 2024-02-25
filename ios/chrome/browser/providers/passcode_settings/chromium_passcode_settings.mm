// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/passcode_settings/passcode_settings_api.h"

#import <UIKit/UIKit.h>

namespace ios::provider {

BOOL SupportsPasscodeSettings() {
  return NO;
}

void OpenPasscodeSettings() {
  [[UIApplication sharedApplication]
                openURL:[NSURL URLWithString:UIApplicationOpenSettingsURLString]
                options:@{}
      completionHandler:nil];
}

}  // namespace ios::provider
