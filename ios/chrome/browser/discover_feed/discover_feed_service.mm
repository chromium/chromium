// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/discover_feed/discover_feed_service.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/ntp/feed_metrics_recorder.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/discover_feed/discover_feed_configuration.h"
#import "ios/public/provider/chrome/browser/discover_feed/discover_feed_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DiscoverFeedService::DiscoverFeedService(
    PrefService* pref_service,
    AuthenticationService* authentication_service,
    signin::IdentityManager* identity_manager) {
  if (identity_manager)
    identity_manager_observation_.Observe(identity_manager);

  feed_metrics_recorder_ = [[FeedMetricsRecorder alloc] init];

  DiscoverFeedConfiguration* discover_config =
      [[DiscoverFeedConfiguration alloc] init];
  discover_config.authService = authentication_service;
  discover_config.prefService = pref_service;
  discover_config.metricsRecorder = feed_metrics_recorder_;
  ios::GetChromeBrowserProvider().GetDiscoverFeedProvider()->StartFeedService(
      discover_config);
}

DiscoverFeedService::~DiscoverFeedService() {}

void DiscoverFeedService::Shutdown() {
  identity_manager_observation_.Reset();

  // Stop the Discover feed to disconnects its services.
  ios::GetChromeBrowserProvider().GetDiscoverFeedProvider()->StopFeed();

  feed_metrics_recorder_ = nil;
}

void DiscoverFeedService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  ios::GetChromeBrowserProvider()
      .GetDiscoverFeedProvider()
      ->UpdateFeedForAccountChange();
}
