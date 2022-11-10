// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_PASSWORD_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_PASSWORD_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/autofill/manual_fill/fallback_view_controller.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_consumer.h"

namespace manual_fill {

extern NSString* const kPasswordDoneButtonAccessibilityIdentifier;
extern NSString* const kPasswordSearchBarAccessibilityIdentifier;
extern NSString* const kPasswordTableViewAccessibilityIdentifier;

}  // namespace manual_fill

@class PasswordViewController;
@class CrURL;

// Delegate of the PasswordViewController.
@protocol PasswordViewControllerDelegate <NSObject>

// User Tapped "Done" button.
- (void)passwordViewControllerDidTapDoneButton:
    (PasswordViewController*)passwordViewController;

// Called when the user taps the link in the header.
- (void)didTapLinkURL:(CrURL*)URL;

@end

// This class presents a list of usernames and passwords in a table view.
@interface PasswordViewController
    : FallbackViewController<ManualFillPasswordConsumer>

// Delegate for the view controller.
@property(nonatomic, weak) id<PasswordViewControllerDelegate> delegate;

- (instancetype)initWithSearchController:(UISearchController*)searchController
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_PASSWORD_VIEW_CONTROLLER_H_
