// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autocomplete/remote_suggestions_service_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/omnibox/browser/remote_suggestions_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
      browser_state->GetSharedURLLoaderFactory());
}

RemoteSuggestionsServiceFactory::RemoteSuggestionsServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "RemoteSuggestionsService",
          BrowserStateDependencyManager::GetInstance()) {}

RemoteSuggestionsServiceFactory::~RemoteSuggestionsServiceFactory() {}
