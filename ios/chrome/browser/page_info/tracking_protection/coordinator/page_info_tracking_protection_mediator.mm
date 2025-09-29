// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/page_info/tracking_protection/coordinator/page_info_tracking_protection_mediator.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/page_info/tracking_protection/ui/page_info_tracking_protection_info.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"

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

// Setter for setting ViewController as the consumer.
- (void)setConsumer:(id<PageInfoTrackingProtectionConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;
  [self dispatchInitialTrackingProtectionInfo];
}

#pragma mark - Private

// Gets the current website URL.
- (GURL)currentURL {
  return _webState->GetNavigationManager()->GetVisibleItem()->GetVirtualURL();
}

// Dispatches the initial state to the ViewController.
- (void)dispatchInitialTrackingProtectionInfo {
  PageInfoTrackingProtectionInfo* info =
      [[PageInfoTrackingProtectionInfo alloc] init];

  info.shouldShowTrackingProtectionUI = [self shouldShowTrackingProtectionsUI];
  info.hasTrackingProtectionException = [self hasTrackingProtectionException];

  [self.consumer setTrackingProtectionInfo:info];
}

// Determines if the TrackingProtection section should be shown.
- (BOOL)shouldShowTrackingProtectionsUI {
  return _trackingProtectionSettings->IsFpProtectionEnabled();
}

// Determines if there is an exception for the current site.
- (BOOL)hasTrackingProtectionException {
  return _trackingProtectionSettings->HasTrackingProtectionException(
      [self currentURL]);
}

#pragma mark - PageInfoTrackingProtectionMutator

- (void)toggleTrackingProtections {
  // TODO(crbug.com/442799468): Implement toggling logic.
}

@end
