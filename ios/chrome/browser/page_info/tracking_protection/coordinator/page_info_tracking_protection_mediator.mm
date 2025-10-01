// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/page_info/tracking_protection/coordinator/page_info_tracking_protection_mediator.h"

#import "base/feature_list.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/privacy_sandbox/privacy_sandbox_features.h"
#import "ios/chrome/browser/page_info/tracking_protection/ui/page_info_tracking_protection_info.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"

@implementation PageInfoTrackingProtectionMediator {
  raw_ptr<web::WebState> _webState;
  raw_ptr<privacy_sandbox::TrackingProtectionSettings>
      _trackingProtectionSettings;
  PageInfoTrackingProtectionInfo* _trackingProtectionInfo;
}

- (instancetype)initWithWebState:(web::WebState*)webState
      trackingProtectionSettings:(privacy_sandbox::TrackingProtectionSettings*)
                                     trackingProtectionSettings {
  self = [super init];
  if (self) {
    _webState = webState;
    _trackingProtectionSettings = trackingProtectionSettings;
    _trackingProtectionInfo = [[PageInfoTrackingProtectionInfo alloc]
        initWithHasTrackingProtectionException:
            [self hasTrackingProtectionException]
                shouldShowTrackingProtectionUI:
                    [self shouldShowTrackingProtectionsUI]];
  }
  return self;
}

#pragma mark - Public

// Setter for setting ViewController as the consumer.
- (void)setConsumer:(id<PageInfoTrackingProtectionConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;
  [self.consumer setTrackingProtectionInfo:_trackingProtectionInfo];
}

#pragma mark - Private

// Gets the current website URL.
- (GURL)currentURL {
  return _webState->GetNavigationManager()->GetVisibleItem()->GetVirtualURL();
}

// Determines if the TrackingProtection section should be shown.
- (BOOL)shouldShowTrackingProtectionsUI {
  return _trackingProtectionSettings->IsFpProtectionEnabled() &&
         base::FeatureList::IsEnabled(privacy_sandbox::kActUserBypassUx);
}

// Determines if there is an exception for the current site.
- (BOOL)hasTrackingProtectionException {
  return _trackingProtectionSettings->HasTrackingProtectionException(
      [self currentURL]);
}

#pragma mark - PageInfoTrackingProtectionMutator

// Toggles the tracking protection state and updates the backend.
- (void)toggleTrackingProtectionState {
  const GURL& URL = [self currentURL];
  if ([self hasTrackingProtectionException]) {
    base::RecordAction(base::UserMetricsAction(
        "PageInfo.PrivacySubpage.ReenableTrackingProtections"));
    _trackingProtectionSettings->RemoveTrackingProtectionException(URL);
  } else {
    base::RecordAction(base::UserMetricsAction(
        "PageInfo.PrivacySubpage.PauseTrackingProtections"));
    _trackingProtectionSettings->AddTrackingProtectionException(URL);
  }
  _trackingProtectionInfo.hasTrackingProtectionException =
      [self hasTrackingProtectionException];
  [self.consumer setTrackingProtectionInfo:_trackingProtectionInfo];
}

@end
