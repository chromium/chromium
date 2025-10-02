// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_bottom_sheet_view_controller.h"

@implementation LensOverlayBottomSheetViewController

@synthesize selectedDetentIdentifier = _selectedDetentIdentifier;
@synthesize detentsDelegate = _detentsDelegate;
@synthesize sheetDelegate = _sheetDelegate;
@synthesize detents = _detents;

- (instancetype)init {
  return [super init];
}

- (void)setContent:(UIViewController*)contentViewController {
  // NO-OP: Not implemented.
}

- (BOOL)isBottomSheetPresented {
  return NO;
}

- (void)presentAnimated:(BOOL)animated completion:(ProceduralBlock)completion {
  // NO-OP: Not implemented.
  if (completion) {
    completion();
  }
}

- (void)dismissAnimated:(BOOL)animated completion:(ProceduralBlock)completion {
  // NO-OP: Not implemented.
  if (completion) {
    completion();
  }
}

- (void)setSelectedDetentIdentifier:(NSString*)selectedDetentIdentifier
                           animated:(BOOL)animated {
  // NO-OP: Not implemented.
}

- (BOOL)isInLargestDetent {
  return NO;
}

@end
