// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/coordinator/fake_chrome_lens_overlay_result.h"

#import "url/gurl.h"

@implementation FakeChromeLensOverlayResult

- (void)resultSuccessfullyLoadedInWebView {
  // NO-OP
}

- (void)resultLoadingCancelledInWebView {
  // NO-OP
}

- (void)resultFailedToLoadInWebViewWithError:(NSError*)error {
  // NO-OP
}

- (void)resultWebviewShown {
  // NO-OP
}

- (void)resultWebviewSwipedWithDirection:
    (UISwipeGestureRecognizerDirection)direction {
  // NO-OP
}

@end
