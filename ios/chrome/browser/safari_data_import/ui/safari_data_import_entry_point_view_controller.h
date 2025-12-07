// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_UI_SAFARI_DATA_IMPORT_ENTRY_POINT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_UI_SAFARI_DATA_IMPORT_ENTRY_POINT_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/bottom_sheet/bottom_sheet_view_controller.h"

/// Screen presented for `SafariDataImportScreen::kEntryPoint`.
@interface SafariDataImportEntryPointViewController : BottomSheetViewController

/// Whether the "remind me later" button should be displayed. Must be set before
/// the view is loaded.
@property(nonatomic, assign) BOOL showReminderButton;

@end

#endif  // IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_UI_SAFARI_DATA_IMPORT_ENTRY_POINT_VIEW_CONTROLLER_H_
