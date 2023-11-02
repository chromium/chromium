// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/model_type_store_service_factory.h"

#include <utility>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/sync/model/model_type_store_service_impl.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"

// static
ModelTypeStoreServiceFactory* ModelTypeStoreServiceFactory::GetInstance() {
  static base::NoDestructor<ModelTypeStoreServiceFactory> instance;
  return instance.get();
}

// static
syncer::ModelTypeStoreService* ModelTypeStoreServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<syncer::ModelTypeStoreService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

ModelTypeStoreServiceFactory::ModelTypeStoreServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "ModelTypeStoreService",
          BrowserStateDependencyManager::GetInstance()) {}

ModelTypeStoreServiceFactory::~ModelTypeStoreServiceFactory() {}

std::unique_ptr<KeyedService>
ModelTypeStoreServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<syncer::ModelTypeStoreServiceImpl>(
      browser_state->GetStatePath());
}

web::BrowserState* ModelTypeStoreServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}
