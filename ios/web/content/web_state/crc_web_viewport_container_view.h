// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_CONTENT_WEB_STATE_CRC_WEB_VIEWPORT_CONTAINER_VIEW_H_
#define IOS_WEB_CONTENT_WEB_STATE_CRC_WEB_VIEWPORT_CONTAINER_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/web/common/crw_content_view.h"
#import "ios/web/common/crw_viewport_adjustment_container.h"

// Container view class that manages the display of content.
@interface CRCWebViewportContainerView : UIView <CRWViewportAdjustmentContainer>

// The minimum viewport insets.
@property(nonatomic, assign) UIEdgeInsets minViewportInsets;

// The maximum viewport insets.
@property(nonatomic, assign) UIEdgeInsets maxViewportInsets;

@end

#endif  // IOS_WEB_CONTENT_WEB_STATE_CRC_WEB_VIEWPORT_CONTAINER_VIEW_H_
