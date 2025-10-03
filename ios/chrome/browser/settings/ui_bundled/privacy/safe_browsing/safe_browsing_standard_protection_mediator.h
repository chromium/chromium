// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PRIVACY_SAFE_BROWSING_SAFE_BROWSING_STANDARD_PROTECTION_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PRIVACY_SAFE_BROWSING_SAFE_BROWSING_STANDARD_PROTECTION_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/settings/ui_bundled/privacy/safe_browsing/safe_browsing_standard_protection_consumer.h"

class AuthenticationService;
class PrefService;

namespace signin {
class IdentityManager;
}

// Mediator for the Google services settings.
@interface SafeBrowsingStandardProtectionMediator : NSObject

// View controller.
@property(nonatomic, weak) id<SafeBrowsingStandardProtectionConsumer> consumer;

// Designated initializer. All the parameters should not be null.
// `userPrefService`: preference service from the profile.
// `authService`: authentication service from profile.
// `identityManager`: identity manager from profile.
- (instancetype)initWithUserPrefService:(PrefService*)userPrefService
                            authService:(AuthenticationService*)authService
                        identityManager:
                            (signin::IdentityManager*)identityManager
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects observers.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PRIVACY_SAFE_BROWSING_SAFE_BROWSING_STANDARD_PROTECTION_MEDIATOR_H_
