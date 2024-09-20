// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autocomplete/model/remote_suggestions_service_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/omnibox/browser/remote_suggestions_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

// static
RemoteSuggestionsService* RemoteSuggestionsServiceFactory::GetForProfile(
    ProfileIOS* profile,
    bool create_if_necessary) {
  return static_cast<RemoteSuggestionsService*>(
      GetInstance()->GetServiceForBrowserState(profile, create_if_necessary));
}

// static
RemoteSuggestionsServiceFactory*
RemoteSuggestionsServiceFactory::GetInstance() {
  static base::NoDestructor<RemoteSuggestionsServiceFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyedService>
RemoteSuggestionsServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  return std::make_unique<RemoteSuggestionsService>(
      /*document_suggestions_service=*/nullptr,
      profile->GetSharedURLLoaderFactory());
}

RemoteSuggestionsServiceFactory::RemoteSuggestionsServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "RemoteSuggestionsService",
          BrowserStateDependencyManager::GetInstance()) {}

RemoteSuggestionsServiceFactory::~RemoteSuggestionsServiceFactory() {}
