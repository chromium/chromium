// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_bottom_sheet_detent_proxy.h"

@implementation LensOverlayBottomSheetDetentProxy

- (NSString*)identifier {
  if (_systemDetent) {
    return _systemDetent.identifier;
  }

  return _lensOverlayDetent.identifier;
}

- (instancetype)initWithSystemDetent:
    (UISheetPresentationControllerDetent*)systemDetent {
  self = [super init];
  if (self) {
    _systemDetent = systemDetent;
  }

  return self;
}

- (instancetype)initWithLensOverlayDetent:
    (LensOverlayBottomSheetDetent*)lensOverlayDetent {
  self = [super init];
  if (self) {
    _lensOverlayDetent = lensOverlayDetent;
  }

  return self;
}

@end
