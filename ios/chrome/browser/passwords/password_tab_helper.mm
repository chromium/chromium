// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/password_tab_helper.h"

#import "base/check.h"
#import "base/memory/ptr_util.h"
#import "base/metrics/histogram_macros.h"
#import "components/password_manager/core/browser/manage_passwords_referrer.h"
#import "components/password_manager/core/browser/password_manager_constants.h"
#import "ios/chrome/browser/passwords/password_controller.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

PasswordTabHelper::~PasswordTabHelper() = default;

void PasswordTabHelper::SetBaseViewController(
    UIViewController* baseViewController) {
  controller_.baseViewController = baseViewController;
}

void PasswordTabHelper::SetPasswordControllerDelegate(
    id<PasswordControllerDelegate> delegate) {
  controller_.delegate = delegate;
}

void PasswordTabHelper::SetDispatcher(CommandDispatcher* dispatcher) {
  controller_.dispatcher = dispatcher;
}

id<FormSuggestionProvider> PasswordTabHelper::GetSuggestionProvider() {
  return controller_.suggestionProvider;
}

password_manager::PasswordGenerationFrameHelper*
PasswordTabHelper::GetGenerationHelper() {
  return controller_.passwordManagerDriver->GetPasswordGenerationHelper();
}

password_manager::PasswordManager* PasswordTabHelper::GetPasswordManager() {
  return controller_.passwordManager;
}

password_manager::PasswordManagerClient*
PasswordTabHelper::GetPasswordManagerClient() {
  return controller_.passwordManagerClient;
}

id<PasswordGenerationProvider>
PasswordTabHelper::GetPasswordGenerationProvider() {
  return controller_.generationProvider;
}

PasswordTabHelper::PasswordTabHelper(web::WebState* web_state)
    : web::WebStatePolicyDecider(web_state),
      controller_([[PasswordController alloc] initWithWebState:web_state]) {
  web_state->AddObserver(this);
}

void PasswordTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);
  controller_ = nil;
}

void PasswordTabHelper::ShouldAllowRequest(
    NSURLRequest* request,
    web::WebStatePolicyDecider::RequestInfo request_info,
    web::WebStatePolicyDecider::PolicyDecisionCallback callback) {
  GURL request_url = net::GURLWithNSURL(request.URL);
  if (request_info.target_frame_is_main &&
      ui::PageTransitionCoreTypeIs(request_info.transition_type,
                                   ui::PAGE_TRANSITION_LINK) &&
      request_url == GURL(password_manager::kManageMyPasswordsURL)) {
    id<ApplicationSettingsCommands> settings_command_handler =
        HandlerForProtocol(controller_.dispatcher, ApplicationSettingsCommands);

    [settings_command_handler showSavedPasswordsSettingsFromViewController:nil
                                                          showCancelButton:NO];
    std::move(callback).Run(
        web::WebStatePolicyDecider::PolicyDecision::Cancel());
    UMA_HISTOGRAM_ENUMERATION(
        "PasswordManager.ManagePasswordsReferrer",
        password_manager::ManagePasswordsReferrer::kPasswordsGoogleWebsite);
    return;
  }
  std::move(callback).Run(web::WebStatePolicyDecider::PolicyDecision::Allow());
}

void PasswordTabHelper::WebStateDestroyed() {}

WEB_STATE_USER_DATA_KEY_IMPL(PasswordTabHelper)
