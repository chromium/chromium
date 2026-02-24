// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CLEAR_BROWSING_DATA_QUICK_DELETE_OTHER_DATA_COORDINATOR_QUICK_DELETE_OTHER_DATA_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CLEAR_BROWSING_DATA_QUICK_DELETE_OTHER_DATA_COORDINATOR_QUICK_DELETE_OTHER_DATA_MEDIATOR_H_

#import <Foundation/Foundation.h>

class AuthenticationService;
class TemplateURLService;

namespace signin {
class IdentityManager;
}

@protocol QuickDeleteOtherDataConsumer;

// Mediator for the "Quick Delete Other Data" page.
@interface QuickDeleteOtherDataMediator : NSObject

// Consumer for the "Quick Delete Other Data" page.
@property(nonatomic, weak) id<QuickDeleteOtherDataConsumer> consumer;

// The designated initializer. `authenticationService` is used to verify the
// user's sign-in status while the `identityManager` is used to create an
// observer that tracks if the sign-in status has changed. The
// `templateURLService` is used to verify if the user's default search engine is
// Google.
- (instancetype)
    initWithAuthenticationService:(AuthenticationService*)authenticationService
                  identityManager:(signin::IdentityManager*)identityManager
               templateURLService:(TemplateURLService*)templateURLService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CLEAR_BROWSING_DATA_QUICK_DELETE_OTHER_DATA_COORDINATOR_QUICK_DELETE_OTHER_DATA_MEDIATOR_H_
