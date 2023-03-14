// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_VERSION_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_VERSION_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_header_footer_item.h"

@class VersionFooter;

// Protocol notified when the footer is tapped.
@protocol VersionFooterDelegate

// Called when the version footer is tapped.
- (void)didTapVersionFooter:(VersionFooter*)footer;

@end

// Item to display the version of the current build.
@interface VersionItem : TableViewHeaderFooterItem

// The display string representing the version.
@property(nonatomic, copy) NSString* text;

@end

// Footer view class associated to VersionItem.
@interface VersionFooter : UITableViewHeaderFooterView

// Label for the current build version.
@property(nonatomic, readonly, strong) UILabel* textLabel;

// Delegate.
@property(nonatomic, weak) id<VersionFooterDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_VERSION_ITEM_H_
