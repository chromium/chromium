// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORD_FILTER_CHIPS_HEADER_ITEM_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORD_FILTER_CHIPS_HEADER_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_header_footer_item.h"

@class ChromeButton;
@class PasswordFilterChipsHeaderView;

// Credential categories used to filter the Password Manager list.
enum class PasswordCredentialFilter {
  kAll,
  kPasswords,
  kPasskeys,
};

// Delegate notified when the user taps a credential filter chip.
@protocol PasswordFilterChipsDelegate <NSObject>

// Called when the user selects `filter` in `headerView`.
- (void)passwordFilterChipsHeaderView:(PasswordFilterChipsHeaderView*)headerView
                      didSelectFilter:(PasswordCredentialFilter)filter;

@end

// Header item displaying horizontal credential filter chips.
@interface PasswordFilterChipsHeaderItem : TableViewHeaderFooterItem

// Currently selected credential filter. Defaults to `kAll`.
@property(nonatomic, assign) PasswordCredentialFilter selectedFilter;

// Whether the filter chip buttons are interactive. Defaults to `YES`.
@property(nonatomic, assign, getter=isEnabled) BOOL enabled;

@end

// Header view rendering the credential filter chip buttons.
@interface PasswordFilterChipsHeaderView : UITableViewHeaderFooterView

// Delegate notified when a filter chip is tapped.
@property(nonatomic, weak) id<PasswordFilterChipsDelegate> delegate;

// Filter chip buttons displayed in the header.
@property(nonatomic, copy, readonly) NSArray<ChromeButton*>* chipButtons;

// Updates button styles and interaction state for `selectedFilter` and
// `enabled`.
- (void)configureWithSelectedFilter:(PasswordCredentialFilter)selectedFilter
                            enabled:(BOOL)enabled;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORD_FILTER_CHIPS_HEADER_ITEM_H_
