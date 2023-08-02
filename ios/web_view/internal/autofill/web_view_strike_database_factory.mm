// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/autofill/web_view_strike_database_factory.h"

#include <utility>

#include "base/no_destructor.h"
#import "components/autofill/core/browser/strike_databases/strike_database.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/web_view/internal/app/application_context.h"
#include "ios/web_view/internal/web_view_browser_state.h"

namespace ios_web_view {

// static
autofill::StrikeDatabase* WebViewStrikeDatabaseFactory::GetForBrowserState(
    WebViewBrowserState* browser_state) {
  return static_cast<autofill::StrikeDatabase*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
WebViewStrikeDatabaseFactory* WebViewStrikeDatabaseFactory::GetInstance() {
  static base::NoDestructor<WebViewStrikeDatabaseFactory> instance;
  return instance.get();
}

WebViewStrikeDatabaseFactory::WebViewStrikeDatabaseFactory()
    : BrowserStateKeyedServiceFactory(
          "AutofillStrikeDatabase",
          BrowserStateDependencyManager::GetInstance()) {
}

WebViewStrikeDatabaseFactory::~WebViewStrikeDatabaseFactory() {}

std::unique_ptr<KeyedService>
WebViewStrikeDatabaseFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);

  leveldb_proto::ProtoDatabaseProvider* db_provider =
      browser_state->GetProtoDatabaseProvider();

  return std::make_unique<autofill::StrikeDatabase>(
      db_provider, browser_state->GetStatePath());
}

}  // namespace ios_web_view
