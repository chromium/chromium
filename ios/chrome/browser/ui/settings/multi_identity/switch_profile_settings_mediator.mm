// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/multi_identity/switch_profile_settings_mediator.h"

#import <Foundation/Foundation.h>

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

@implementation SwitchProfileSettingsMediator

#pragma mark - SwitchProfileSettingsDelegate

- (void)openProfileInNewWindow:(NSString*)browserStateName {
  // Update the last used browserState so that the newly created scene is linked
  // to the selected browserState (and not to the old one).
  GetApplicationContext()->GetLocalState()->SetString(
      prefs::kBrowserStateLastUsed, base::SysNSStringToUTF8(browserStateName));

  // Open the selected profile in a new window.
  if (@available(iOS 17, *)) {
    UISceneSessionActivationRequest* activationRequest =
        [UISceneSessionActivationRequest request];
    [UIApplication.sharedApplication
        activateSceneSessionForRequest:activationRequest
                          errorHandler:nil];
  }
}

@end
