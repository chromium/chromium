// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_DYNAMIC_TYPE_UTIL_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_DYNAMIC_TYPE_UTIL_H_

#import <UIKit/UIKit.h>

// ********************
// Specific cases utils
// ********************

// The normal font for the LocationBarSteadyView.
UIFont* LocationBarSteadyViewFont(UIContentSizeCategory currentCategory);

// A smaller category font for the LocationBarSteadyView for use when the large
// Contextual Panel entrypoint is being shown in the location bar.
UIFont* SmallLocationBarSteadyViewFont(UIContentSizeCategory currentCategory);

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_DYNAMIC_TYPE_UTIL_H_
