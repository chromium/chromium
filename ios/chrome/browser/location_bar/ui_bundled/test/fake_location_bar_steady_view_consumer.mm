// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/ui_bundled/test/fake_location_bar_steady_view_consumer.h"

@implementation FakeLocationBarSteadyViewConsumer

- (void)updateLocationText:(NSString*)string clipTail:(BOOL)clipTail {
  _locationText = string;
  _clipTail = clipTail;
}

- (void)updateLocationIcon:(UIImage*)icon
        securityStatusText:(NSString*)statusText {
  _icon = icon;
  _statusText = statusText;
}

- (void)updateLocationShareable:(BOOL)shareable {
  _locationShareable = shareable;
}

- (void)updateAfterNavigatingToNTP {
}

- (void)attemptShowingLensOverlayIPH {
}

- (void)recordLensOverlayAvailability {
}

@end
