// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_WELCOME_WELCOME_SCREEN_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_WELCOME_WELCOME_SCREEN_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/first_run/welcome/welcome_screen_consumer.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

@protocol TOSCommands;

// Extends the base delegate protocol to handle taps on the "Accept and
// continue" button.
@protocol WelcomeScreenViewControllerDelegate <PromoStyleViewControllerDelegate>

// Called when the user taps on "Manage" related to metric reporting.
- (void)showUMADialog;

// Logs scrollability metric on view appears.
- (void)logScrollButtonVisible:(BOOL)scrollButtonVisible
        withUMACheckboxVisible:(BOOL)umaCheckboxVisible;

@end

// View controller of welcome screen.
@interface WelcomeScreenViewController
    : PromoStyleViewController <WelcomeScreenConsumer>

@property(nonatomic, weak) id<WelcomeScreenViewControllerDelegate> delegate;

// Init with the handler used to manage the display of TOS.
- (instancetype)initWithTOSHandler:(id<TOSCommands>)TOSHandler
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_WELCOME_WELCOME_SCREEN_VIEW_CONTROLLER_H_
