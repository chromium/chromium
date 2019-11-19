// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_PRESENTATION_INFOBAR_MODAL_POSITIONER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_PRESENTATION_INFOBAR_MODAL_POSITIONER_H_

#import <UIKit/UIKit.h>

// InfobarBannerPositioner contains methods used to position the InfobarBanner.
@protocol InfobarModalPositioner

// The target height for the modal view to be presented based on |width|.
- (CGFloat)modalHeightForWidth:(CGFloat)width;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_PRESENTATION_INFOBAR_MODAL_POSITIONER_H_
