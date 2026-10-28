// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/private_insights/model/private_insights_service_factory.h"

#import <memory>
#import <utility>

#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "components/private_insights/private_insights_features.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

std::unique_ptr<KeyedService> BuildPrivateInsightsService(ProfileIOS& profile) {
  if (!base::FeatureList::IsEnabled(
          private_insights::kPrivateInsightsFeature)) {
    return nullptr;
  }
  // FCP state (e.g. OpStats) lives under the profile state path, so two
  // profiles never share contribution history.
  auto service = std::make_unique<private_insights::PrivateInsightsService>(
      GetApplicationContext()->GetLocalState(), profile.GetStatePath(),
      profile.GetSharedURLLoaderFactory());
  service->Init();
  return service;
}

}  // namespace

PrivateInsightsServiceFactory::PrivateInsightsServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "PrivateInsightsService",
          ProfileSelection::kNoInstanceInIncognito,
          ServiceCreation::kCreateWithProfile,
          TestingCreation::kNoServiceForTests) {}

// static
PrivateInsightsServiceFactory::TestingFactoryByRef
PrivateInsightsServiceFactory::GetDefaultFactory() {
  return base::BindOnce(&BuildPrivateInsightsService);
}

std::unique_ptr<KeyedService>
PrivateInsightsServiceFactory::BuildServiceInstanceFor(
    ProfileIOS& profile) const {
  return BuildPrivateInsightsService(profile);
}
