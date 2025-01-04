// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_ENTERPRISE_MANAGED_PROFILE_CREATION_LEARN_MORE_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_ENTERPRISE_MANAGED_PROFILE_CREATION_LEARN_MORE_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"

@class LearnMoreTableViewController;
// Delegate for LearnMoreTableViewController, to receive events when the view
// controller is dismissed.
@protocol LearnMoreTableViewControllerPresentationDelegate <NSObject>
// Called when the view controller is dismissed, using the "Done" button.
- (void)dismissLearnMoreTableViewController:
    (LearnMoreTableViewController*)viewController;

@end

// View controller for the LearnMore dialog.
@interface LearnMoreTableViewController : ChromeTableViewController

@property(nonatomic, weak) id<LearnMoreTableViewControllerPresentationDelegate>
    presentationDelegate;

- (instancetype)initWithUserEmail:(NSString*)userEmail
                     hostedDomain:(NSString*)hostedDomain
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_ENTERPRISE_MANAGED_PROFILE_CREATION_LEARN_MORE_TABLE_VIEW_CONTROLLER_H_
