// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/listener/non_blocking_push_client.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "jingle/notifier/listener/push_client_observer.h"

namespace notifier {

// All methods are called on the delegate thread unless specified
// otherwise.
class NonBlockingPushClient::Core
    : public base::RefCountedThreadSafe<NonBlockingPushClient::Core>,
      public PushClientObserver {
 public:
  // Called on the parent thread.
  explicit Core(
      const scoped_refptr<base::SingleThreadTaskRunner>&
          delegate_task_runner,
      const base::WeakPtr<NonBlockingPushClient>& parent_push_client);

  // Must be called after being created.
  //
  // This is separated out from the constructor since posting tasks
  // from the constructor is dangerous.
  void CreateOnDelegateThread(
      const CreateBlockingPushClientCallback&
          create_blocking_push_client_callback);

  // Must be called before being destroyed.
  void DestroyOnDelegateThread();

  void UpdateSubscriptions(const SubscriptionList& subscriptions);
  void UpdateCredentials(
      const std::string& email,
      const std::string& token,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);
  void SendNotification(const Notification& data);
  void SendPing();

  void OnNotificationsEnabled() override;
  void OnNotificationsDisabled(NotificationsDisabledReason reason) override;
  void OnIncomingNotification(const Notification& notification) override;
  void OnPingResponse() override;

 private:
  friend class base::RefCountedThreadSafe<NonBlockingPushClient::Core>;

  // Called on either the parent thread or the delegate thread.
  ~Core() override;

  const scoped_refptr<base::SingleThreadTaskRunner> parent_task_runner_;
  const scoped_refptr<base::SingleThreadTaskRunner> delegate_task_runner_;

  const base::WeakPtr<NonBlockingPushClient> parent_push_client_;
  std::unique_ptr<PushClient> delegate_push_client_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

NonBlockingPushClient::Core::Core(
    const scoped_refptr<base::SingleThreadTaskRunner>& delegate_task_runner,
    const base::WeakPtr<NonBlockingPushClient>& parent_push_client)
    : parent_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      delegate_task_runner_(delegate_task_runner),
      parent_push_client_(parent_push_client) {}

NonBlockingPushClient::Core::~Core() {
  DCHECK(parent_task_runner_->BelongsToCurrentThread() ||
         delegate_task_runner_->BelongsToCurrentThread());
  DCHECK(!delegate_push_client_.get());
}

void NonBlockingPushClient::Core::CreateOnDelegateThread(
    const CreateBlockingPushClientCallback&
        create_blocking_push_client_callback) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  DCHECK(!delegate_push_client_.get());
  delegate_push_client_ = create_blocking_push_client_callback.Run();
  delegate_push_client_->AddObserver(this);
}

void NonBlockingPushClient::Core::DestroyOnDelegateThread() {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  DCHECK(delegate_push_client_.get());
  delegate_push_client_->RemoveObserver(this);
  delegate_push_client_.reset();
}

void NonBlockingPushClient::Core::UpdateSubscriptions(
    const SubscriptionList& subscriptions) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  DCHECK(delegate_push_client_.get());
  delegate_push_client_->UpdateSubscriptions(subscriptions);
}

void NonBlockingPushClient::Core::UpdateCredentials(
    const std::string& email,
    const std::string& token,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  DCHECK(delegate_push_client_.get());
  delegate_push_client_->UpdateCredentials(email, token, traffic_annotation);
}

void NonBlockingPushClient::Core::SendNotification(
    const Notification& notification) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  DCHECK(delegate_push_client_.get());
  delegate_push_client_->SendNotification(notification);
}

void NonBlockingPushClient::Core::SendPing() {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  DCHECK(delegate_push_client_.get());
  delegate_push_client_->SendPing();
}

void NonBlockingPushClient::Core::OnNotificationsEnabled() {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  parent_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&NonBlockingPushClient::OnNotificationsEnabled,
                                parent_push_client_));
}

void NonBlockingPushClient::Core::OnNotificationsDisabled(
    NotificationsDisabledReason reason) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  parent_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&NonBlockingPushClient::OnNotificationsDisabled,
                                parent_push_client_, reason));
}

void NonBlockingPushClient::Core::OnIncomingNotification(
    const Notification& notification) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  parent_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&NonBlockingPushClient::OnIncomingNotification,
                                parent_push_client_, notification));
}

void NonBlockingPushClient::Core::OnPingResponse() {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  parent_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&NonBlockingPushClient::OnPingResponse,
                                parent_push_client_));
}

NonBlockingPushClient::NonBlockingPushClient(
    const scoped_refptr<base::SingleThreadTaskRunner>& delegate_task_runner,
    const CreateBlockingPushClientCallback&
        create_blocking_push_client_callback)
    : delegate_task_runner_(delegate_task_runner) {
  core_ = new Core(delegate_task_runner_, weak_ptr_factory_.GetWeakPtr());
  delegate_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&NonBlockingPushClient::Core::CreateOnDelegateThread,
                     core_, create_blocking_push_client_callback));
}

NonBlockingPushClient::~NonBlockingPushClient() {
  DCHECK(thread_checker_.CalledOnValidThread());
  delegate_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&NonBlockingPushClient::Core::DestroyOnDelegateThread,
                     core_));
}

void NonBlockingPushClient::AddObserver(PushClientObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.AddObserver(observer);
}

void NonBlockingPushClient::RemoveObserver(PushClientObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.RemoveObserver(observer);
}

void NonBlockingPushClient::UpdateSubscriptions(
    const SubscriptionList& subscriptions) {
  DCHECK(thread_checker_.CalledOnValidThread());
  delegate_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&NonBlockingPushClient::Core::UpdateSubscriptions, core_,
                     subscriptions));
}

void NonBlockingPushClient::UpdateCredentials(
    const std::string& email,
    const std::string& token,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK(thread_checker_.CalledOnValidThread());
  delegate_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&NonBlockingPushClient::Core::UpdateCredentials,
                                core_, email, token, traffic_annotation));
}

void NonBlockingPushClient::SendNotification(
    const Notification& notification) {
  DCHECK(thread_checker_.CalledOnValidThread());
  delegate_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&NonBlockingPushClient::Core::SendNotification,
                                core_, notification));
}

void NonBlockingPushClient::SendPing() {
  DCHECK(thread_checker_.CalledOnValidThread());
  delegate_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&NonBlockingPushClient::Core::SendPing, core_));
}

void NonBlockingPushClient::OnNotificationsEnabled() {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (auto& observer : observers_)
    observer.OnNotificationsEnabled();
}

void NonBlockingPushClient::OnNotificationsDisabled(
    NotificationsDisabledReason reason) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (auto& observer : observers_)
    observer.OnNotificationsDisabled(reason);
}

void NonBlockingPushClient::OnIncomingNotification(
    const Notification& notification) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (auto& observer : observers_)
    observer.OnIncomingNotification(notification);
}

void NonBlockingPushClient::OnPingResponse() {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (auto& observer : observers_)
    observer.OnPingResponse();
}

}  // namespace notifier
