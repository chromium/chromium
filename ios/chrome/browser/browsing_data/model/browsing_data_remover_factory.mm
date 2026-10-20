// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browsing_data/model/browsing_data_remover_factory.h"

#import <utility>

#import "ios/chrome/browser/browsing_data/model/browsing_data_remover_impl.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

BrowsingDataRemoverFactory::BrowsingDataRemoverFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "BrowsingDataRemover",
          ProfileSelection::kOwnInstanceInIncognito) {}

std::unique_ptr<KeyedService>
BrowsingDataRemoverFactory::BuildServiceInstanceFor(ProfileIOS* profile) const {
  // TODO(crbug.com/40940855): the factory should declare the services
  // used by BrowsingDataRemoverImpl and inject them in the constructor.
  return std::make_unique<BrowsingDataRemoverImpl>(profile);
}
