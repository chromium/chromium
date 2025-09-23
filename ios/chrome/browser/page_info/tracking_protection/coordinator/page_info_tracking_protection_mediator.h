// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAGE_INFO_TRACKING_PROTECTION_COORDINATOR_PAGE_INFO_TRACKING_PROTECTION_MEDIATOR_H_
#define IOS_CHROME_BROWSER_PAGE_INFO_TRACKING_PROTECTION_COORDINATOR_PAGE_INFO_TRACKING_PROTECTION_MEDIATOR_H_

#import "components/privacy_sandbox/tracking_protection_settings.h"
#import "ios/chrome/browser/page_info/tracking_protection/ui/page_info_tracking_protection_consumer.h"
#import "ios/chrome/browser/page_info/tracking_protection/ui/page_info_tracking_protection_mutator.h"
#import "ios/web/public/web_state.h"

// Mediator for TrackingProtectionSettings frontend UI.
@interface PageInfoTrackingProtectionMediator
    : NSObject <PageInfoTrackingProtectionMutator>

// ViewController (consumer) to dispatch UI updates to.
@property(nonatomic, weak) id<PageInfoTrackingProtectionConsumer> consumer;

- (instancetype)initWithWebState:(web::WebState*)webState
      trackingProtectionSettings:(privacy_sandbox::TrackingProtectionSettings*)
                                     trackingProtectionSettings
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_PAGE_INFO_TRACKING_PROTECTION_COORDINATOR_PAGE_INFO_TRACKING_PROTECTION_MEDIATOR_H_
