// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_APP_NOTIFICATION_PRESENTER_H_
#define REMOTING_IOS_APP_NOTIFICATION_PRESENTER_H_

#import <Foundation/Foundation.h>

#include <optional>

#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/threading/sequence_bound.h"
#include "base/timer/timer.h"
#include "remoting/client/notification/notification_client.h"
#include "remoting/client/notification/notification_message.h"

namespace remoting {

// Singleton class to present a notification message on the app. Message will
// be presented whenever the signed-in user is changed and a matching message
// is found, while no message has been previously presented.
class NotificationPresenter final {
 public:
  static NotificationPresenter* GetInstance();

  NotificationPresenter(const NotificationPresenter&) = delete;
  NotificationPresenter& operator=(const NotificationPresenter&) = delete;

  void Start();

 private:
  friend class base::NoDestructor<NotificationPresenter>;

  enum class State {
    NOT_FETCHED,
    FETCHING,
    FETCHED,
  };

  NotificationPresenter();
  ~NotificationPresenter() = delete;

  void FetchNotification();
  void OnNotificationFetched(std::optional<NotificationMessage> notification);

  NotificationClient notification_client_;

  base::OneShotTimer fetch_notification_timer_;

  // nil if the presenter is not started.
  id<NSObject> user_update_observer_ = nil;

  State state_ = State::NOT_FETCHED;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace remoting

#endif  // REMOTING_IOS_APP_NOTIFICATION_PRESENTER_H_
