// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_IN_OTHER_APPS_PASSWORDS_IN_OTHER_APPS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_IN_OTHER_APPS_PASSWORDS_IN_OTHER_APPS_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/settings/password/passwords_in_other_apps/passwords_in_other_apps_consumer.h"

@protocol PasswordsInOtherAppsViewControllerDelegate;

// Protocol used to display Passwords In Other Apps promotional page.
@protocol PasswordsInOtherAppsPresenter

// Method invoked when the promotional page is dismissed by the user hitting
// "Back".
- (void)passwordsInOtherAppsViewControllerDidDismiss;

@end

// View controller that shows Passwords In Other Apps promotional page.
@interface PasswordsInOtherAppsViewController
    : UIViewController <PasswordsInOtherAppsConsumer>

// Object that manages showing and dismissal of the current view.
@property(nonatomic, weak) id<PasswordsInOtherAppsPresenter> presenter;

// The delegate to invoke when buttons are tapped.
@property(nonatomic, weak) id<PasswordsInOtherAppsViewControllerDelegate>
    delegate;

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNAme
                         bundle:(NSBundle*)nibBundle NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_IN_OTHER_APPS_PASSWORDS_IN_OTHER_APPS_VIEW_CONTROLLER_H_
