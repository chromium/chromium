// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autocomplete/model/remote_suggestions_service_factory.h"

#import <utility>

#import "components/omnibox/browser/remote_suggestions_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

std::unique_ptr<KeyedService>
RemoteSuggestionsServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<RemoteSuggestionsService>(
      /*document_suggestions_service=*/nullptr,
      /*enterprise_search_aggregator_suggestions_service=*/nullptr,
      profile->GetSharedURLLoaderFactory());
}

RemoteSuggestionsServiceFactory::RemoteSuggestionsServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "RemoteSuggestionsService",
          ProfileSelection::kOwnInstanceInIncognito) {}
