// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/password_tab_helper.h"

#import "base/check.h"
#import "base/memory/ptr_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "components/password_manager/core/browser/manage_passwords_referrer.h"
#import "components/password_manager/core/browser/password_manager_constants.h"
#import "ios/chrome/browser/passwords/model/password_controller.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "net/base/apple/url_conversions.h"

PasswordTabHelper::~PasswordTabHelper() = default;

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

SharedPasswordController* PasswordTabHelper::GetSharedPasswordController() {
  return controller_.sharedPasswordController;
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
    id<SettingsCommands> settings_command_handler =
        HandlerForProtocol(controller_.dispatcher, SettingsCommands);

    [settings_command_handler showSavedPasswordsSettingsFromViewController:nil
                                                          showCancelButton:NO];
    std::move(callback).Run(
        web::WebStatePolicyDecider::PolicyDecision::Cancel());
    UMA_HISTOGRAM_ENUMERATION(
        "PasswordManager.ManagePasswordsReferrer",
        password_manager::ManagePasswordsReferrer::kPasswordsGoogleWebsite);
    base::RecordAction(
        base::UserMetricsAction("MobileWebsiteOpenPasswordManager"));
    return;
  }
  std::move(callback).Run(web::WebStatePolicyDecider::PolicyDecision::Allow());
}

void PasswordTabHelper::WebStateDestroyed() {}

WEB_STATE_USER_DATA_KEY_IMPL(PasswordTabHelper)
