// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PARCEL_TRACKING_MAGIC_STACK_PARCEL_LIST_HALF_SHEET_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PARCEL_TRACKING_MAGIC_STACK_PARCEL_LIST_HALF_SHEET_TABLE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"

enum class ParcelType;
@class ParcelTrackingItem;

@protocol MagicStackParcelListHalfSheetTableViewControllerDelegate

// Indicates to the receiver to dismiss the parcel list half sheet.
- (void)dismissParcelListHalfSheet;

// Indicates to the receiver to stop tracking `parcelID`.
- (void)untrackParcel:(NSString*)parcelID carrier:(ParcelType)carrier;

@end

// Presents a list of tracked parcels.
@interface MagicStackParcelListHalfSheetTableViewController
    : LegacyChromeTableViewController

// Delegate for this ViewController.
@property(nonatomic, weak)
    id<MagicStackParcelListHalfSheetTableViewControllerDelegate>
        delegate;

// Initializes this class with the `parcels` to display.
- (instancetype)initWithParcels:(NSArray<ParcelTrackingItem*>*)parcels
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PARCEL_TRACKING_MAGIC_STACK_PARCEL_LIST_HALF_SHEET_TABLE_VIEW_CONTROLLER_H_
