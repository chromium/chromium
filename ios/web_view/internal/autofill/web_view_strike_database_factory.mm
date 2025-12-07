// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/web_view_strike_database_factory.h"

#import <utility>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/strike_database/strike_database.h"
#import "ios/web_view/internal/app/application_context.h"
#import "ios/web_view/internal/web_view_browser_state.h"

namespace ios_web_view {

// static
strike_database::StrikeDatabase*
WebViewStrikeDatabaseFactory::GetForBrowserState(
    WebViewBrowserState* browser_state) {
  return static_cast<strike_database::StrikeDatabase*>(
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
          BrowserStateDependencyManager::GetInstance()) {}

WebViewStrikeDatabaseFactory::~WebViewStrikeDatabaseFactory() {}

std::unique_ptr<KeyedService>
WebViewStrikeDatabaseFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);

  leveldb_proto::ProtoDatabaseProvider* db_provider =
      browser_state->GetProtoDatabaseProvider();

  return std::make_unique<strike_database::StrikeDatabase>(
      db_provider, browser_state->GetStatePath());
}

}  // namespace ios_web_view
