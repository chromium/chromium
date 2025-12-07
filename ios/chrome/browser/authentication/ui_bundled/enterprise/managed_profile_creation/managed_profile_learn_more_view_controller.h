// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_ENTERPRISE_MANAGED_PROFILE_CREATION_MANAGED_PROFILE_LEARN_MORE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_ENTERPRISE_MANAGED_PROFILE_CREATION_MANAGED_PROFILE_LEARN_MORE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"

@class ManagedProfileLearnMoreViewController;
// Delegate for ManagedProfileLearnMoreViewController, to receive events when
// the view controller is dismissed.
@protocol ManagedProfileLearnMoreViewControllerPresentationDelegate <NSObject>
// Called when the view controller is dismissed, using the "Done" button.
- (void)dismissManagedProfileLearnMoreViewController:
    (ManagedProfileLearnMoreViewController*)viewController;

@end

// View controller for the LearnMore dialog.
@interface ManagedProfileLearnMoreViewController : UIViewController

@property(nonatomic, weak)
    id<ManagedProfileLearnMoreViewControllerPresentationDelegate>
        presentationDelegate;

- (instancetype)initWithUserEmail:(NSString*)userEmail
                     hostedDomain:(NSString*)hostedDomain
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_ENTERPRISE_MANAGED_PROFILE_CREATION_MANAGED_PROFILE_LEARN_MORE_VIEW_CONTROLLER_H_
