// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content/web_state/crc_web_viewport_container_view.h"

#import "base/check.h"
#import "base/notreached.h"
#import "ios/web/common/crw_viewport_adjustment.h"
#import "ios/web/common/crw_viewport_adjustment_container.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CRCWebViewportContainerView () <CRWViewportAdjustment>

@end

@implementation CRCWebViewportContainerView

@synthesize viewportInsets = _viewportInsets;
@synthesize viewportEdgesAffectedBySafeArea = _viewportEdgesAffectedBySafeArea;
@synthesize minViewportInsets = _minViewportInsets;
@synthesize maxViewportInsets = _maxViewportInsets;

- (UIView<CRWViewportAdjustment>*)fullscreenViewportAdjuster {
  return self;
}

- (void)updateMinViewportInsets:(UIEdgeInsets)minInsets
              maxViewportInsets:(UIEdgeInsets)maxInsets {
  _minViewportInsets = minInsets;
  _maxViewportInsets = maxInsets;
}

@end
