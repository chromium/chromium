// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PRESENTERS_VERTICAL_ANIMATION_CONTAINER_H_
#define IOS_CHROME_BROWSER_UI_PRESENTERS_VERTICAL_ANIMATION_CONTAINER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/presenters/contained_presenter.h"

// Helper that manages the positioning and presentation of a view controller,
// anchoring at the bottom of its container, and animating it up from below.
@interface VerticalAnimationContainer : NSObject<ContainedPresenter>
@end

#endif  // IOS_CHROME_BROWSER_UI_PRESENTERS_VERTICAL_ANIMATION_CONTAINER_H_
