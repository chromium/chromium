// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_COMMON_CRW_VIEWPORT_ADJUSTMENT_CONTAINER_H_
#define IOS_WEB_COMMON_CRW_VIEWPORT_ADJUSTMENT_CONTAINER_H_

#import "ios/web/common/crw_viewport_adjustment.h"

@class CRWWebViewContentView;

// The container view for CRWViewportAdjustment.
@protocol CRWViewportAdjustmentContainer <NSObject>
// The CRWViewportAdjustment view being displayed.
@property(nonatomic, strong, readonly)
    UIView<CRWViewportAdjustment>* fullscreenViewportAdjuster;
@end

#endif  // IOS_WEB_COMMON_CRW_VIEWPORT_ADJUSTMENT_CONTAINER_H_
