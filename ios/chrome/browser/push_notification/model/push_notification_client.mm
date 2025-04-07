// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/model/push_notification_client.h"

#import "base/notreached.h"
#import "base/task/bind_post_task.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_prefs.h"
#import "ios/chrome/browser/safety_check_notifications/utils/constants.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/tips_notifications/model/utils.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_sender.h"

PushNotificationClient::PushNotificationClient(
    PushNotificationClientId client_id,
    PushNotificationClientScope client_scope)
    : client_id_(client_id), client_scope_(client_scope) {}

PushNotificationClient::~PushNotificationClient() = default;

PushNotificationClientId PushNotificationClient::GetClientId() const {
  return client_id_;
}

PushNotificationClientScope PushNotificationClient::GetClientScope() const {
  return client_scope_;
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
      case PushNotificationClientId::kReminders:
        // Features do not support feedback.
        NOTREACHED();
    }
  }

  if (urls_delayed_for_loading_.size()) {
    for (auto& url : urls_delayed_for_loading_) {
      LoadUrlInNewTab(url.first, browser, std::move(url.second));
    }
    urls_delayed_for_loading_.clear();
  }
}

Browser* PushNotificationClient::GetSceneLevelForegroundActiveBrowser() {
  ProfileIOS* profile = GetAnyProfile();

  if (!profile) {
    return nullptr;
  }

  return GetSceneLevelForegroundActiveBrowserForProfile(profile);
}

Browser* PushNotificationClient::GetSceneLevelForegroundActiveBrowserForProfile(
    ProfileIOS* profile) {
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

void PushNotificationClient::CheckRateLimitBeforeSchedulingNotification(
    ScheduledNotificationRequest request,
    base::OnceCallback<void(NSError*)> completion) {
  base::Time last_send_tab_open =
      GetApplicationContext()->GetLocalState()->GetTime(
          push_notification_prefs::kSendTabLastOpenTimestamp);
  const base::TimeDelta time_since_open =
      base::Time::Now() - last_send_tab_open;
  if (time_since_open < base::Minutes(10)) {
    // Delay the notification if there was a Send Tab To Self Notification
    // delivered in the last 10 minutes.
    request.time_interval += base::Days(1);
    ScheduleNotification(request, std::move(completion));
    return;
  }

  auto completion_handler = base::CallbackToBlock(base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&PushNotificationClient::HandlePendingNotificationResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(request),
                     std::move(completion))));

  [UNUserNotificationCenter.currentNotificationCenter
      getPendingNotificationRequestsWithCompletionHandler:completion_handler];
}

void PushNotificationClient::HandlePendingNotificationResult(
    ScheduledNotificationRequest notification,
    base::OnceCallback<void(NSError*)> completion,
    NSArray<UNNotificationRequest*>* requests) {
  if ([requests count] > 0) {
    // Delay a tips notification if there is a scheduled Safety Check
    // notification.
    NSArray* safetyCheckIds = @[
      kSafetyCheckSafeBrowsingNotificationID,
      kSafetyCheckUpdateChromeNotificationID,
      kSafetyCheckPasswordNotificationID,
    ];
    for (UNNotificationRequest* request in requests) {
      if ([notification.identifier isEqualToString:kTipsNotificationId]) {
        if ([safetyCheckIds containsObject:request.identifier]) {
          notification.time_interval += base::Days(1);
          break;
        }
      }
    }
  }
  ScheduleNotification(notification, std::move(completion));
}

void PushNotificationClient::ScheduleNotification(
    ScheduledNotificationRequest request,
    base::OnceCallback<void(NSError*)> completion) {
  auto completion_block = base::CallbackToBlock(std::move(completion));

  [UNUserNotificationCenter.currentNotificationCenter
      addNotificationRequest:CreateRequest(request)
       withCompletionHandler:completion_block];
}

UNNotificationRequest* PushNotificationClient::CreateRequest(
    ScheduledNotificationRequest request) {
  if ([request.identifier isEqualToString:kTipsNotificationId]) {
    return [UNNotificationRequest
        requestWithIdentifier:kTipsNotificationId
                      content:request.content
                      trigger:[UNTimeIntervalNotificationTrigger
                                  triggerWithTimeInterval:request.time_interval
                                                              .InSecondsF()
                                                  repeats:NO]];
  }
  NOTREACHED();
}

ProfileIOS* PushNotificationClient::GetAnyProfile() {
  std::vector<ProfileIOS*> loaded_profiles =
      GetApplicationContext()->GetProfileManager()->GetLoadedProfiles();

  if (loaded_profiles.empty()) {
    return nullptr;
  }

  return loaded_profiles.back();
}
