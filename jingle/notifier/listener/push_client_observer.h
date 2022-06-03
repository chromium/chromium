// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_NOTIFIER_LISTENER_PUSH_CLIENT_OBSERVER_H_
#define JINGLE_NOTIFIER_LISTENER_PUSH_CLIENT_OBSERVER_H_

#include "jingle/notifier/listener/notification_defines.h"

namespace notifier {

enum NotificationsDisabledReason {
  // There is an underlying transient problem (e.g., network- or
  // XMPP-related).
  TRANSIENT_NOTIFICATION_ERROR,
  DEFAULT_NOTIFICATION_ERROR = TRANSIENT_NOTIFICATION_ERROR,
  // Our credentials have been rejected.
  NOTIFICATION_CREDENTIALS_REJECTED,
  // No error (useful for avoiding keeping a separate bool for
  // notifications enabled/disabled).
  NO_NOTIFICATION_ERROR
};

// A PushClientObserver is notified when notifications are enabled or
// disabled, and when a notification is received.
class PushClientObserver {
 protected:
  virtual ~PushClientObserver();

 public:
  // Called when notifications are enabled.
  virtual void OnNotificationsEnabled() = 0;

  // Called when notifications are disabled, with the reason (not
  // equal to NO_ERROR) in |reason|.
  virtual void OnNotificationsDisabled(
      NotificationsDisabledReason reason) = 0;

  // Called when a notification is received.  The details of the
  // notification are in |notification|.
  virtual void OnIncomingNotification(const Notification& notification) = 0;

  // Called when a ping response is received. Default implementation does
  // nothing.
  virtual void OnPingResponse();
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_LISTENER_PUSH_CLIENT_OBSERVER_H_
