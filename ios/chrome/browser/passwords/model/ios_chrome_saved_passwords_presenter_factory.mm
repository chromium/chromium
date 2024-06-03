// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/ios_chrome_saved_passwords_presenter_factory.h"

#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "ios/chrome/browser/affiliations/model/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/webauthn/model/ios_passkey_model_factory.h"

namespace IOSChromeSavedPasswordsPresenterFactory {

password_manager::SavedPasswordsPresenter* GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return new password_manager::SavedPasswordsPresenter(
      IOSChromeAffiliationServiceFactory::GetForBrowserState(browser_state),
      IOSChromeProfilePasswordStoreFactory::GetForBrowserState(
          browser_state, ServiceAccessType::EXPLICIT_ACCESS),
      IOSChromeAccountPasswordStoreFactory::GetForBrowserState(
          browser_state, ServiceAccessType::EXPLICIT_ACCESS),
      IOSPasskeyModelFactory::GetForBrowserState(browser_state));
}

}  // namespace IOSChromeSavedPasswordsPresenterFactory
