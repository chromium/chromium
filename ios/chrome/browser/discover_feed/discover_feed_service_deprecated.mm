// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/discover_feed/discover_feed_service_deprecated.h"

#import "ios/chrome/browser/ui/ntp/feed_metrics_recorder.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/discover_feed/discover_feed_configuration.h"
#import "ios/public/provider/chrome/browser/discover_feed/discover_feed_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DiscoverFeedServiceDeprecated::DiscoverFeedServiceDeprecated(
    PrefService* pref_service,
    AuthenticationService* authentication_service,
    signin::IdentityManager* identity_manager) {
  if (identity_manager)
    identity_manager_observation_.Observe(identity_manager);

  discover_feed_provider_observation_.Observe(
      ios::GetChromeBrowserProvider().GetDiscoverFeedProvider());

  feed_metrics_recorder_ = [[FeedMetricsRecorder alloc] init];

  DiscoverFeedConfiguration* discover_config =
      [[DiscoverFeedConfiguration alloc] init];
  discover_config.authService = authentication_service;
  discover_config.prefService = pref_service;
  discover_config.metricsRecorder = feed_metrics_recorder_;

  ios::GetChromeBrowserProvider().GetDiscoverFeedProvider()->StartFeedService(
      discover_config);
}

DiscoverFeedServiceDeprecated::~DiscoverFeedServiceDeprecated() = default;

void DiscoverFeedServiceDeprecated::Shutdown() {
  // Stop the Discover feed to disconnects its services.
  ios::GetChromeBrowserProvider().GetDiscoverFeedProvider()->StopFeedService();

  discover_feed_provider_observation_.Reset();
  identity_manager_observation_.Reset();

  feed_metrics_recorder_ = nil;
}

void DiscoverFeedServiceDeprecated::CreateFeedModels() {
  ios::GetChromeBrowserProvider().GetDiscoverFeedProvider()->CreateFeedModels();
}

void DiscoverFeedServiceDeprecated::ClearFeedModels() {
  ios::GetChromeBrowserProvider().GetDiscoverFeedProvider()->ClearFeedModels();
}

FeedMetricsRecorder* DiscoverFeedServiceDeprecated::GetFeedMetricsRecorder() {
  return ios::GetChromeBrowserProvider()
      .GetDiscoverFeedProvider()
      ->GetFeedMetricsRecorder();
}

UIViewController*
DiscoverFeedServiceDeprecated::NewDiscoverFeedViewControllerWithConfiguration(
    DiscoverFeedViewControllerConfiguration* configuration) {
  return ios::GetChromeBrowserProvider()
      .GetDiscoverFeedProvider()
      ->NewDiscoverFeedViewControllerWithConfiguration(configuration);
}

UIViewController*
DiscoverFeedServiceDeprecated::NewFollowingFeedViewControllerWithConfiguration(
    DiscoverFeedViewControllerConfiguration* configuration) {
  return ios::GetChromeBrowserProvider()
      .GetDiscoverFeedProvider()
      ->NewFollowingFeedViewControllerWithConfiguration(configuration);
}

void DiscoverFeedServiceDeprecated::RemoveFeedViewController(
    UIViewController* feed_view_controller) {
  ios::GetChromeBrowserProvider()
      .GetDiscoverFeedProvider()
      ->RemoveFeedViewController(feed_view_controller);
}

void DiscoverFeedServiceDeprecated::UpdateTheme() {
  ios::GetChromeBrowserProvider().GetDiscoverFeedProvider()->UpdateTheme();
}

void DiscoverFeedServiceDeprecated::RefreshFeedIfNeeded() {
  ios::GetChromeBrowserProvider()
      .GetDiscoverFeedProvider()
      ->RefreshFeedIfNeeded();
}

void DiscoverFeedServiceDeprecated::RefreshFeed() {
  ios::GetChromeBrowserProvider().GetDiscoverFeedProvider()->RefreshFeed();
}

void DiscoverFeedServiceDeprecated::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  ios::GetChromeBrowserProvider()
      .GetDiscoverFeedProvider()
      ->UpdateFeedForAccountChange();
}

void DiscoverFeedServiceDeprecated::OnDiscoverFeedModelRecreated() {
  NotifyDiscoverFeedModelRecreated();
}
