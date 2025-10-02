// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_bottom_sheet_detent.h"

@implementation LensOverlayBottomSheetDetent {
  // The resolver for the detents value.
  CGFloat (^_valueResolver)();
}

- (instancetype)initWithIdentifier:(NSString*)identifier
                     valueResolver:(CGFloat (^)())valueResolver {
  self = [super init];
  if (self) {
    _identifier = identifier;
    _valueResolver = valueResolver;
  }

  return self;
}

#pragma mark - Public

- (CGFloat)value {
  if (!_valueResolver) {
    return 0;
  }
  return _valueResolver();
}

@end
