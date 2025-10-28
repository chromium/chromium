// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CLEAR_BROWSING_DATA_QUICK_DELETE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CLEAR_BROWSING_DATA_QUICK_DELETE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/keyboard/ui_bundled/key_command_actions.h"
#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/quick_delete_consumer.h"
#import "ios/chrome/browser/shared/ui/bottom_sheet/table_view_bottom_sheet_view_controller.h"

@protocol QuickDeleteMutator;
@protocol QuickDeletePresentationCommands;

// View controller for Quick Delete, the new vesion of Clear/Delete Browsing
// Data.
@interface QuickDeleteViewController
    : BottomSheetViewController <QuickDeleteConsumer, KeyCommandActions>

// Local dispatcher for this `QuickDeleteViewController`.
@property(nonatomic, weak) id<QuickDeletePresentationCommands>
    presentationHandler;

@property(nonatomic, weak) id<QuickDeleteMutator> mutator;

// Moves Voiceover focus to the browsing data row.
- (void)focusOnBrowsingDataRow;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CLEAR_BROWSING_DATA_QUICK_DELETE_VIEW_CONTROLLER_H_
