// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/model/push_notification_client.h"

#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_sender.h"

PushNotificationClient::PushNotificationClient(
    PushNotificationClientId client_id)
    : client_id_(client_id) {}

PushNotificationClient::~PushNotificationClient() = default;

PushNotificationClientId PushNotificationClient::GetClientId() {
  return client_id_;
}

void PushNotificationClient::OnSceneActiveForegroundBrowserReady() {
  if (!urls_delayed_for_loading_.size() && !feedback_presentation_delayed_) {
    return;
  }

  // TODO(crbug.com/41497027): The notifications should probbaly be linked
  // to a specific profile, and thus this should check that the code here
  // use the correct profile.
  Browser* browser = GetSceneLevelForegroundActiveBrowser();
  CHECK(browser);

  if (feedback_presentation_delayed_) {
    id<ApplicationCommands> handler =
        static_cast<id<ApplicationCommands>>(browser->GetCommandDispatcher());
    switch (feedback_presentation_delayed_client_) {
      case PushNotificationClientId::kContent:
      case PushNotificationClientId::kSports:
        [handler
            showReportAnIssueFromViewController:browser->GetSceneState()
                                                    .window.rootViewController
                                         sender:UserFeedbackSender::
                                                    ContentNotification
                            specificProductData:feedback_data_];
        feedback_presentation_delayed_ = false;
        break;
      case PushNotificationClientId::kTips:
      case PushNotificationClientId::kCommerce:
      case PushNotificationClientId::kSendTab:
      case PushNotificationClientId::kSafetyCheck:
        // Features do not support feedback.
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }

  if (urls_delayed_for_loading_.size()) {
    for (auto& url : urls_delayed_for_loading_) {
      LoadUrlInNewTab(url.first, browser, std::move(url.second));
    }
    urls_delayed_for_loading_.clear();
  }
}

// TODO(crbug.com/41497027): Current implementation returns any Scene. Instead
// the notification should includes some way to identify the associated profile
// to use (maybe by including the gaia id of the associated profile).
Browser* PushNotificationClient::GetSceneLevelForegroundActiveBrowser() {
  ProfileIOS* profile = GetAnyProfile();
  if (!profile) {
    return nullptr;
  }

  std::set<Browser*> browsers =
      BrowserListFactory::GetForProfile(profile)->BrowsersOfType(
          BrowserList::BrowserType::kRegular);

  for (Browser* browser : browsers) {
    if (browser->GetSceneState().activationLevel ==
        SceneActivationLevelForegroundActive) {
      return browser;
    }
  }
  return nullptr;
}

void PushNotificationClient::LoadUrlInNewTab(const GURL& url) {
  LoadUrlInNewTab(url, base::DoNothing());
}

void PushNotificationClient::LoadUrlInNewTab(
    const GURL& url,
    base::OnceCallback<void(Browser*)> callback) {
  Browser* browser = GetSceneLevelForegroundActiveBrowser();
  if (!browser) {
    urls_delayed_for_loading_.emplace_back(url, std::move(callback));
    return;
  }

  LoadUrlInNewTab(url, browser, std::move(callback));
}

void PushNotificationClient::LoadUrlInNewTab(
    const GURL& url,
    Browser* browser,
    base::OnceCallback<void(Browser*)> callback) {
  id<ApplicationCommands> handler =
      static_cast<id<ApplicationCommands>>(browser->GetCommandDispatcher());
  [handler openURLInNewTab:[OpenNewTabCommand commandWithURLFromChrome:url]];
  std::move(callback).Run(browser);
}

void PushNotificationClient::LoadFeedbackWithPayloadAndClientId(
    NSDictionary<NSString*, NSString*>* data,
    PushNotificationClientId client) {
  Browser* browser = GetSceneLevelForegroundActiveBrowser();
  if (!browser && data) {
    feedback_presentation_delayed_client_ = client;
    feedback_presentation_delayed_ = true;
    feedback_data_ = data;
    return;
  }
}

ProfileIOS* PushNotificationClient::GetAnyProfile() {
  std::vector<ProfileIOS*> loaded_profiles =
      GetApplicationContext()->GetProfileManager()->GetLoadedProfiles();

  if (loaded_profiles.empty()) {
    return nullptr;
  }

  return loaded_profiles.back();
}
