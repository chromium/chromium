// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_GOOGLE_SERVICES_BULK_UPLOAD_BULK_UPLOAD_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_GOOGLE_SERVICES_BULK_UPLOAD_BULK_UPLOAD_TABLE_VIEW_CONTROLLER_H_

#import <vector>

#import "ios/chrome/browser/settings/ui_bundled/google_services/bulk_upload/bulk_upload_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"

@protocol BulkUploadMutator;

@interface BulkUploadTableViewController : LegacyChromeTableViewController

@property(nonatomic, weak) id<BulkUploadMutator> mutator;

// Update the view items in the table view.
- (void)updateViewWithViewItems:(NSArray<BulkUploadViewItem*>*)viewItems;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_GOOGLE_SERVICES_BULK_UPLOAD_BULK_UPLOAD_TABLE_VIEW_CONTROLLER_H_
