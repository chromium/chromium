// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_PUBLIC_HISTORY_PRESENTATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_PUBLIC_HISTORY_PRESENTATION_DELEGATE_H_

// Delegate used to make the tab UI visible.
@protocol HistoryPresentationDelegate
// Tells the delegate to show the non-incognito tab UI. NO-OP if the correct tab
// UI is already visible. Delegate may also dismiss history.
- (void)showActiveRegularTabFromHistory;
// Tells the delegate to show the incognito tab UI. NO-OP if the correct tab UI
// is already visible. Delegate may also dismiss history.
- (void)showActiveIncognitoTabFromHistory;
@end

#endif  // IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_PUBLIC_HISTORY_PRESENTATION_DELEGATE_H_
