// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/push_notification_subscriber.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "remoting/base/logging.h"
#include "remoting/signaling/iq_sender.h"
#include "remoting/signaling/signaling_address.h"
#include "remoting/signaling/signaling_id_util.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting {

namespace {

const char kGooglePushNamespace[] = "google:push";

}  // namespace

PushNotificationSubscriber::Subscription::Subscription() = default;
PushNotificationSubscriber::Subscription::~Subscription() = default;

PushNotificationSubscriber::PushNotificationSubscriber(
    SignalStrategy* signal_strategy,
    const SubscriptionList& subscriptions)
    : signal_strategy_(signal_strategy), subscriptions_(subscriptions) {
  signal_strategy_->AddListener(this);
}

PushNotificationSubscriber::~PushNotificationSubscriber() {
  signal_strategy_->RemoveListener(this);
}

void PushNotificationSubscriber::OnSignalStrategyStateChange(
    SignalStrategy::State state) {
  if (state == SignalStrategy::CONNECTED) {
    for (const Subscription& subscription : subscriptions_) {
      Subscribe(subscription);
    }
    subscriptions_.clear();  // no longer needed
  }
}

bool PushNotificationSubscriber::OnSignalStrategyIncomingStanza(
    const jingle_xmpp::XmlElement* stanza) {
  // Ignore all XMPP stanzas.
  return false;
}

void PushNotificationSubscriber::Subscribe(const Subscription& subscription) {
  VLOG(0) << "Subscribing to push notifications on channel: "
          << subscription.channel << ".";

  std::string bare_jid;
  SplitSignalingIdResource(signal_strategy_->GetLocalAddress().id(), &bare_jid,
                           nullptr);

  // Build a subscription request.
  jingle_xmpp::XmlElement* subscribe_element =
      new jingle_xmpp::XmlElement(jingle_xmpp::QName(kGooglePushNamespace, "subscribe"));
  jingle_xmpp::XmlElement* item_element =
      new jingle_xmpp::XmlElement(jingle_xmpp::QName(kGooglePushNamespace, "item"));
  subscribe_element->AddElement(item_element);
  item_element->SetAttr(jingle_xmpp::QName(std::string(), "channel"),
                        subscription.channel);
  item_element->SetAttr(jingle_xmpp::QName(std::string(), "from"), subscription.from);

  // Send the request.
  iq_sender_.reset(new IqSender(signal_strategy_));
  iq_request_ = iq_sender_->SendIq(
      "set", bare_jid, base::WrapUnique(subscribe_element),
      base::Bind(&PushNotificationSubscriber::OnSubscriptionResult,
                 base::Unretained(this)));
}

void PushNotificationSubscriber::OnSubscriptionResult(
    IqRequest* request,
    const jingle_xmpp::XmlElement* response) {
  std::string response_type =
      response->Attr(jingle_xmpp::QName(std::string(), "type"));
  if (response_type != "result") {
    LOG(ERROR) << "Invalid response type for subscription: " << response_type;
  }

  // The IqSender and IqRequest are no longer needed after receiving a
  // reply to the subscription request.
  iq_request_.reset();
  iq_sender_.reset();
}

}  // namespace remoting
