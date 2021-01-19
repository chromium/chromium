// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_BVC_CONTAINER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_MAIN_BVC_CONTAINER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/gestures/view_revealing_animatee.h"
#import "ios/chrome/browser/ui/thumb_strip/thumb_strip_attacher.h"

// A UIViewController instance designed to contain an instance of
// BrowserViewController ("BVC") as a child. Since the BVC itself often
// implements a great deal of custom logic around handling view controller
// presentation and other features, this containing view controller handles
// forwarding calls to the BVC instance where needed.
//
// This class isn't coupled to any implementation details of the BVC; it could
// be used as a generic forwarding container if needed. In that case, its name
// should be changed.
@interface BVCContainerViewController
    : UIViewController <ThumbStripAttacher, ViewRevealingAnimatee>

// The BVC instance being contained. If this is set, the current BVC (if any)
// will be removed as a child view controller, and the new |currentBVC| will
// be added as a child and have its view resized to this object's view's bounds.
@property(nonatomic, weak) UIViewController* currentBVC;

@end

#endif  // IOS_CHROME_BROWSER_UI_MAIN_BVC_CONTAINER_VIEW_CONTROLLER_H_
