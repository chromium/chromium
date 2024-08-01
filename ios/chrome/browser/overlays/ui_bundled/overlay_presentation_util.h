// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_PRESENTATION_UTIL_H_
#define IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_PRESENTATION_UTIL_H_

#import <UIKit/UIKit.h>

@protocol InfobarModalPositioner;

// Returns a sized frame for the given modal view defined by the
// `modalPositioner` that fits in the `containerView`.
CGRect ContainedModalFrameThatFit(id<InfobarModalPositioner> modalPositioner,
                                  UIView* containerView);

#endif  // IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_PRESENTATION_UTIL_H_
