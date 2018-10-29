// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/listener/xmpp_push_client.h"

#include "base/logging.h"
#include "jingle/notifier/base/notifier_options_util.h"
#include "jingle/notifier/listener/push_client_observer.h"
#include "jingle/notifier/listener/send_ping_task.h"
#include "jingle/notifier/listener/push_notifications_send_update_task.h"

namespace notifier {

XmppPushClient::XmppPushClient(const NotifierOptions& notifier_options)
    : notifier_options_(notifier_options) {
  DCHECK(notifier_options_.request_context_getter->
         GetNetworkTaskRunner()->BelongsToCurrentThread());
}

XmppPushClient::~XmppPushClient() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void XmppPushClient::OnConnect(
    base::WeakPtr<buzz::XmppTaskParentInterface> base_task) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base_task_ = base_task;

  if (!base_task_.get()) {
    NOTREACHED();
    return;
  }

  // Listen for notifications.
  {
    // Owned by |base_task_|.
    PushNotificationsListenTask* listener =
        new PushNotificationsListenTask(base_task_.get(), this);
    listener->Start();
  }

  // Send subscriptions.
  {
    // Owned by |base_task_|.
    PushNotificationsSubscribeTask* subscribe_task =
        new PushNotificationsSubscribeTask(
            base_task_.get(), subscriptions_, this);
    subscribe_task->Start();
  }

  std::vector<Notification> notifications_to_send;
  notifications_to_send.swap(pending_notifications_to_send_);
  for (std::vector<Notification>::const_iterator it =
           notifications_to_send.begin();
       it != notifications_to_send.end(); ++it) {
    DVLOG(1) << "Push: Sending pending notification " << it->ToString();
    SendNotification(*it);
  }
}

void XmppPushClient::OnTransientDisconnection() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "Push: Transient disconnection";
  base_task_.reset();
  for (auto& observer : observers_)
    observer.OnNotificationsDisabled(TRANSIENT_NOTIFICATION_ERROR);
}

void XmppPushClient::OnCredentialsRejected() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "Push: Credentials rejected";
  base_task_.reset();
  for (auto& observer : observers_)
    observer.OnNotificationsDisabled(NOTIFICATION_CREDENTIALS_REJECTED);
}

void XmppPushClient::OnNotificationReceived(
    const Notification& notification) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (auto& observer : observers_)
    observer.OnIncomingNotification(notification);
}

void XmppPushClient::OnPingResponseReceived() {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (auto& observer : observers_)
    observer.OnPingResponse();
}

void XmppPushClient::OnSubscribed() {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (auto& observer : observers_)
    observer.OnNotificationsEnabled();
}

void XmppPushClient::OnSubscriptionError() {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (auto& observer : observers_)
    observer.OnNotificationsDisabled(TRANSIENT_NOTIFICATION_ERROR);
}

void XmppPushClient::AddObserver(PushClientObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.AddObserver(observer);
}

void XmppPushClient::RemoveObserver(PushClientObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.RemoveObserver(observer);
}

void XmppPushClient::UpdateSubscriptions(
    const SubscriptionList& subscriptions) {
  DCHECK(thread_checker_.CalledOnValidThread());
  subscriptions_ = subscriptions;
}

void XmppPushClient::UpdateCredentials(
    const std::string& email,
    const std::string& token,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "Push: Updating credentials for " << email;
  xmpp_settings_ = MakeXmppClientSettings(notifier_options_, email, token);
  if (login_.get()) {
    login_->UpdateXmppSettings(xmpp_settings_);
  } else {
    DVLOG(1) << "Push: Starting XMPP connection";
    base_task_.reset();
    login_.reset(new notifier::Login(
        this, xmpp_settings_, notifier_options_.request_context_getter,
        GetServerList(notifier_options_), notifier_options_.try_ssltcp_first,
        notifier_options_.auth_mechanism, traffic_annotation,
        notifier_options_.network_connection_tracker));
    login_->StartConnection();
  }
}

void XmppPushClient::SendNotification(const Notification& notification) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!base_task_.get()) {
    // TODO(akalin): Figure out whether we really need to do this.
    DVLOG(1) << "Push: Cannot send notification "
             << notification.ToString() << "; sending later";
    pending_notifications_to_send_.push_back(notification);
    return;
  }
  // Owned by |base_task_|.
  PushNotificationsSendUpdateTask* task =
      new PushNotificationsSendUpdateTask(base_task_.get(), notification);
  task->Start();
}

void XmppPushClient::SendPing() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!base_task_.get()) {
    DVLOG(1) << "Push: Cannot send ping";
    return;
  }
  // Owned by |base_task_|.
  SendPingTask* task = new SendPingTask(base_task_.get(), this);
  task->Start();
}

}  // namespace notifier
