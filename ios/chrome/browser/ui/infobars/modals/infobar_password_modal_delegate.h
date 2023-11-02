// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_PASSWORD_MODAL_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_PASSWORD_MODAL_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_delegate.h"

// Delegate to handle Password Infobar Modal actions.
@protocol InfobarPasswordModalDelegate <InfobarModalDelegate>

// Updates (or saves in case they haven't been previously saved) the `username`
// and `password` of the PasswordManagerInfobarDelegate.
- (void)updateCredentialsWithUsername:(NSString*)username
                             password:(NSString*)password;

// Blocks the current site to never prompt the user to save its credentials
// again.
- (void)neverSaveCredentialsForCurrentSite;

// Dismisses the InfobarModal with no animation, then presents the Password
// Settings screen modally.
- (void)presentPasswordSettings;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_PASSWORD_MODAL_DELEGATE_H_
