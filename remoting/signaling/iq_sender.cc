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
#include "remoting/signaling/jingle_data_structures.h"
#include "remoting/signaling/signal_strategy.h"
#include "remoting/signaling/signaling_id_util.h"

namespace remoting {

IqSender::IqSender(SignalStrategy* signal_strategy)
    : signal_strategy_(signal_strategy) {
  signal_strategy_->AddListener(this);
}

IqSender::~IqSender() {
  signal_strategy_->RemoveListener(this);
}

std::unique_ptr<IqRequest> IqSender::SendIq(JingleMessage&& message,
                                            ReplyCallback callback) {
  if (message.message_id.empty()) {
    message.message_id = signal_strategy_->GetNextId();
  }
  std::string id = message.message_id;
  std::string to = message.to.id();

  if (!signal_strategy_->SendMessage(SignalingMessage(std::move(message)))) {
    return nullptr;
  }

  DCHECK(requests_.find(id) == requests_.end());
  bool callback_exists = !callback.is_null();
  auto request = std::make_unique<IqRequest>(this, std::move(callback), to);
  if (callback_exists) {
    requests_[id] = request.get();
  }
  return request;
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

void IqSender::OnSignalingStateChanged(SignalStrategy::State state) {}

bool IqSender::OnSignalingMessage(const SignalingAddress& sender_address,
                                  const SignalingMessage& message) {
  // Currently JingleMessageFromXml only returns JingleMessage for 'set' IQs.
  // IQ results and errors are parsed into JingleMessageReply by the signal
  // strategy and handled above. If this changes in the future, we might need
  // to handle JingleMessage responses here.
  const auto* jingle_reply = std::get_if<JingleMessageReply>(&message);
  if (!jingle_reply) {
    return false;
  }

  auto it = requests_.find(jingle_reply->message_id);
  if (it == requests_.end()) {
    return false;
  }

  IqRequest* request = it->second;

  if (NormalizeSignalingId(request->addressee_) !=
      NormalizeSignalingId(jingle_reply->from.id())) {
    LOG(ERROR) << "Received IQ response from an invalid JID. Ignoring it."
               << " Message received from: " << jingle_reply->from.id()
               << " Original JID: " << request->addressee_;
    return false;
  }

  requests_.erase(it);
  request->OnResponse(*jingle_reply);

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

void IqRequest::CallCallback(const JingleMessageReply& reply) {
  if (!callback_.is_null()) {
    std::move(callback_).Run(this, reply);
  }
}

void IqRequest::OnTimeout() {
  JingleMessageReply reply;
  reply.reply_type = JingleMessageReply::REPLY_ERROR;
  reply.error_type = JingleMessageReply::UNEXPECTED_REQUEST;
  reply.text = "timeout";
  CallCallback(reply);
}

void IqRequest::OnResponse(const JingleMessageReply& reply) {
  // It's unsafe to delete signal strategy here, and the callback may
  // want to do that, so we post task to invoke the callback later.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&IqRequest::DeliverResponse,
                                weak_factory_.GetWeakPtr(), reply));
}

void IqRequest::DeliverResponse(const JingleMessageReply& reply) {
  CallCallback(reply);
}

}  // namespace remoting
