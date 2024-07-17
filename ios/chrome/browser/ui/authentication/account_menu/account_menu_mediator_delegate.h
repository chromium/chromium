// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_MENU_ACCOUNT_MENU_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_MENU_ACCOUNT_MENU_MEDIATOR_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/settings/google_services/sync_error_settings_command_handler.h"

@class AccountMenuMediator;
@protocol SystemIdentity;

@protocol AccountMenuMediatorDelegate <SyncErrorSettingsCommandHandler>

- (void)mediatorWantsToBeDismissed:(AccountMenuMediator*)mediator;

- (void)triggerSignoutWithTargetRect:(CGRect)targetRect
                          completion:(void (^)(BOOL success))completion;

- (void)triggerSigninWithSystemIdentity:(id<SystemIdentity>)identity
                             completion:
                                 (void (^)(id<SystemIdentity> systemIdentity))
                                     completion;

- (void)triggerAccountSwitchSnackbarWithIdentity:
    (id<SystemIdentity>)systemIdentity;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_MENU_ACCOUNT_MENU_MEDIATOR_DELEGATE_H_
