// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_store_bundle/model/app_store_bundle_service_factory.h"

#import <utility>

#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/public/provider/chrome/browser/app_store_bundle/app_store_bundle_api.h"

AppStoreBundleServiceFactory::AppStoreBundleServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "AppStoreBundleService",
          ProfileSelection::kRedirectedInIncognito) {}

std::unique_ptr<KeyedService>
AppStoreBundleServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return ios::provider::CreateAppStoreBundleService();
}
