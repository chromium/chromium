// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_IDENTITY_CHOOSER_IDENTITY_CHOOSER_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_IDENTITY_CHOOSER_IDENTITY_CHOOSER_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_

#import <UIKit/UIKit.h>

@class IdentityChooserViewController;

// Delegate protocol for presentation events of IdentityChooserViewController.
@protocol IdentityChooserViewControllerPresentationDelegate<NSObject>

// Called when IdentityChooserViewController disappear.
- (void)identityChooserViewControllerDidDisappear:
    (IdentityChooserViewController*)viewController;

// Called when the user taps on "Add Account…" cell.
- (void)identityChooserViewControllerDidTapOnAddAccount:
    (IdentityChooserViewController*)viewController;

// Called when the user taps on an identity.
- (void)identityChooserViewController:
            (IdentityChooserViewController*)viewController
          didSelectIdentityWithGaiaID:(NSString*)gaiaID;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_IDENTITY_CHOOSER_IDENTITY_CHOOSER_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_
