// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content/web_state/crc_web_viewport_container_view.h"

#import "base/check.h"
#import "base/notreached.h"
#import "ios/web/common/crw_viewport_adjustment.h"
#import "ios/web/common/crw_viewport_adjustment_container.h"

@interface CRCWebViewportContainerView () <CRWViewportAdjustment>

@end

@implementation CRCWebViewportContainerView

@synthesize viewportInsets = _viewportInsets;
@synthesize viewportEdgesAffectedBySafeArea = _viewportEdgesAffectedBySafeArea;
@synthesize minViewportInsets = _minViewportInsets;
@synthesize maxViewportInsets = _maxViewportInsets;

- (id)init {
  if ((self = [super init])) {
    // TODO(crbug.com/40272999): `updateMinViewportInsets` is not called when
    // FullscreenSmoothScrollingDefault is disabled, so we populate them here.
    // We cannot load them from FullscreenController because that would make
    // this code dependant on UI. Rather we will need to propagated the values
    // down to the active WebState.
    _minViewportInsets = UIEdgeInsetsMake(79, 0, 0, 0);
    _maxViewportInsets = UIEdgeInsetsMake(109, 0, 78, 0);
  }
  return self;
}

- (UIView<CRWViewportAdjustment>*)fullscreenViewportAdjuster {
  return self;
}

- (void)updateMinViewportInsets:(UIEdgeInsets)minInsets
              maxViewportInsets:(UIEdgeInsets)maxInsets {
  _minViewportInsets = minInsets;
  _maxViewportInsets = maxInsets;
}

@end
