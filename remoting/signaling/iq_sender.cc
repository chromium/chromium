// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/iq_sender.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "remoting/signaling/signal_strategy.h"
#include "remoting/signaling/signaling_id_util.h"
#include "remoting/signaling/xmpp_constants.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting {

// static
std::unique_ptr<jingle_xmpp::XmlElement> IqSender::MakeIqStanza(
    const std::string& type,
    const std::string& addressee,
    std::unique_ptr<jingle_xmpp::XmlElement> iq_body) {
  std::unique_ptr<jingle_xmpp::XmlElement> stanza(
      new jingle_xmpp::XmlElement(kQNameIq));
  stanza->AddAttr(kQNameType, type);
  if (!addressee.empty()) {
    stanza->AddAttr(kQNameTo, addressee);
  }
  stanza->AddElement(iq_body.release());
  return stanza;
}

IqSender::IqSender(SignalStrategy* signal_strategy)
    : signal_strategy_(signal_strategy) {
  signal_strategy_->AddListener(this);
}

IqSender::~IqSender() {
  signal_strategy_->RemoveListener(this);
}

std::unique_ptr<IqRequest> IqSender::SendIq(
    std::unique_ptr<jingle_xmpp::XmlElement> stanza,
    ReplyCallback callback) {
  std::string addressee = stanza->Attr(kQNameTo);
  std::string id = stanza->Attr(kQNameId);
  if (id.empty()) {
    id = signal_strategy_->GetNextId();
    stanza->AddAttr(kQNameId, id);
  }
  if (!signal_strategy_->SendStanza(std::move(stanza))) {
    return nullptr;
  }
  DCHECK(requests_.find(id) == requests_.end());
  bool callback_exists = !callback.is_null();
  auto request =
      std::make_unique<IqRequest>(this, std::move(callback), addressee);
  if (callback_exists) {
    requests_[id] = request.get();
  }
  return request;
}

std::unique_ptr<IqRequest> IqSender::SendIq(
    const std::string& type,
    const std::string& addressee,
    std::unique_ptr<jingle_xmpp::XmlElement> iq_body,
    ReplyCallback callback) {
  return SendIq(MakeIqStanza(type, addressee, std::move(iq_body)),
                std::move(callback));
}

void IqSender::RemoveRequest(IqRequest* request) {
  auto it = requests_.begin();
  while (it != requests_.end()) {
    auto cur = it;
    ++it;
    if (cur->second == request) {
      requests_.erase(cur);
      break;
    }
  }
}

void IqSender::OnSignalStrategyStateChange(SignalStrategy::State state) {}

bool IqSender::OnSignalStrategyIncomingStanza(
    const jingle_xmpp::XmlElement* stanza) {
  if (stanza->Name() != kQNameIq) {
    LOG(WARNING) << "Received unexpected non-IQ packet " << stanza->Str();
    return false;
  }

  const std::string& type = stanza->Attr(kQNameType);
  if (type.empty()) {
    LOG(WARNING) << "IQ packet missing type " << stanza->Str();
    return false;
  }

  if (type != "result" && type != "error") {
    return false;
  }

  const std::string& id = stanza->Attr(kQNameId);
  if (id.empty()) {
    LOG(WARNING) << "IQ packet missing id " << stanza->Str();
    return false;
  }

  std::string from = stanza->Attr(kQNameFrom);

  auto it = requests_.find(id);
  if (it == requests_.end()) {
    return false;
  }

  IqRequest* request = it->second;

  if (NormalizeSignalingId(request->addressee_) != NormalizeSignalingId(from)) {
    LOG(ERROR) << "Received IQ response from an invalid JID. Ignoring it."
               << " Message received from: " << from
               << " Original JID: " << request->addressee_;
    return false;
  }

  requests_.erase(it);
  request->OnResponse(stanza);

  return true;
}

IqRequest::IqRequest(IqSender* sender,
                     IqSender::ReplyCallback callback,
                     const std::string& addressee)
    : sender_(sender), callback_(std::move(callback)), addressee_(addressee) {}

IqRequest::~IqRequest() {
  sender_->RemoveRequest(this);
}

void IqRequest::SetTimeout(base::TimeDelta timeout) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&IqRequest::OnTimeout, weak_factory_.GetWeakPtr()),
      timeout);
}

void IqRequest::CallCallback(const jingle_xmpp::XmlElement* stanza) {
  if (!callback_.is_null()) {
    std::move(callback_).Run(this, stanza);
  }
}

void IqRequest::OnTimeout() {
  CallCallback(nullptr);
}

void IqRequest::OnResponse(const jingle_xmpp::XmlElement* stanza) {
  // It's unsafe to delete signal strategy here, and the callback may
  // want to do that, so we post task to invoke the callback later.
  std::unique_ptr<jingle_xmpp::XmlElement> stanza_copy(
      new jingle_xmpp::XmlElement(*stanza));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&IqRequest::DeliverResponse, weak_factory_.GetWeakPtr(),
                     std::move(stanza_copy)));
}

void IqRequest::DeliverResponse(
    std::unique_ptr<jingle_xmpp::XmlElement> stanza) {
  CallCallback(stanza.get());
}

}  // namespace remoting
