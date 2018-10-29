// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/autofill/web_view_strike_database_factory.h"

#include <utility>

#include "base/memory/singleton.h"
#include "components/autofill/core/browser/strike_database.h"
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
  return base::Singleton<WebViewStrikeDatabaseFactory>::get();
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
  return std::make_unique<autofill::StrikeDatabase>(
      browser_state->GetStatePath().Append(
          FILE_PATH_LITERAL("AutofillStrikeDatabase")));
}

}  // namespace ios_web_view
