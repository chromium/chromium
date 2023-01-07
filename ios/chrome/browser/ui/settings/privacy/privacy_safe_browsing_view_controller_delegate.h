// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_SAFE_BROWSING_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_SAFE_BROWSING_VIEW_CONTROLLER_DELEGATE_H_

@class PrivacySafeBrowsingViewController;
@class TableViewItem;

// Delegate for PrivacySafeBrowsingViewController instance, to manage the
// model.
@protocol PrivacySafeBrowsingViewControllerDelegate <NSObject>

// Sends `item` to the model to handle logic and navigation.
- (void)didSelectItem:(TableViewItem*)item;

// Handles navigation related to an accessory view being clicked.
- (void)didTapInfoButton:(UIButton*)button onItem:(TableViewItem*)item;

// Selects the item based on the most recent preference values
// changes.
- (void)selectItem;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_SAFE_BROWSING_VIEW_CONTROLLER_DELEGATE_H_
