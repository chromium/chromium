/// Copyright 2025 The Chromium Authors
/// Use of this source code is governed by a BSD-style license that can be
/// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DATA_IMPORT_UI_DATA_IMPORT_CREDENTIAL_CONFLICT_RESOLUTION_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_DATA_IMPORT_UI_DATA_IMPORT_CREDENTIAL_CONFLICT_RESOLUTION_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"

@class PasskeyImportItem;
@class PasswordImportItem;
@protocol DataImportImportStageTransitionHandler;
@protocol DataImportCredentialConflictMutator;
@protocol DataImportCredentialConflictResolutionViewControllerDelegate;
@protocol ReauthenticationProtocol;

/// View controller listing credential conflicts introduced by data import and
/// allowing the user to resolve them.
@interface DataImportCredentialConflictResolutionViewController
    : ChromeTableViewController

/// Mutator object to handle conflict resolution decision.
@property(nonatomic, weak) id<DataImportCredentialConflictMutator> mutator;

/// Module for reauthentication used when the user wants to reveal a password.
@property(nonatomic, strong) id<ReauthenticationProtocol> reauthModule;

/// Handles dismissal of this view.
@property(nonatomic, weak)
    id<DataImportCredentialConflictResolutionViewControllerDelegate>
        delegate;

- (instancetype)initWithPasswordConflicts:
                    (NSArray<PasswordImportItem*>*)passwords
                         passkeyConflicts:(NSArray<PasskeyImportItem*>*)passkeys
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_DATA_IMPORT_UI_DATA_IMPORT_CREDENTIAL_CONFLICT_RESOLUTION_VIEW_CONTROLLER_H_
