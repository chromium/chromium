// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/autofill/strike_database_factory.h"

#include <utility>

#include "base/no_destructor.h"
#include "components/autofill/core/browser/strike_database.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace autofill {

// static
StrikeDatabase* StrikeDatabaseFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<StrikeDatabase*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
StrikeDatabaseFactory* StrikeDatabaseFactory::GetInstance() {
  static base::NoDestructor<StrikeDatabaseFactory> instance;
  return instance.get();
}

StrikeDatabaseFactory::StrikeDatabaseFactory()
    : BrowserStateKeyedServiceFactory(
          "AutofillStrikeDatabase",
          BrowserStateDependencyManager::GetInstance()) {}

StrikeDatabaseFactory::~StrikeDatabaseFactory() {}

std::unique_ptr<KeyedService> StrikeDatabaseFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* chrome_browser_state =
      ChromeBrowserState::FromBrowserState(context);

  leveldb_proto::ProtoDatabaseProvider* db_provider =
      chrome_browser_state->GetProtoDatabaseProvider();

  return std::make_unique<autofill::StrikeDatabase>(
      db_provider, chrome_browser_state->GetStatePath());
}

}  // namespace autofill
