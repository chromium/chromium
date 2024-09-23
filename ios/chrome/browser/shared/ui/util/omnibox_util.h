// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_OMNIBOX_UTIL_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_OMNIBOX_UTIL_H_

#import <UIKit/UIKit.h>

class Browser;
@class LayoutGuideCenter;

/// Returns true if the omnibox is at the bottom in the current layout.
bool IsCurrentLayoutBottomOmnibox(LayoutGuideCenter* layout_guide_center);
bool IsCurrentLayoutBottomOmnibox(Browser* browser);

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_OMNIBOX_UTIL_H_
