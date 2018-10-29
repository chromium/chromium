// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/identity_test_environment_chrome_browser_state_adaptor.h"

#include "ios/chrome/browser/signin/account_tracker_service_factory.h"
#include "ios/chrome/browser/signin/fake_gaia_cookie_manager_service_builder.h"
#include "ios/chrome/browser/signin/fake_oauth2_token_service_builder.h"
#include "ios/chrome/browser/signin/fake_signin_manager_builder.h"
#include "ios/chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "ios/chrome/browser/signin/signin_manager_factory.h"

namespace {

TestChromeBrowserState::TestingFactories GetIdentityTestEnvironmentFactories() {
  return {{ios::GaiaCookieManagerServiceFactory::GetInstance(),
           base::BindRepeating(&BuildFakeGaiaCookieManagerService)},
          {ProfileOAuth2TokenServiceFactory::GetInstance(),
           base::BindRepeating(&BuildFakeOAuth2TokenService)},
          {ios::SigninManagerFactory::GetInstance(),
           base::BindRepeating(&ios::BuildFakeSigninManager)}};
}

}  // namespace

// static
std::unique_ptr<TestChromeBrowserState>
IdentityTestEnvironmentChromeBrowserStateAdaptor::
    CreateChromeBrowserStateForIdentityTestEnvironment() {
  return CreateChromeBrowserStateForIdentityTestEnvironment(
      TestChromeBrowserState::TestingFactories());
}

// static
std::unique_ptr<TestChromeBrowserState>
IdentityTestEnvironmentChromeBrowserStateAdaptor::
    CreateChromeBrowserStateForIdentityTestEnvironment(
        const TestChromeBrowserState::TestingFactories& input_factories) {
  TestChromeBrowserState::Builder builder;

  for (auto& input_factory : input_factories) {
    builder.AddTestingFactory(input_factory.first, input_factory.second);
  }

  for (auto& identity_factory : GetIdentityTestEnvironmentFactories()) {
    builder.AddTestingFactory(identity_factory.first, identity_factory.second);
  }

  return builder.Build();
}

// static
void IdentityTestEnvironmentChromeBrowserStateAdaptor::
    AppendIdentityTestEnvironmentFactories(
        TestChromeBrowserState::TestingFactories* factories_to_append_to) {
  TestChromeBrowserState::TestingFactories identity_factories =
      GetIdentityTestEnvironmentFactories();
  factories_to_append_to->insert(factories_to_append_to->end(),
                                 identity_factories.begin(),
                                 identity_factories.end());
}

IdentityTestEnvironmentChromeBrowserStateAdaptor::
    IdentityTestEnvironmentChromeBrowserStateAdaptor(
        ios::ChromeBrowserState* browser_state)
    : identity_test_env_(
          ios::AccountTrackerServiceFactory::GetForBrowserState(browser_state),
          static_cast<FakeProfileOAuth2TokenService*>(
              ProfileOAuth2TokenServiceFactory::GetForBrowserState(
                  browser_state)),
          static_cast<FakeSigninManager*>(
              ios::SigninManagerFactory::GetForBrowserState(browser_state)),
          static_cast<FakeGaiaCookieManagerService*>(
              ios::GaiaCookieManagerServiceFactory::GetForBrowserState(
                  browser_state)),
          IdentityManagerFactory::GetForBrowserState(browser_state)) {}
