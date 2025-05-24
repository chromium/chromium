// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"

#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/signin_client_factory.h"

// static
std::unique_ptr<KeyedService>
IdentityTestEnvironmentBrowserStateAdaptor::BuildIdentityManagerForTests(
    web::BrowserState* context) {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  return signin::IdentityTestEnvironment::BuildIdentityManagerForTests(
      SigninClientFactory::GetForProfile(profile), profile->GetPrefs(),
      profile->GetStatePath());
}
