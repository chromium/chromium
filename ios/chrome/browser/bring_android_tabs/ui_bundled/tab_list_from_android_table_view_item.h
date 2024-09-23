// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BRING_ANDROID_TABS_UI_BUNDLED_TAB_LIST_FROM_ANDROID_TABLE_VIEW_ITEM_H_
#define IOS_CHROME_BROWSER_BRING_ANDROID_TABS_UI_BUNDLED_TAB_LIST_FROM_ANDROID_TABLE_VIEW_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

@class CrURL;
@class FaviconView;

// TabListFromAndroidTableViewItem contains the model data for a
// TabListFromAndroidTableViewCell.
@interface TabListFromAndroidTableViewItem : TableViewItem

// The title of the page at `URL`.
@property(nonatomic, readwrite, copy) NSString* title;

// CrURL from which the cell will retrieve a favicon and display the host name.
@property(nonatomic, readwrite, strong) CrURL* URL;

// Identifier to match a TabListFromAndroidTableViewItem with its
// TabListFromAndroidTableViewCell.
@property(nonatomic, readonly) NSString* uniqueIdentifier;

@end

// TabListFromAndroidTableViewCell is used in the Bring Android Tabs prompt.
// Contains a favicon, title, and URL.
@interface TabListFromAndroidTableViewCell : TableViewCell

// The imageview that is displayed on the leading edge of the cell.  This
// contains a favicon composited on top of an off-white background.
@property(nonatomic, readwrite, strong) FaviconView* faviconView;

// The cell title.
@property(nonatomic, readonly, strong) UILabel* titleLabel;

// The host URL associated with this cell.
@property(nonatomic, readonly, strong) UILabel* URLLabel;

// Unique identifier that matches with one TabListFromAndroidTableViewItem.
@property(nonatomic, strong) NSString* cellUniqueIdentifier;

@end

#endif  // IOS_CHROME_BROWSER_BRING_ANDROID_TABS_UI_BUNDLED_TAB_LIST_FROM_ANDROID_TABLE_VIEW_ITEM_H_
