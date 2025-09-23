// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/page_info/tracking_protection/coordinator/page_info_tracking_protection_mediator.h"

#import "base/memory/raw_ptr.h"

@implementation PageInfoTrackingProtectionMediator {
  raw_ptr<web::WebState> _webState;
  raw_ptr<privacy_sandbox::TrackingProtectionSettings>
      _trackingProtectionSettings;
}

- (instancetype)initWithWebState:(web::WebState*)webState
      trackingProtectionSettings:(privacy_sandbox::TrackingProtectionSettings*)
                                     trackingProtectionSettings {
  self = [super init];
  if (self) {
    _webState = webState;
    _trackingProtectionSettings = trackingProtectionSettings;
  }
  return self;
}

#pragma mark - PageInfoTrackingProtectionMutator

- (void)toggleTrackingProtections {
  // TODO - implement the toggling logic here
}

@end
