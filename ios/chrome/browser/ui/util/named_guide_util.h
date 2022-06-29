// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UTIL_NAMED_GUIDE_UTIL_H_
#define IOS_CHROME_BROWSER_UI_UTIL_NAMED_GUIDE_UTIL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/util/layout_guide_names.h"

// Creates NamedGuides with the GuideNames in `names` and adds them to `view`.
void AddNamedGuidesToView(NSArray<GuideName*>* names, UIView* view);

// Sets the constrained views for the NamedGuides indicated by the keys of
// `views_for_names` to their corresponding values in the `views_for_names`.
void SetNamedGuideConstrainedViews(
    NSDictionary<GuideName*, UIView*>* views_for_names);

#endif  // IOS_CHROME_BROWSER_UI_UTIL_NAMED_GUIDE_UTIL_H_
