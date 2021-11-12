// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_NOTIFIER_LISTENER_XMPP_PUSH_CLIENT_H_
#define JINGLE_NOTIFIER_LISTENER_XMPP_PUSH_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "jingle/notifier/base/notifier_options.h"
#include "jingle/notifier/communicator/login.h"
#include "jingle/notifier/listener/notification_defines.h"
#include "jingle/notifier/listener/push_client.h"
#include "jingle/notifier/listener/push_notifications_listen_task.h"
#include "jingle/notifier/listener/push_notifications_subscribe_task.h"
#include "jingle/notifier/listener/send_ping_task.h"
#include "third_party/libjingle_xmpp/xmpp/xmppclientsettings.h"

namespace jingle_xmpp {
class XmppTaskParentInterface;
}  // namespace jingle_xmpp

namespace notifier {

// This class implements a client for the XMPP google:push protocol.
//
// This class must be used on a single thread.
class XmppPushClient :
      public PushClient,
      public Login::Delegate,
      public PushNotificationsListenTaskDelegate,
      public PushNotificationsSubscribeTaskDelegate,
      public SendPingTaskDelegate {
 public:
  explicit XmppPushClient(const NotifierOptions& notifier_options);

  XmppPushClient(const XmppPushClient&) = delete;
  XmppPushClient& operator=(const XmppPushClient&) = delete;

  ~XmppPushClient() override;

  // PushClient implementation.
  void AddObserver(PushClientObserver* observer) override;
  void RemoveObserver(PushClientObserver* observer) override;
  void UpdateSubscriptions(const SubscriptionList& subscriptions) override;
  void UpdateCredentials(
      const std::string& email,
      const std::string& token,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override;
  void SendNotification(const Notification& notification) override;
  void SendPing() override;

  // Login::Delegate implementation.
  void OnConnect(
      base::WeakPtr<jingle_xmpp::XmppTaskParentInterface> base_task) override;
  void OnTransientDisconnection() override;
  void OnCredentialsRejected() override;

  // PushNotificationsListenTaskDelegate implementation.
  void OnNotificationReceived(const Notification& notification) override;

  // PushNotificationsSubscribeTaskDelegate implementation.
  void OnSubscribed() override;
  void OnSubscriptionError() override;

  // SendPingTaskDelegate implementation.
  void OnPingResponseReceived() override;

 private:
  base::ThreadChecker thread_checker_;
  const NotifierOptions notifier_options_;
  base::ObserverList<PushClientObserver>::Unchecked observers_;

  // XMPP connection settings.
  SubscriptionList subscriptions_;
  jingle_xmpp::XmppClientSettings xmpp_settings_;

  std::unique_ptr<notifier::Login> login_;

  // The XMPP connection.
  base::WeakPtr<jingle_xmpp::XmppTaskParentInterface> base_task_;

  std::vector<Notification> pending_notifications_to_send_;
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_LISTENER_XMPP_PUSH_CLIENT_H_
