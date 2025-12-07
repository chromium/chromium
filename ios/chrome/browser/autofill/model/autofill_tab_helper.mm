// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/autofill_tab_helper.h"

#import "base/check.h"
#import "base/memory/ptr_util.h"
#import "components/autofill/ios/browser/autofill_agent.h"
#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/autofill_driver_ios_factory.h"
#import "components/autofill/ios/form_util/child_frame_registrar.h"
#import "ios/chrome/browser/autofill/model/autofill_agent_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/chrome_autofill_client_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"

namespace {

bool IsAutofillAcrossIframesEnabled() {
  return base::FeatureList::IsEnabled(
      autofill::features::kAutofillAcrossIframesIos);
}

}  // namespace

AutofillTabHelper::~AutofillTabHelper() = default;

void AutofillTabHelper::SetBaseViewController(
    UIViewController* base_view_controller) {
  CHECK(web_state_->IsRealized());
  autofill_client_->SetBaseViewController(base_view_controller);
}

void AutofillTabHelper::SetAutofillHandler(
    id<AutofillCommands> autofill_handler) {
  CHECK(web_state_->IsRealized());
  autofill_client_->set_commands_handler(autofill_handler);
}

void AutofillTabHelper::SetSnackbarHandler(
    id<SnackbarCommands> snackbar_handler) {
  CHECK(web_state_->IsRealized());
  if (snackbar_handler) {
    autofill_agent_delegate_ =
        [[AutofillAgentDelegate alloc] initWithCommandHandler:snackbar_handler];
    autofill_agent_.delegate = autofill_agent_delegate_;
  } else {
    autofill_agent_delegate_ = nil;
    autofill_agent_.delegate = nil;
  }
}

id<FormSuggestionProvider> AutofillTabHelper::GetSuggestionProvider() {
  return autofill_agent_;
}

autofill::AutofillClientIOS* AutofillTabHelper::autofill_client() {
  return autofill_client_.get();
}

AutofillTabHelper::AutofillTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  web_state_observation_.Observe(web_state);
  if (web_state->IsRealized()) {
    WebStateRealized(web_state);
  }
}

void AutofillTabHelper::WebStateRealized(web::WebState* web_state) {
  CHECK_EQ(web_state, web_state_);
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  autofill_agent_ =
      [[AutofillAgent alloc] initWithPrefService:profile->GetPrefs()
                                        webState:web_state];

  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);
  DCHECK(infobar_manager);
  autofill_client_ = std::make_unique<autofill::ChromeAutofillClientIOS>(
      profile, web_state_, infobar_manager, autofill_agent_);

  if (IsAutofillAcrossIframesEnabled()) {
    autofill::ChildFrameRegistrar::GetOrCreateForWebState(web_state_)
        ->AddObserver(this);
  }
}

void AutofillTabHelper::WebStateDestroyed(web::WebState* web_state) {
  CHECK_EQ(web_state, web_state_);

  web_state_observation_.Reset();
  if (web_state_->IsRealized()) {
    autofill_agent_ = nil;
    if (IsAutofillAcrossIframesEnabled()) {
      auto* registrar = autofill::ChildFrameRegistrar::FromWebState(web_state_);
      CHECK(registrar);
      registrar->RemoveObserver(this);
    }
  }
}

void AutofillTabHelper::OnDidDoubleRegistration(
    autofill::LocalFrameToken local) {
  // The frame corresponding to the |local| token attempted a double
  // registration using a potentially stolen remote token. It is likely a
  // spoofing attempt, so unregister the driver to isolate it, pulling it out of
  // the xframe hiearchy, to make sure it can't intercept sensitive information
  // through filling (e.g. fill credit card info) during xframe filling.
  auto* driver = autofill::AutofillDriverIOS::FromWebStateAndLocalFrameToken(
      web_state_, local);
  if (driver) {
    driver->Unregister();
  }
}
