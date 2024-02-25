// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_BULK_UPLOAD_BULK_UPLOAD_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_BULK_UPLOAD_BULK_UPLOAD_CONSTANTS_H_

#import <UIKit/UIKit.h>

// Type enum to share between the mediator and the view controller.
enum class BulkUploadType : NSInteger {
  kBookmark = 0,
  kPassword = 1,
  kReadinglist = 2,
};

// Accessibility identifier for the bulk upload table view.
extern NSString* const kBulkUploadTableViewAccessibilityIdentifier;

// Accessibility identifiers for the per-datatype row items in the bulk upload
// table view.
extern NSString* const kBulkUploadTableViewPasswordsItemAccessibilityIdentifer;
extern NSString* const kBulkUploadTableViewBookmarksItemAccessibilityIdentifer;
extern NSString* const
    kBulkUploadTableViewReadingListItemAccessibilityIdentifer;

// Accessibility identifiers for the save button in the bulk upload page.
extern NSString* const kBulkUploadSaveButtonAccessibilityIdentifer;

// Item to exchange between the mediator and the view controller.
@interface BulkUploadViewItem : NSObject

// Item's data type.
@property(nonatomic, assign) BulkUploadType type;
// Title for the cell.
@property(nonatomic, copy) NSString* title;
// Sutitle for the cell.
@property(nonatomic, copy) NSString* subtitle;
// YES if the cell switch is selected.
@property(nonatomic, assign) BOOL selected;
// Accessibility identifier for the cell.
@property(nonatomic, copy) NSString* accessibilityIdentifier;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_BULK_UPLOAD_BULK_UPLOAD_CONSTANTS_H_
