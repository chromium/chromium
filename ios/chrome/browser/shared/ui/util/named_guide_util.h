// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_NAMED_GUIDE_UTIL_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_NAMED_GUIDE_UTIL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"

// Creates NamedGuides with the GuideNames in `names` and adds them to `view`.
void AddNamedGuidesToView(NSArray<GuideName*>* names, UIView* view);

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_NAMED_GUIDE_UTIL_H_
