// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/signin_browser_state_info_updater.h"

#import <string>

#import "base/strings/utf_string_conversions.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_info_cache.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state_manager.h"

SigninBrowserStateInfoUpdater::SigninBrowserStateInfoUpdater(
    signin::IdentityManager* identity_manager,
    SigninErrorController* signin_error_controller,
    const base::FilePath& browser_state_path)
    : identity_manager_(identity_manager),
      signin_error_controller_(signin_error_controller),
      browser_state_path_(browser_state_path) {
  // Some tests don't have a ChromeBrowserStateManager, disable this service.
  if (!GetApplicationContext()->GetChromeBrowserStateManager())
    return;

  identity_manager_observation_.Observe(identity_manager_);

  signin_error_controller_observation_.Observe(signin_error_controller);

  UpdateBrowserStateInfo();
  // TODO(crbug.com/908457): Call OnErrorChanged() here, to catch any change
  // that happened since the construction of SigninErrorController. BrowserState
  // metrics depend on this bug and must be fixed first.
}

SigninBrowserStateInfoUpdater::~SigninBrowserStateInfoUpdater() = default;

void SigninBrowserStateInfoUpdater::Shutdown() {
  identity_manager_observation_.Reset();
  signin_error_controller_observation_.Reset();
}

void SigninBrowserStateInfoUpdater::UpdateBrowserStateInfo() {
  ios::ChromeBrowserStateManager* browser_state_manager =
      GetApplicationContext()->GetChromeBrowserStateManager();
  BrowserStateInfoCache* cache =
      browser_state_manager->GetBrowserStateInfoCache();
  size_t index = cache->GetIndexOfBrowserStateWithPath(browser_state_path_);

  if (index == std::string::npos)
    return;

  if (identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    CoreAccountInfo account_info =
        identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
    cache->SetAuthInfoOfBrowserStateAtIndex(
        index, account_info.gaia, base::UTF8ToUTF16(account_info.email));
  } else {
    cache->SetAuthInfoOfBrowserStateAtIndex(index, /*gaia_id=*/std::string(),
                                            /*user_name=*/std::u16string());
  }
}

void SigninBrowserStateInfoUpdater::OnErrorChanged() {
  BrowserStateInfoCache* cache = GetApplicationContext()
                                     ->GetChromeBrowserStateManager()
                                     ->GetBrowserStateInfoCache();
  size_t index = cache->GetIndexOfBrowserStateWithPath(browser_state_path_);
  if (index == std::string::npos)
    return;

  cache->SetBrowserStateIsAuthErrorAtIndex(
      index, signin_error_controller_->HasError());
}

void SigninBrowserStateInfoUpdater::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  UpdateBrowserStateInfo();
}
