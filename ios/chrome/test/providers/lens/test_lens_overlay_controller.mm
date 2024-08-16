// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/providers/lens/test_lens_overlay_controller.h"

#import "ios/public/provider/chrome/browser/lens/lens_configuration.h"
#import "ios/public/provider/chrome/browser/lens/lens_overlay_result.h"

@implementation TestLensOverlayController

- (void)setLensOverlayDelegate:(id<ChromeLensOverlayDelegate>)delegate {
  // NO-OP
}

- (void)setQueryText:(NSString*)text {
  // NO-OP
}

- (void)start {
  // NO-OP
}

- (void)reloadResult:(id<ChromeLensOverlayResult>)result {
  // NO-OP
}

@end
