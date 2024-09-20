// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/coordinator/fake_chrome_lens_overlay.h"

#import "base/apple/foundation_util.h"
#import "ios/public/provider/chrome/browser/lens/lens_overlay_result.h"
#import "url/gurl.h"

/// ChromeLensOverlayResult test object.
@interface TestChromeLensOverlayResult : NSObject <ChromeLensOverlayResult>

/// The result URL that is meant to be loaded in the LRP.
@property(nonatomic, assign) GURL searchResultURL;
/// The selected portion of the original snapshot.
@property(nonatomic, strong) UIImage* selectionPreviewImage;
/// Data containing the suggest signals.
@property(nonatomic, strong) NSData* suggestSignals;
/// Query text.
@property(nonatomic, strong) NSString* queryText;

@end

@implementation TestChromeLensOverlayResult
@end

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
  TestChromeLensOverlayResult* resultObject =
      base::apple::ObjCCastStrict<TestChromeLensOverlayResult>(result);
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

#pragma mark - Public

- (void)simulateSelectionUpdate {
  [self sendNewResult];
}

#pragma mark - Private

- (void)sendNewResult {
  TestChromeLensOverlayResult* result =
      [[TestChromeLensOverlayResult alloc] init];
  result.queryText = _currentQueryText;
  result.searchResultURL = self.resultURL;
  result.selectionPreviewImage = [[UIImage alloc] init];
  result.suggestSignals = nil;

  self.lastResult = result;

  [self.lensOverlayDelegate lensOverlayDidStartSearchRequest:self];
  [self.lensOverlayDelegate lensOverlay:self didGenerateResult:result];
}

@end
