// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/identity_test_environment_browser_state_adaptor.h"

#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/signin_client_factory.h"

// static
std::unique_ptr<KeyedService>
IdentityTestEnvironmentBrowserStateAdaptor::BuildIdentityManagerForTests(
    web::BrowserState* context) {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return signin::IdentityTestEnvironment::BuildIdentityManagerForTests(
      SigninClientFactory::GetForBrowserState(browser_state),
      browser_state->GetPrefs(), browser_state->GetStatePath());
}
