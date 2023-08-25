// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autocomplete/model/remote_suggestions_service_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/omnibox/browser/remote_suggestions_service.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

// static
RemoteSuggestionsService* RemoteSuggestionsServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state,
    bool create_if_necessary) {
  return static_cast<RemoteSuggestionsService*>(
      GetInstance()->GetServiceForBrowserState(browser_state,
                                               create_if_necessary));
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
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<RemoteSuggestionsService>(
      /*document_suggestions_service=*/nullptr,
      browser_state->GetSharedURLLoaderFactory());
}

RemoteSuggestionsServiceFactory::RemoteSuggestionsServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "RemoteSuggestionsService",
          BrowserStateDependencyManager::GetInstance()) {}

RemoteSuggestionsServiceFactory::~RemoteSuggestionsServiceFactory() {}
