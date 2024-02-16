// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_VIEW_CONTROLLER_AUDIENCE_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_VIEW_CONTROLLER_AUDIENCE_H_

// Audience for the History Sync view controller.
@protocol HistorySyncViewControllerAudience <NSObject>

// Notifies that the view appeared with hidden action buttons.
- (void)viewAppearedWithHiddenButtons;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_VIEW_CONTROLLER_AUDIENCE_H_
