// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_METRICS_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_METRICS_HANDLER_H_

// Protocol for informing the coordinator of metrics-trackable events that
// happen in the view, so it can fire the correct metrics.
@protocol PopupMenuMetricsHandler

// Called when the popup menu is scrolled.
- (void)popupMenuScrolled;

// Called when the user takes an action in the popup menu.
- (void)popupMenuTookAction;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_METRICS_HANDLER_H_
