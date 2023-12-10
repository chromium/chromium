// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/ios/app/notification_presenter.h"

#import <MaterialComponents/MaterialDialogs.h>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/strings/sys_string_conversions.h"
#include "remoting/base/string_resources.h"
#include "remoting/client/chromoting_client_runtime.h"
#import "remoting/ios/app/notification_dialog_view_controller.h"
#include "remoting/ios/app/view_utils.h"
#import "remoting/ios/facade/remoting_authentication.h"
#import "remoting/ios/facade/remoting_service.h"
#import "remoting/ios/persistence/remoting_preferences.h"
#include "ui/base/l10n/l10n_util.h"

namespace remoting {

namespace {

// This is to make sure notification shows up after the user status toast (see
// UserStatusPresenter).
// TODO(yuweih): Chain this class with UserStatusPresenter on the
// kUserDidUpdate event.
constexpr base::TimeDelta kFetchNotificationDelay = base::Seconds(2);

// The "Don't show again" checkbox only appears if the notification has been
// shown for at least |kTimesShownRequiredToAllowSilencing| times. Note that
// this means the checkbox shows up starting from the
// (|kTimesShownRequiredToAllowSilencing| + 1)th app launch.
constexpr unsigned int kTimesShownRequiredToAllowSilencing = 2u;

enum NotificationUiState : unsigned int {
  NOT_SHOWN = 0,
  SHOWN = 1,
  SILENCED = 2,
};

NotificationUiState NSNumberToUiState(NSNumber* number) {
  return number ? static_cast<NotificationUiState>(number.unsignedIntValue)
                : NOT_SHOWN;
}

NSNumber* UiStateToNSNumber(NotificationUiState state) {
  return @(static_cast<unsigned int>(state));
}

}  // namespace

// static
NotificationPresenter* NotificationPresenter::GetInstance() {
  static base::NoDestructor<NotificationPresenter> instance;
  return instance.get();
}

NotificationPresenter::NotificationPresenter()
    : notification_client_(
          ChromotingClientRuntime::GetInstance()->network_task_runner()) {}

void NotificationPresenter::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!user_update_observer_);

  FetchNotification();
  user_update_observer_ = [NSNotificationCenter.defaultCenter
      addObserverForName:kUserDidUpdate
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification*) {
                // This implicitly captures |this|, but should be fine since
                // |NotificationPresenter| is singleton.
                FetchNotification();
              }];
}

void NotificationPresenter::FetchNotification() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ == State::FETCHED) {
    return;
  }
  UserInfo* user = RemotingService.instance.authentication.user;
  std::string user_email = [user isAuthenticated]
                               ? base::SysNSStringToUTF8(user.userEmail)
                               : std::string();

  state_ = State::FETCHING;
  // If FetchNotification() is called when the timer is already running, this
  // will restart the timer with the new user email.
  fetch_notification_timer_.Start(
      FROM_HERE, kFetchNotificationDelay,
      base::BindOnce(
          [](NotificationPresenter* that, const std::string& user_email) {
            that->notification_client_.GetNotification(
                user_email,
                base::BindOnce(&NotificationPresenter::OnNotificationFetched,
                               base::Unretained(that)));
          },
          base::Unretained(this), user_email));
}

void NotificationPresenter::OnNotificationFetched(
    std::optional<NotificationMessage> notification) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(State::FETCHING, state_);

  if (!notification) {
    // No notification for this user. Need to check again when user changes.
    state_ = State::NOT_FETCHED;
    return;
  }

  state_ = State::FETCHED;

  DCHECK(!notification->message_id.empty());

  std::string last_seen_message_id =
      base::SysNSStringToUTF8([RemotingPreferences.instance
          objectForFlag:RemotingFlagLastSeenNotificationMessageId]);
  NotificationUiState ui_state = NSNumberToUiState([RemotingPreferences.instance
      objectForFlag:RemotingFlagNotificationUiState]);
  if (notification->allow_silence && ui_state == SILENCED &&
      last_seen_message_id == notification->message_id) {
    return;
  }

  if (last_seen_message_id != notification->message_id) {
    [RemotingPreferences.instance
        setObject:base::SysUTF8ToNSString(notification->message_id)
          forFlag:RemotingFlagLastSeenNotificationMessageId];
    ui_state = NOT_SHOWN;
    [RemotingPreferences.instance setObject:UiStateToNSNumber(ui_state)
                                    forFlag:RemotingFlagNotificationUiState];
    [RemotingPreferences.instance setObject:@(0u)
                                    forFlag:RemotingFlagNotificationShownTimes];
    [RemotingPreferences.instance synchronizeFlags];
  }

  NSNumber* notificationShownTimesNsNumber = [RemotingPreferences.instance
      objectForFlag:RemotingFlagNotificationShownTimes];
  unsigned int notification_shown_times =
      notificationShownTimesNsNumber
          ? notificationShownTimesNsNumber.unsignedIntValue
          : 0u;

  BOOL allowSilence =
      notification->allow_silence && ui_state == SHOWN &&
      notification_shown_times >= kTimesShownRequiredToAllowSilencing;
  NotificationDialogViewController* dialogVc =
      [[NotificationDialogViewController alloc]
          initWithNotificationMessage:*notification
                         allowSilence:allowSilence];
  [dialogVc presentOnTopVCWithCompletion:^(BOOL isSilenced) {
    NotificationUiState new_ui_state = isSilenced ? SILENCED : SHOWN;
    [RemotingPreferences.instance setObject:UiStateToNSNumber(new_ui_state)
                                    forFlag:RemotingFlagNotificationUiState];
    [RemotingPreferences.instance setObject:@(notification_shown_times + 1)
                                    forFlag:RemotingFlagNotificationShownTimes];
    [RemotingPreferences.instance synchronizeFlags];
  }];
}

}  // namespace remoting
