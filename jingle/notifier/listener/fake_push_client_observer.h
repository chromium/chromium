// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_NOTIFIER_LISTENER_FAKE_PUSH_CLIENT_OBSERVER_H_
#define JINGLE_NOTIFIER_LISTENER_FAKE_PUSH_CLIENT_OBSERVER_H_

#include "base/compiler_specific.h"
#include "jingle/notifier/listener/push_client_observer.h"

namespace notifier {

// PushClientObserver implementation that can be used for testing.
class FakePushClientObserver : public PushClientObserver {
 public:
  FakePushClientObserver();
  ~FakePushClientObserver() override;

  // PushClientObserver implementation.
  void OnNotificationsEnabled() override;
  void OnNotificationsDisabled(NotificationsDisabledReason reason) override;
  void OnIncomingNotification(const Notification& notification) override;

  NotificationsDisabledReason last_notifications_disabled_reason() const;
  const Notification& last_incoming_notification() const;

 private:
  NotificationsDisabledReason last_notifications_disabled_reason_;
  Notification last_incoming_notification_;
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_LISTENER_FAKE_PUSH_CLIENT_OBSERVER_H_
