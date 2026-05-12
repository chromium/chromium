// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_IDENTITY_DOCS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_IDENTITY_DOCS_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/identity_docs_consumer.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_controller_protocol.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_view_controller.h"

@protocol AutofillAIBaseMutator;
@class IdentityDocsTableViewController;

// Delegate for presentation events related to IdentityDocsTableViewController.
@protocol IdentityDocsTableViewControllerDelegate <NSObject>

// Called when the view controller is removed from its parent.
- (void)identityDocsTableViewControllerDidRemove:
    (IdentityDocsTableViewController*)controller;

@end

// The TableView for Identity Docs settings page.
@interface IdentityDocsTableViewController
    : SettingsRootTableViewController <IdentityDocsConsumer,
                                       SettingsControllerProtocol>

// Presentation delegate.
@property(nonatomic, weak) id<IdentityDocsTableViewControllerDelegate> delegate;

// Mutator for actions in the view.
@property(nonatomic, weak) id<AutofillAIBaseMutator> mutator;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_IDENTITY_DOCS_TABLE_VIEW_CONTROLLER_H_
