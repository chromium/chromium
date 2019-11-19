// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/signin_browser_state_info_updater.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include <string>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/browser_state_info_cache.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"

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

  identity_manager_observer_.Add(identity_manager_);

  signin_error_controller_observer_.Add(signin_error_controller);

  UpdateBrowserStateInfo();
  // TODO(crbug.com/908457): Call OnErrorChanged() here, to catch any change
  // that happened since the construction of SigninErrorController. BrowserState
  // metrics depend on this bug and must be fixed first.
}

SigninBrowserStateInfoUpdater::~SigninBrowserStateInfoUpdater() = default;

void SigninBrowserStateInfoUpdater::Shutdown() {
  identity_manager_observer_.RemoveAll();
  signin_error_controller_observer_.RemoveAll();
}

void SigninBrowserStateInfoUpdater::UpdateBrowserStateInfo() {
  ios::ChromeBrowserStateManager* browser_state_manager =
      GetApplicationContext()->GetChromeBrowserStateManager();
  BrowserStateInfoCache* cache =
      browser_state_manager->GetBrowserStateInfoCache();
  size_t index = cache->GetIndexOfBrowserStateWithPath(browser_state_path_);

  if (index == std::string::npos)
    return;

  if (identity_manager_->HasPrimaryAccount()) {
    CoreAccountInfo account_info = identity_manager_->GetPrimaryAccountInfo();
    cache->SetAuthInfoOfBrowserStateAtIndex(
        index, account_info.gaia, base::UTF8ToUTF16(account_info.email));
  } else {
    cache->SetAuthInfoOfBrowserStateAtIndex(index, /*gaia_id=*/std::string(),
                                            /*user_name=*/base::string16());
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

void SigninBrowserStateInfoUpdater::OnPrimaryAccountSet(
    const CoreAccountInfo& primary_account_info) {
  UpdateBrowserStateInfo();
}

void SigninBrowserStateInfoUpdater::OnPrimaryAccountCleared(
    const CoreAccountInfo& previous_primary_account_info) {
  UpdateBrowserStateInfo();
}
