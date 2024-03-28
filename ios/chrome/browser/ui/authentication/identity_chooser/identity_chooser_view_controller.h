// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_IDENTITY_CHOOSER_IDENTITY_CHOOSER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_IDENTITY_CHOOSER_IDENTITY_CHOOSER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"
#import "ios/chrome/browser/ui/authentication/identity_chooser/identity_chooser_consumer.h"

@protocol IdentityChooserViewControllerPresentationDelegate;

// View controller to display the list of identities, to let the user choose an
// identity. IdentityChooserViewController also displays "Add Account…" cell
// at the end.
@interface IdentityChooserViewController
    : LegacyChromeTableViewController <IdentityChooserConsumer>

// Presentation delegate.
@property(nonatomic, weak) id<IdentityChooserViewControllerPresentationDelegate>
    presentationDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_IDENTITY_CHOOSER_IDENTITY_CHOOSER_VIEW_CONTROLLER_H_
