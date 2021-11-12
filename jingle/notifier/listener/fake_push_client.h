// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_NOTIFIER_LISTENER_FAKE_PUSH_CLIENT_H_
#define JINGLE_NOTIFIER_LISTENER_FAKE_PUSH_CLIENT_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "jingle/notifier/listener/push_client.h"
#include "jingle/notifier/listener/push_client_observer.h"

namespace notifier {

// PushClient implementation that can be used for testing.
class FakePushClient : public PushClient {
 public:
  FakePushClient();

  FakePushClient(const FakePushClient&) = delete;
  FakePushClient& operator=(const FakePushClient&) = delete;

  ~FakePushClient() override;

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

  // Triggers OnNotificationsEnabled on all observers.
  void EnableNotifications();

  // Triggers OnNotificationsDisabled on all observers.
  void DisableNotifications(NotificationsDisabledReason reason);

  // Triggers OnIncomingNotification on all observers.
  void SimulateIncomingNotification(const Notification& notification);

  const SubscriptionList& subscriptions() const;
  const std::string& email() const;
  const std::string& token() const;
  const std::vector<Notification>& sent_notifications() const;
  int sent_pings() const;

 private:
  base::ObserverList<PushClientObserver>::Unchecked observers_;
  SubscriptionList subscriptions_;
  std::string email_;
  std::string token_;
  std::vector<Notification> sent_notifications_;
  int sent_pings_;
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_LISTENER_FAKE_PUSH_CLIENT_H_
