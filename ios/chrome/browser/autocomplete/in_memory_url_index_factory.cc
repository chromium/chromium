// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/autocomplete/in_memory_url_index_factory.h"

#include <utility>

#include "base/bind.h"
#include "base/no_destructor.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/omnibox/browser/in_memory_url_index.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#include "ios/components/webui/web_ui_url_constants.h"

namespace ios {

namespace {

std::unique_ptr<KeyedService> BuildInMemoryURLIndex(
    web::BrowserState* context) {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);

  SchemeSet allowed_schemes;
  allowed_schemes.insert(kChromeUIScheme);

  // Do not force creation of the HistoryService if saving history is disabled.
  std::unique_ptr<InMemoryURLIndex> in_memory_url_index(new InMemoryURLIndex(
      ios::BookmarkModelFactory::GetForBrowserState(browser_state),
      ios::HistoryServiceFactory::GetForBrowserState(
          browser_state, ServiceAccessType::IMPLICIT_ACCESS),
      ios::TemplateURLServiceFactory::GetForBrowserState(browser_state),
      browser_state->GetStatePath(), allowed_schemes));
  in_memory_url_index->Init();
  return in_memory_url_index;
}

}  // namespace

// static
InMemoryURLIndex* InMemoryURLIndexFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<InMemoryURLIndex*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
InMemoryURLIndexFactory* InMemoryURLIndexFactory::GetInstance() {
  static base::NoDestructor<InMemoryURLIndexFactory> instance;
  return instance.get();
}

InMemoryURLIndexFactory::InMemoryURLIndexFactory()
    : BrowserStateKeyedServiceFactory(
          "InMemoryURLIndex",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::BookmarkModelFactory::GetInstance());
  DependsOn(ios::HistoryServiceFactory::GetInstance());
  DependsOn(ios::TemplateURLServiceFactory::GetInstance());
}

InMemoryURLIndexFactory::~InMemoryURLIndexFactory() {}

// static
BrowserStateKeyedServiceFactory::TestingFactory
InMemoryURLIndexFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildInMemoryURLIndex);
}

std::unique_ptr<KeyedService> InMemoryURLIndexFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildInMemoryURLIndex(context);
}

web::BrowserState* InMemoryURLIndexFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

bool InMemoryURLIndexFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace ios
