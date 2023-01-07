// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_METRICS_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_METRICS_HANDLER_H_

// Protocol for informing the coordinator of metrics-trackable events that
// happen in the view, so it can fire the correct metrics.
@protocol PopupMenuMetricsHandler

// Called when the popup menu is scrolled vertically.
- (void)popupMenuScrolledVertically;

// Called when the popup menu is scrolled horizontally. This is only fired on
// the new popup menu. The ond one doesn't have a horizontal scroll.
- (void)popupMenuScrolledHorizontally;

// Called when the user takes an action in the popup menu.
- (void)popupMenuTookAction;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_METRICS_HANDLER_H_
