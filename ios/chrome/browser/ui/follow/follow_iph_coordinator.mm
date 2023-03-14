// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/follow/follow_iph_coordinator.h"

#import "ios/chrome/browser/discover_feed/discover_feed_service.h"
#import "ios/chrome/browser/discover_feed/discover_feed_service_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/ntp/metrics/feed_metrics_recorder.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FollowIPHCoordinator

#pragma mark - FollowIPHPresenter

- (void)presentFollowWhileBrowsingIPH {
  id<BrowserCoordinatorCommands> browserCommandsHandler =
      static_cast<id<BrowserCoordinatorCommands>>(
          self.browser->GetCommandDispatcher());
  [browserCommandsHandler showFollowWhileBrowsingIPH];
  FeedMetricsRecorder* feedMetricsRecorder =
      DiscoverFeedServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState())
          ->GetFeedMetricsRecorder();
  [feedMetricsRecorder recordFollowRecommendationIPHShown];
}

@end
