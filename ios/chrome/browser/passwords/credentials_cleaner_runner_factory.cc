// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/passwords/credentials_cleaner_runner_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/password_manager/core/browser/credentials_cleaner_runner.h"
#include "ios/web/public/browser_state.h"

CredentialsCleanerRunnerFactory::CredentialsCleanerRunnerFactory()
    : BrowserStateKeyedServiceFactory(
          "CredentialsCleanerRunner",
          BrowserStateDependencyManager::GetInstance()) {}

CredentialsCleanerRunnerFactory::~CredentialsCleanerRunnerFactory() = default;

CredentialsCleanerRunnerFactory*
CredentialsCleanerRunnerFactory::GetInstance() {
  static base::NoDestructor<CredentialsCleanerRunnerFactory> instance;
  return instance.get();
}

password_manager::CredentialsCleanerRunner*
CredentialsCleanerRunnerFactory::GetForBrowserState(
    web::BrowserState* browser_state) {
  return static_cast<password_manager::CredentialsCleanerRunner*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

std::unique_ptr<KeyedService>
CredentialsCleanerRunnerFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  return std::make_unique<password_manager::CredentialsCleanerRunner>();
}
