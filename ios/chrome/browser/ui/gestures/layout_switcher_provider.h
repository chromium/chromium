// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_GESTURES_LAYOUT_SWITCHER_PROVIDER_H_
#define IOS_CHROME_BROWSER_UI_GESTURES_LAYOUT_SWITCHER_PROVIDER_H_

#import "ios/chrome/browser/ui/gestures/layout_switcher.h"

// Protocol that provides access to a layout switcher.
@protocol LayoutSwitcherProvider

// The layout switcher.
@property(nonatomic, readonly, weak) id<LayoutSwitcher> layoutSwitcher;

@end

#endif  // IOS_CHROME_BROWSER_UI_GESTURES_LAYOUT_SWITCHER_PROVIDER_H_
