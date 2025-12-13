// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/discover_feed/model/discover_feed_service_factory.h"

#import "ios/chrome/browser/discover_feed/model/discover_feed_configuration.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_recorder.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/public/provider/chrome/browser/discover_feed/discover_feed_api.h"

// static
DiscoverFeedService* DiscoverFeedServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<DiscoverFeedService>(
      profile, /*create=*/true);
}

// static
DiscoverFeedService* DiscoverFeedServiceFactory::GetForProfileIfExists(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<DiscoverFeedService>(
      profile, /*create=*/false);
}

// static
DiscoverFeedServiceFactory* DiscoverFeedServiceFactory::GetInstance() {
  static base::NoDestructor<DiscoverFeedServiceFactory> instance;
  return instance.get();
}

DiscoverFeedServiceFactory::DiscoverFeedServiceFactory()
    : ProfileKeyedServiceFactoryIOS("DiscoverFeedService") {
  DependsOn(AuthenticationServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(ios::TemplateURLServiceFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
}

DiscoverFeedServiceFactory::~DiscoverFeedServiceFactory() = default;

std::unique_ptr<KeyedService>
DiscoverFeedServiceFactory::BuildServiceInstanceFor(ProfileIOS* profile) const {
  DiscoverFeedConfiguration* configuration =
      [[DiscoverFeedConfiguration alloc] init];
  configuration.profilePrefService = profile->GetPrefs();
  configuration.localStatePrefService =
      GetApplicationContext()->GetLocalState();
  configuration.authService =
      AuthenticationServiceFactory::GetForProfile(profile);
  configuration.identityManager =
      IdentityManagerFactory::GetForProfile(profile);
  configuration.metricsRecorder = [[FeedMetricsRecorder alloc]
           initWithPrefService:profile->GetPrefs()
      featureEngagementTracker:feature_engagement::TrackerFactory::
                                   GetForProfile(profile)];
  configuration.singleSignOnService =
      GetApplicationContext()->GetSingleSignOnService();
  configuration.templateURLService =
      ios::TemplateURLServiceFactory::GetForProfile(profile);
  configuration.syncService = SyncServiceFactory::GetForProfile(profile);

  return ios::provider::CreateDiscoverFeedService(configuration);
}
