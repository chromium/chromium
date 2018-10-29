// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_POSITIONER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_POSITIONER_H_

#import <UIKit/UIKit.h>

// InfobarPositioner contains methods that are used to position the infobars
// on the screen.
@protocol InfobarPositioner

// View to which the popup view should be added as subview.
- (UIView*)parentView;

// YES if |parentView| is currently visible.
- (BOOL)isParentViewVisible;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_POSITIONER_H_
