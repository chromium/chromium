// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This class listens for notifications from the Google Push notifications
// service, and signals when they arrive.  It checks all incoming stanzas to
// see if they look like notifications, and filters out those which are not
// valid.
//
// The task is deleted automatically by the jingle_xmpp::XmppClient. This occurs in the
// destructor of TaskRunner, which is a superclass of jingle_xmpp::XmppClient.

#ifndef JINGLE_NOTIFIER_LISTENER_PUSH_NOTIFICATIONS_LISTEN_TASK_H_
#define JINGLE_NOTIFIER_LISTENER_PUSH_NOTIFICATIONS_LISTEN_TASK_H_

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "third_party/libjingle_xmpp/xmpp/xmpptask.h"

namespace jingle_xmpp {
class XmlElement;
}

namespace notifier {

struct Notification;

class PushNotificationsListenTask : public jingle_xmpp::XmppTask {
 public:
  class Delegate {
   public:
    virtual void OnNotificationReceived(const Notification& notification) = 0;

   protected:
    virtual ~Delegate();
  };

  PushNotificationsListenTask(jingle_xmpp::XmppTaskParentInterface* parent,
                              Delegate* delegate);

  PushNotificationsListenTask(const PushNotificationsListenTask&) = delete;
  PushNotificationsListenTask& operator=(const PushNotificationsListenTask&) =
      delete;

  ~PushNotificationsListenTask() override;

  // Overriden from jingle_xmpp::XmppTask.
  int ProcessStart() override;
  int ProcessResponse() override;
  bool HandleStanza(const jingle_xmpp::XmlElement* stanza) override;

 private:
  bool IsValidNotification(const jingle_xmpp::XmlElement* stanza);

  raw_ptr<Delegate> delegate_;
};

typedef PushNotificationsListenTask::Delegate
    PushNotificationsListenTaskDelegate;

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_LISTENER_PUSH_NOTIFICATIONS_LISTEN_TASK_H_
