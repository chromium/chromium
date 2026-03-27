// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_UTILS_H_
#define IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_UTILS_H_

#import <UIKit/UIKit.h>

// The position of the app bar.
enum class AppBarPosition {
  kNone,
  kBottom,
  kLeft,
  kRight,
};

// Returns the position of the app bar based on the view's window orientation.
AppBarPosition AppBarPositionForView(UIView* view);

#endif  // IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_UTILS_H_
