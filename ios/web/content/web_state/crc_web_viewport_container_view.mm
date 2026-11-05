// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content/web_state/crc_web_viewport_container_view.h"

#import "ios/web/common/crw_viewport_adjustment.h"
#import "ios/web/common/crw_viewport_adjustment_container.h"

@interface CRCWebViewportContainerView () <CRWViewportAdjustment>

@end

@implementation CRCWebViewportContainerView

@synthesize viewportInsets = _viewportInsets;
@synthesize viewportEdgesAffectedBySafeArea = _viewportEdgesAffectedBySafeArea;
@synthesize webViewResizingType = _webViewResizingType;

- (id)init {
  if ((self = [super init])) {
    self.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  }
  return self;
}

- (UIView<CRWViewportAdjustment>*)fullscreenViewportAdjuster {
  return self;
}

- (void)updateMinViewportInsets:(UIEdgeInsets)minInsets
              maxViewportInsets:(UIEdgeInsets)maxInsets {
  // The native view handles toolbar space without separate Blink controls.
}

@end
