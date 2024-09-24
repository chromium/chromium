// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/providers/lens/test_lens_overlay_controller.h"

#import "ios/public/provider/chrome/browser/lens/lens_configuration.h"
#import "ios/public/provider/chrome/browser/lens/lens_overlay_result.h"

@implementation TestLensOverlayController

- (BOOL)isPanningSelectionUI {
  return NO;
}

- (void)setLensOverlayDelegate:(id<ChromeLensOverlayDelegate>)delegate {
  // NO-OP
}

- (void)setQueryText:(NSString*)text clearSelection:(BOOL)clearSelection {
  // NO-OP
}

- (void)start {
  // NO-OP
}

- (void)reloadResult:(id<ChromeLensOverlayResult>)result {
  // NO-OP
}

- (void)removeSelectionWithClearText:(BOOL)clearText {
  // NO-OP
}

- (void)setOcclusionInsets:(UIEdgeInsets)occlusionInsets
                reposition:(BOOL)reposition
                  animated:(BOOL)animated {
  // NO-OP
}

- (void)resetSelectionAreaToInitialPosition:(void (^)())completion {
  // NO-OP
}

@end
