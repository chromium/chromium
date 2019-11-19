// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_NOTIFIER_LISTENER_NON_BLOCKING_PUSH_CLIENT_H_
#define JINGLE_NOTIFIER_LISTENER_NON_BLOCKING_PUSH_CLIENT_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "jingle/notifier/listener/push_client.h"
#include "jingle/notifier/listener/push_client_observer.h"

namespace base {
class SingleThreadTaskRunner;
} // namespace base

namespace notifier {

// This class implements a PushClient that doesn't block; it delegates
// to another blocking PushClient on a separate thread.
//
// This class must be used on a single thread.
class NonBlockingPushClient : public PushClient {
 public:
  // The type for a function that creates a (blocking) PushClient.
  // Will be called on the delegate task runner.
  typedef base::Callback<std::unique_ptr<PushClient>()>
      CreateBlockingPushClientCallback;

  // Runs the given callback on the given task runner, and delegates
  // to that PushClient.
  explicit NonBlockingPushClient(
      const scoped_refptr<base::SingleThreadTaskRunner>& delegate_task_runner,
      const CreateBlockingPushClientCallback&
          create_blocking_push_client_callback);
  ~NonBlockingPushClient() override;

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

 private:
  class Core;

  void OnNotificationsEnabled();
  void OnNotificationsDisabled(NotificationsDisabledReason reason);
  void OnIncomingNotification(const Notification& notification);
  void OnPingResponse();

  base::ThreadChecker thread_checker_;
  const scoped_refptr<base::SingleThreadTaskRunner> delegate_task_runner_;
  scoped_refptr<Core> core_;

  base::ObserverList<PushClientObserver>::Unchecked observers_;

  base::WeakPtrFactory<NonBlockingPushClient> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NonBlockingPushClient);
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_LISTENER_NON_BLOCKING_PUSH_CLIENT_H_
