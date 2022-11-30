// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_IN_OTHER_APPS_PASSWORDS_IN_OTHER_APPS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_IN_OTHER_APPS_PASSWORDS_IN_OTHER_APPS_MEDIATOR_H_

#import <Foundation/Foundation.h>
#import "ios/chrome/browser/ui/settings/password/passwords_in_other_apps/passwords_in_other_apps_consumer.h"
#import "ios/chrome/browser/ui/settings/password/passwords_in_other_apps/passwords_in_other_apps_view_controller_delegate.h"
#import "ios/chrome/browser/ui/settings/utils/password_auto_fill_status_observer.h"

// This mediator fetches and organises passwords autofill status for its
// consumer, as well as implementing its delegate methods to responding to its
// action items.
@interface PasswordsInOtherAppsMediator
    : NSObject <PasswordAutoFillStatusObserver,
                PasswordsInOtherAppsViewControllerDelegate>

// Consumer for the mediator that utilizes mediator properties and updates.
@property(nonatomic, weak) id<PasswordsInOtherAppsConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_IN_OTHER_APPS_PASSWORDS_IN_OTHER_APPS_MEDIATOR_H_
