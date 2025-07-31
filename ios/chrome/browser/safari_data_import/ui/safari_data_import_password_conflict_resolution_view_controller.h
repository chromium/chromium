/// Copyright 2025 The Chromium Authors
/// Use of this source code is governed by a BSD-style license that can be
/// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_UI_SAFARI_DATA_IMPORT_PASSWORD_CONFLICT_RESOLUTION_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_UI_SAFARI_DATA_IMPORT_PASSWORD_CONFLICT_RESOLUTION_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"

@class PasswordImportItem;
@protocol SafariDataImportImportStageTransitionHandler;
@protocol SafariDataImportPasswordConflictMutator;

/// View controller listing password conflicts introduced by Safari data import
/// and allowing the user to resolve them.
@interface SafariDataImportPasswordConflictResolutionViewController
    : ChromeTableViewController

/// Mutator object to handle conflict resolution decision.
@property(nonatomic, weak) id<SafariDataImportPasswordConflictMutator> mutator;

- (instancetype)initWithPasswordConflicts:
    (NSArray<PasswordImportItem*>*)passwords NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_UI_SAFARI_DATA_IMPORT_PASSWORD_CONFLICT_RESOLUTION_VIEW_CONTROLLER_H_
