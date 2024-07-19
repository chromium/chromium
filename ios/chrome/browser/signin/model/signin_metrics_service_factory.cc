// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/model/signin_metrics_service_factory.h"

#include <utility>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/core/browser/signin_metrics_service.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/model/identity_manager_factory.h"

// static
SigninMetricsService* SigninMetricsServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<SigninMetricsService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
SigninMetricsServiceFactory* SigninMetricsServiceFactory::GetInstance() {
  static base::NoDestructor<SigninMetricsServiceFactory> instance;
  return instance.get();
}

SigninMetricsServiceFactory::SigninMetricsServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "SigninMetricsService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

SigninMetricsServiceFactory::~SigninMetricsServiceFactory() {}

std::unique_ptr<KeyedService>
SigninMetricsServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* chrome_browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<SigninMetricsService>(
      *IdentityManagerFactory::GetForBrowserState(chrome_browser_state),
      *chrome_browser_state->GetPrefs());
}

bool SigninMetricsServiceFactory::ServiceIsCreatedWithBrowserState() const {
  return true;
}

void SigninMetricsServiceFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  SigninMetricsService::RegisterProfilePrefs(registry);
}
