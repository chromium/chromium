// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/model/push_notification_client.h"

#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/bind_post_task.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/push_notification/model/push_notification_prefs.h"
#import "ios/chrome/browser/safety_check_notifications/utils/constants.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/tips_notifications/model/utils.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_sender.h"

namespace {

// Logs a failure reason to the appropriate patterned histogram based on
// `client_id` and `reason`.
void LogProfileRequestCreationFailure(
    PushNotificationClientId client_id,
    ProfileNotificationRequestCreationFailureReason reason) {
  std::string client_name = PushNotificationClientIdToString(client_id);

  std::string histogram_name =
      base::StrCat({"IOS.PushNotification.ProfileRequestCreationFailureReason.",
                    client_name});

  base::UmaHistogramEnumeration(histogram_name, reason);
}

// Constant string for the error domain related to Profile-based local
// notifications.
const NSErrorDomain kIOSProfileLocalNotificationErrorDomain =
    @"ios_profile_local_notification_error_domain";

// `NSError` error codes specifically for Profile-based iOS notification
// handling.
enum class IOSProfileLocalNotificationErrorCode {
  // Indicates that Profile-based notification scheduling failed due to an
  // invalid or missing Profile.
  kInvalidProfile = 1,
  // Indicates that the `UNNotificationRequest` could not be created for a
  // Profile-based notification.
  kRequestCreationFailed = 2,
};

// Creates a standardized `NSError` for Profile-based notification scheduling
// failures due to an invalid or missing Profile.
NSError* CreateInvalidProfileError() {
  CHECK(IsMultiProfilePushNotificationHandlingEnabled());

  NSDictionary* user_info = @{
    NSLocalizedDescriptionKey : @"Invalid Profile provided when scheduling "
                                @"Profile-based local notification.",
  };

  return [NSError
      errorWithDomain:kIOSProfileLocalNotificationErrorDomain
                 code:static_cast<NSInteger>(
                          IOSProfileLocalNotificationErrorCode::kInvalidProfile)
             userInfo:user_info];
}

// Creates a standardized `NSError` for failures to create a
// `UNNotificationRequest` for a Profile-based notification.
NSError* CreateRequestCreationError() {
  CHECK(IsMultiProfilePushNotificationHandlingEnabled());

  NSDictionary* userInfo = @{
    NSLocalizedDescriptionKey : @"Failed to create the UNNotificationRequest "
                                @"for Profile-based local notification.",
  };

  return [NSError errorWithDomain:kIOSProfileLocalNotificationErrorDomain
                             code:static_cast<NSInteger>(
                                      IOSProfileLocalNotificationErrorCode::
                                          kRequestCreationFailed)
                         userInfo:userInfo];
}

// Helper function to add the original Profile name to a Profile-based
// notification content's `userInfo` dictionary.
void AddProfileNameToNotificationContent(UNMutableNotificationContent* content,
                                         std::string_view profile_name) {
  CHECK(IsMultiProfilePushNotificationHandlingEnabled());
  CHECK(content);
  CHECK(!profile_name.empty());

  NSMutableDictionary* mutable_user_info =
      [content.userInfo mutableCopy] ?: [NSMutableDictionary dictionary];

  std::string name = std::string(profile_name);

  mutable_user_info[kOriginatingProfileNameKey] = base::SysUTF8ToNSString(name);

  content.userInfo = mutable_user_info;
}

// Searches for a browser associated with the provided `profile`. Returns the
// first matching browser with `SceneActivationLevelForegroundActive`, or
// `nullptr` if none exists for this `profile`.
Browser* GetSceneLevelForegroundActiveBrowserForProfile(ProfileIOS* profile) {
  if (!profile) {
    return nullptr;
  }

  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile);

  if (!browser_list) {
    return nullptr;
  }

  std::set<Browser*> browsers =
      browser_list->BrowsersOfType(BrowserList::BrowserType::kRegular);

  for (Browser* browser : browsers) {
    if (!browser) {
      continue;
    }

    if (browser->GetSceneState().activationLevel ==
        SceneActivationLevelForegroundActive) {
      return browser;
    }
  }

  return nullptr;
}

}  // namespace

