// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This class handles subscribing to the new Google push notifications.

#ifndef JINGLE_NOTIFIER_LISTENER_PUSH_NOTIFICATIONS_SUBSCRIBE_TASK_H_
#define JINGLE_NOTIFIER_LISTENER_PUSH_NOTIFICATIONS_SUBSCRIBE_TASK_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "jingle/notifier/listener/notification_defines.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"
#include "third_party/libjingle_xmpp/xmpp/xmpptask.h"

namespace notifier {
class PushNotificationsSubscribeTask : public jingle_xmpp::XmppTask {
 public:
  class Delegate {
   public:
     virtual ~Delegate() {}
     virtual void OnSubscribed() = 0;
     virtual void OnSubscriptionError() = 0;
  };

  PushNotificationsSubscribeTask(jingle_xmpp::XmppTaskParentInterface* parent,
                                 const SubscriptionList& subscriptions,
                                 Delegate* delegate);
  ~PushNotificationsSubscribeTask() override;

  // Overridden from XmppTask.
  int ProcessStart() override;
  int ProcessResponse() override;
  bool HandleStanza(const jingle_xmpp::XmlElement* stanza) override;

 private:
  // Assembles an Xmpp stanza which can be sent to subscribe to notifications.
  static jingle_xmpp::XmlElement* MakeSubscriptionMessage(
      const SubscriptionList& subscriptions,
      const jingle_xmpp::Jid& jid, const std::string& task_id);

  SubscriptionList subscriptions_;
  Delegate* delegate_;

  FRIEND_TEST_ALL_PREFIXES(PushNotificationsSubscribeTaskTest,
                           MakeSubscriptionMessage);

  DISALLOW_COPY_AND_ASSIGN(PushNotificationsSubscribeTask);
};

typedef PushNotificationsSubscribeTask::Delegate
    PushNotificationsSubscribeTaskDelegate;


}  // namespace notifier

#endif  // JINGLE_NOTIFIER_LISTENER_PUSH_NOTIFICATIONS_SUBSCRIBE_TASK_H_
