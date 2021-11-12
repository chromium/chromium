// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Methods for sending the update stanza to notify peers via xmpp.

#ifndef JINGLE_NOTIFIER_LISTENER_PUSH_NOTIFICATIONS_SEND_UPDATE_TASK_H_
#define JINGLE_NOTIFIER_LISTENER_PUSH_NOTIFICATIONS_SEND_UPDATE_TASK_H_

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "jingle/notifier/listener/notification_defines.h"
#include "third_party/libjingle_xmpp/xmpp/xmpptask.h"

namespace jingle_xmpp {
class Jid;
class XmlElement;
}  // namespace

namespace notifier {

class PushNotificationsSendUpdateTask : public jingle_xmpp::XmppTask {
 public:
  PushNotificationsSendUpdateTask(
      jingle_xmpp::XmppTaskParentInterface* parent, const Notification& notification);

  PushNotificationsSendUpdateTask(const PushNotificationsSendUpdateTask&) =
      delete;
  PushNotificationsSendUpdateTask& operator=(
      const PushNotificationsSendUpdateTask&) = delete;

  ~PushNotificationsSendUpdateTask() override;

  // Overridden from jingle_xmpp::XmppTask.
  int ProcessStart() override;

 private:
  // Allocates and constructs an jingle_xmpp::XmlElement containing the update stanza.
  static jingle_xmpp::XmlElement* MakeUpdateMessage(
      const Notification& notification, const jingle_xmpp::Jid& to_jid_bare);

  const Notification notification_;

  FRIEND_TEST_ALL_PREFIXES(PushNotificationsSendUpdateTaskTest,
                           MakeUpdateMessage);
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_LISTENER_PUSH_NOTIFICATIONS_SEND_UPDATE_TASK_H_
