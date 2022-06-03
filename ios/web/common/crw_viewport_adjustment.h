// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_COMMON_CRW_VIEWPORT_ADJUSTMENT_H_
#define IOS_WEB_COMMON_CRW_VIEWPORT_ADJUSTMENT_H_

#import <UIKit/UIKit.h>

// Protocol used to update a page's viewport when part of the web view is
// covered by browser UI (e.g. toolbars).
@protocol CRWViewportAdjustment <NSObject>
// The viewport insets into the web view that are covered by the Browser UI.
@property(nonatomic, assign) UIEdgeInsets viewportInsets;
// The viewport's affected viewport edges that are affected by the web view's
// safe area insets.
@property(nonatomic, readonly) UIRectEdge viewportEdgesAffectedBySafeArea;

// Updates maximum and minimum viewport insets for the wrapper.
- (void)updateMinViewportInsets:(UIEdgeInsets)minInsets
              maxViewportInsets:(UIEdgeInsets)maxInsets;

@end

#endif  // IOS_WEB_COMMON_CRW_VIEWPORT_ADJUSTMENT_H_
