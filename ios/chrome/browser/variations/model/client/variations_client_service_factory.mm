// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/variations/model/client/variations_client_service_factory.h"

#import <utility>

#import "base/functional/bind.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/variations/model/client/variations_client_service.h"

VariationsClientServiceFactory::VariationsClientServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "VariationsClientService",
          ProfileSelection::kOwnInstanceInIncognito,
          TestingCreation::kNoServiceForTests) {}

std::unique_ptr<KeyedService>
VariationsClientServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<VariationsClientService>(profile);
}
