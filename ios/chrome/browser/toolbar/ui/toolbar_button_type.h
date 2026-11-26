// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_BUTTON_TYPE_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_BUTTON_TYPE_H_

#import <Foundation/Foundation.h>

// The types of the toolbar's control buttons.
typedef NS_ENUM(NSUInteger, ToolbarButtonType) {
  ToolbarButtonTypeBack,
  ToolbarButtonTypeForward,
  ToolbarButtonTypeReload,
  ToolbarButtonTypeStop,
  ToolbarButtonTypeShare,
  ToolbarButtonTypeAssistant,
  ToolbarButtonTypeTabGrid,
  ToolbarButtonTypeTools,
};

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_BUTTON_TYPE_H_
