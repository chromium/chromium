// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/coordinator/fake_chrome_lens_overlay.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/lens_overlay/coordinator/fake_chrome_lens_overlay_result.h"
#import "ios/public/provider/chrome/browser/lens/lens_overlay_result.h"
#import "url/gurl.h"

@implementation FakeChromeLensOverlay {
  NSString* _currentQueryText;
  BOOL _started;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _resultURL = GURL("https://default-response-url.com");
    _currentQueryText = @"";
  }
  return self;
}

#pragma mark - ChromeLensOverlay

- (BOOL)isTextSelection {
  return NO;
}

- (BOOL)isPanningSelectionUI {
  return NO;
}

- (void)setQueryText:(NSString*)text clearSelection:(BOOL)clearSelection {
  _currentQueryText = text;
  [self sendNewResult];
}

- (void)start {
  _started = YES;
}

- (void)reloadResult:(id<ChromeLensOverlayResult>)result {
  self.lastReload = result;
  // Reload the result.
  FakeChromeLensOverlayResult* resultObject =
      base::apple::ObjCCastStrict<FakeChromeLensOverlayResult>(result);
  // Reloading a result generates new URL and Image, so they are not copied.
  _currentQueryText = resultObject.queryText;
  [self sendNewResult];
}

- (void)removeSelectionWithClearText:(BOOL)clearText {
  if (clearText) {
    _currentQueryText = @"";
  }
  [self sendNewResult];
}

- (void)setOcclusionInsets:(UIEdgeInsets)occlusionInsets
                reposition:(BOOL)reposition
                  animated:(BOOL)animated {
  // NO-OP
}

// Resets the selection area to the initial position.
- (void)resetSelectionAreaToInitialPosition:(void (^)())completion {
  // NO-OP
}

- (void)hideUserSelection {
  // NO-OP
}

- (void)setTopIconsHidden:(BOOL)hidden {
  // NO-OP
}

- (void)disableFlyoutMenu:(BOOL)disable {
  // NO-OP
}

#pragma mark - Public

- (void)simulateSelectionUpdate {
  [self sendNewResult];
}

- (void)simulateSuggestSignalsUpdate:(NSData*)signals {
  FakeChromeLensOverlayResult* mutableResult =
      base::apple::ObjCCastStrict<FakeChromeLensOverlayResult>(self.lastResult);

  mutableResult.suggestSignals = signals;
  [self.lensOverlayDelegate lensOverlay:self
        suggestSignalsAvailableOnResult:self.lastResult];
}

#pragma mark - Private

- (void)sendNewResult {
  FakeChromeLensOverlayResult* result =
      [[FakeChromeLensOverlayResult alloc] init];
  result.queryText = _currentQueryText;
  result.searchResultURL = self.resultURL;
  result.selectionPreviewImage = [[UIImage alloc] init];
  result.suggestSignals = nil;

  self.lastResult = result;

  [self.lensOverlayDelegate lensOverlayDidStartSearchRequest:self];
  [self.lensOverlayDelegate lensOverlay:self didGenerateResult:result];
}

@end
