// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_QUICK_DELETE_BROWSING_DATA_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_QUICK_DELETE_BROWSING_DATA_VIEW_CONTROLLER_DELEGATE_H_

// Delegate protocol for actions performed within the Quick Delete browsing data
// page.
@protocol QuickDeleteBrowsingDataViewControllerDelegate

// Method invoked when the user wants to dismiss browsing data page either via
// the 'Confirm', 'Cancel' buttons or by dragging the view down.
- (void)dismissBrowsingDataPage;

// Method invoked when the user taps the footer 'sign out of Chrome' link.
- (void)signOutAndShowActionSheet;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_QUICK_DELETE_BROWSING_DATA_VIEW_CONTROLLER_DELEGATE_H_