PushNotificationClient::PushNotificationClient(
    PushNotificationClientId client_id,
    ProfileIOS* profile)
    : client_id_(client_id),
      client_scope_(PushNotificationClientScope::kPerProfile),
      profile_(profile->AsWeakPtr()) {
  CHECK(IsMultiProfilePushNotificationHandlingEnabled());
  CHECK(profile_.get())
      << "Profile must be provided for kPerProfile client "
         "when IsMultiProfilePushNotificationHandlingEnabled() returns YES";
  // Ensure this Profile is not an off-the-record Profile.
  // Off-the-record (incognito) Profiles have an empty Profile name.
  CHECK(!profile->GetProfileName().empty())
      << "Expected a regular Profile, but GetProfileName() is empty, "
      << "indicating an off-the-record Profile.";
  CHECK(!profile->IsOffTheRecord()) << "Notifications are not supported for "
                                       "off-the-record (incognito) Profiles.";
}

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
  Browser* browser = GetActiveForegroundBrowser();
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

Browser* PushNotificationClient::GetActiveForegroundBrowser() {
  if (!IsMultiProfilePushNotificationHandlingEnabled() ||
      client_scope_ != PushNotificationClientScope::kPerProfile) {
    for (ProfileIOS* profile :
         GetApplicationContext()->GetProfileManager()->GetLoadedProfiles()) {
      if (Browser* browser =
              GetSceneLevelForegroundActiveBrowserForProfile(profile)) {
        return browser;
      }
    }

    return nullptr;
  }

  return GetSceneLevelForegroundActiveBrowserForProfile(profile_.get());
}

ProfileIOS* PushNotificationClient::GetProfile() {
  CHECK_EQ(client_scope_, PushNotificationClientScope::kPerProfile);

  return profile_.get();
}

void PushNotificationClient::LoadUrlInNewTab(const GURL& url) {
  LoadUrlInNewTab(url, base::DoNothing());
}

void PushNotificationClient::LoadUrlInNewTab(
    const GURL& url,
    base::OnceCallback<void(Browser*)> callback) {
  Browser* browser = GetActiveForegroundBrowser();
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
  Browser* browser = GetActiveForegroundBrowser();
  if (!browser && data) {
    feedback_presentation_delayed_client_ = client;
    feedback_presentation_delayed_ = true;
    feedback_data_ = data;
    return;
  }
}

void PushNotificationClient::ScheduleProfileNotification(
    ScheduledNotificationRequest request,
    base::OnceCallback<void(NSError*)> completion,
    std::string_view profile_name) {
  CHECK(IsMultiProfilePushNotificationHandlingEnabled());

  if (profile_name.empty()) {
    std::move(completion).Run(CreateInvalidProfileError());

    return;
  }

  UNNotificationRequest* notification_request =
      CreateRequestForProfile(request, profile_name);

  if (!notification_request) {
    std::move(completion).Run(CreateRequestCreationError());

    return;
  }

  auto completion_block = base::CallbackToBlock(std::move(completion));

  [UNUserNotificationCenter.currentNotificationCenter
      addNotificationRequest:notification_request
       withCompletionHandler:completion_block];
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

UNNotificationRequest* PushNotificationClient::CreateRequestForProfile(
    ScheduledNotificationRequest request,
    std::string_view profile_name) {
  CHECK(IsMultiProfilePushNotificationHandlingEnabled());

  if (profile_name.empty()) {
    LogProfileRequestCreationFailure(
        client_id_,
        ProfileNotificationRequestCreationFailureReason::kInvalidProfileName);
    return nil;
  }

  if (!request.time_interval.is_positive()) {
    LogProfileRequestCreationFailure(
        client_id_,
        ProfileNotificationRequestCreationFailureReason::kInvalidTimeInterval);
    return nil;
  }

  if (!request.identifier || request.identifier.length == 0) {
    LogProfileRequestCreationFailure(
        client_id_,
        ProfileNotificationRequestCreationFailureReason::kInvalidIdentifier);
    return nil;
  }

  if (!request.content) {
    LogProfileRequestCreationFailure(
        client_id_,
        ProfileNotificationRequestCreationFailureReason::kInvalidSourceContent);
    return nil;
  }

  UNMutableNotificationContent* mutable_content = [request.content mutableCopy];

  if (!mutable_content) {
    LogProfileRequestCreationFailure(
        client_id_,
        ProfileNotificationRequestCreationFailureReason::kContentCopyFailed);
    return nil;
  }

  AddProfileNameToNotificationContent(mutable_content, profile_name);

  UNNotificationTrigger* trigger = [UNTimeIntervalNotificationTrigger
      triggerWithTimeInterval:request.time_interval.InSecondsF()
                      repeats:NO];

  CHECK(trigger);

  return [UNNotificationRequest requestWithIdentifier:request.identifier
                                              content:mutable_content
                                              trigger:trigger];
}
