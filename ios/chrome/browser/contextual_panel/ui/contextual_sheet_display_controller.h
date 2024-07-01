// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_UI_CONTEXTUAL_SHEET_DISPLAY_CONTROLLER_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_UI_CONTEXTUAL_SHEET_DISPLAY_CONTROLLER_H_

// Protocol to allow users of the ContextualSheetViewController to control
// its display.
@protocol ContextualSheetDisplayController

// Alerts the controller of the height of the sheet content.
- (void)setContentHeight:(CGFloat)height;

@end

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_UI_CONTEXTUAL_SHEET_DISPLAY_CONTROLLER_H_
