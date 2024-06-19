// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_CARD_UNMASK_AUTHENTICATION_SELECTION_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_CARD_UNMASK_AUTHENTICATION_SELECTION_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/card_unmask_authentication_selection_consumer.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/card_unmask_authentication_selection_mutator.h"

// A view controller showing a selection of challenge options for unmasking
// cards.
@interface CardUnmaskAuthenticationSelectionViewController
    : ChromeTableViewController <CardUnmaskAuthenticationSelectionConsumer,
                                 UITableViewDelegate>

// The delegate for user actions.
@property(nonatomic, weak) id<CardUnmaskAuthenticationSelectionMutator> mutator;

// Creates the card unmask authentication selection view controller.
- (instancetype)init;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_CARD_UNMASK_AUTHENTICATION_SELECTION_VIEW_CONTROLLER_H_
