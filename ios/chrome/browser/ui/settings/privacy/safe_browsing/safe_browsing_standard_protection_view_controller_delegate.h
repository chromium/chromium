// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_SAFE_BROWSING_SAFE_BROWSING_STANDARD_PROTECTION_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_SAFE_BROWSING_SAFE_BROWSING_STANDARD_PROTECTION_VIEW_CONTROLLER_DELEGATE_H_

@class TableViewCell;
@class ChromeTableViewStyler;

// Delegate for SafeBrowsingStandardProtectionViewController instance, to manage
// the model.
@protocol SafeBrowsingStandardProtectionViewControllerDelegate <NSObject>

// Sends switch toggle response to the model so that it can be updated.
- (void)toggleSwitchItem:(TableViewItem*)item withValue:(BOOL)value;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_SAFE_BROWSING_SAFE_BROWSING_STANDARD_PROTECTION_VIEW_CONTROLLER_DELEGATE_H_
