// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_tab_helper.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#import "components/autofill/ios/browser/autofill_agent.h"
#include "components/autofill/ios/browser/autofill_driver_ios.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/ui/autofill/chrome_autofill_client_ios.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

AutofillTabHelper::~AutofillTabHelper() = default;

// static
void AutofillTabHelper::CreateForWebState(
    web::WebState* web_state,
    password_manager::PasswordManager* password_manager) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(
        UserDataKey(),
        base::WrapUnique(new AutofillTabHelper(web_state, password_manager)));
  }
}

void AutofillTabHelper::SetBaseViewController(
    UIViewController* base_view_controller) {
  autofill_client_->SetBaseViewController(base_view_controller);
}

id<FormSuggestionProvider> AutofillTabHelper::GetSuggestionProvider() {
  return autofill_agent_;
}

AutofillTabHelper::AutofillTabHelper(
    web::WebState* web_state,
    password_manager::PasswordManager* password_manager)
    : browser_state_(ios::ChromeBrowserState::FromBrowserState(
          web_state->GetBrowserState())),
      autofill_agent_([[AutofillAgent alloc]
          initWithPrefService:browser_state_->GetPrefs()
                     webState:web_state]) {
  web_state->AddObserver(this);

  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);
  DCHECK(infobar_manager);
  autofill_client_ = std::make_unique<autofill::ChromeAutofillClientIOS>(
      browser_state_, web_state, infobar_manager, autofill_agent_,
      password_manager);

  autofill::AutofillDriverIOS::PrepareForWebStateWebFrameAndDelegate(
      web_state, autofill_client_.get(), autofill_agent_,
      GetApplicationContext()->GetApplicationLocale(),
      autofill::AutofillManager::ENABLE_AUTOFILL_DOWNLOAD_MANAGER);
}

void AutofillTabHelper::WebStateDestroyed(web::WebState* web_state) {
  autofill_agent_ = nil;
  web_state->RemoveObserver(this);
}

WEB_STATE_USER_DATA_KEY_IMPL(AutofillTabHelper)
