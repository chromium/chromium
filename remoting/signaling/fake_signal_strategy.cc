// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/fake_signal_strategy.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notimplemented.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/signaling/jingle_message_xml_converter.h"
#include "remoting/signaling/signaling_id_util.h"
#include "remoting/signaling/xmpp_constants.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting {

// static
void FakeSignalStrategy::Connect(FakeSignalStrategy* peer1,
                                 FakeSignalStrategy* peer2) {
  DCHECK(peer1->main_thread_->BelongsToCurrentThread());
  DCHECK(peer2->main_thread_->BelongsToCurrentThread());
  peer1->ConnectTo(peer2);
  peer2->ConnectTo(peer1);
}

FakeSignalStrategy::FakeSignalStrategy(const SignalingAddress& address)
    : main_thread_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      address_(address),
      last_id_(0) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

FakeSignalStrategy::~FakeSignalStrategy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void FakeSignalStrategy::SetError(Error error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  error_ = error;
}

void FakeSignalStrategy::SetIsSignInError(bool is_sign_in_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_sign_in_error_ = is_sign_in_error;
}

void FakeSignalStrategy::SetState(State state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state == state_) {
    return;
  }
  state_ = state;
  for (auto& observer : listeners_) {
    observer.OnSignalStrategyStateChange(state_);
  }
}

void FakeSignalStrategy::SetPeerCallback(const PeerCallback& peer_callback) {
  peer_callback_ = peer_callback;
}

void FakeSignalStrategy::ConnectTo(FakeSignalStrategy* peer) {
  PeerCallback peer_callback =
      base::BindRepeating(&FakeSignalStrategy::DeliverMessageOnThread,
                          main_thread_, weak_factory_.GetWeakPtr());
  if (peer->main_thread_->BelongsToCurrentThread()) {
    peer->SetPeerCallback(std::move(peer_callback));
  } else {
    peer->main_thread_->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeSignalStrategy::SetPeerCallback,
                       base::Unretained(peer), std::move(peer_callback)));
  }
}

void FakeSignalStrategy::SetLocalAddress(const SignalingAddress& address) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  address_ = address;
}

void FakeSignalStrategy::SimulateMessageReordering() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  simulate_reorder_ = true;
}

void FakeSignalStrategy::SimulateTwoStageConnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  simulate_two_stage_connect_ = true;
}

void FakeSignalStrategy::OnIncomingMessage(SignalingMessage message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!simulate_reorder_) {
    NotifyListeners(std::move(message));
    return;
  }

  // Simulate IQ messages re-ordering by swapping the delivery order of
  // next pair of messages.
  if (pending_message_) {
    NotifyListeners(std::move(message));
    NotifyListeners(std::move(*pending_message_));
    pending_message_.reset();
  } else {
    pending_message_ = std::move(message);
  }
}

void FakeSignalStrategy::ProceedConnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetState(CONNECTED);
}

void FakeSignalStrategy::Connect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetState(simulate_two_stage_connect_ ? CONNECTING : CONNECTED);
}

void FakeSignalStrategy::Disconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetState(DISCONNECTED);
}

SignalStrategy::State FakeSignalStrategy::GetState() const {
  return state_;
}

SignalStrategy::Error FakeSignalStrategy::GetError() const {
  return error_;
}

const SignalingAddress& FakeSignalStrategy::GetLocalAddress() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return address_;
}

void FakeSignalStrategy::AddListener(Listener* listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  listeners_.AddObserver(listener);
}

void FakeSignalStrategy::RemoveListener(Listener* listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  listeners_.RemoveObserver(listener);
}

bool FakeSignalStrategy::SendMessage(
    const SignalingAddress& destination_address,
    SignalingMessage&& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (auto* jingle_message = std::get_if<JingleMessage>(&message)) {
    jingle_message->from = address_;
  }

  if (auto* jingle_reply = std::get_if<JingleMessageReply>(&message)) {
    jingle_reply->from = address_;
  }

  if (peer_callback_.is_null()) {
    return false;
  }

  if (send_delay_.is_zero()) {
    peer_callback_.Run(std::move(message));
  } else {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(peer_callback_, std::move(message)),
        send_delay_);
  }
  return true;
}

std::string FakeSignalStrategy::GetNextId() {
  ++last_id_;
  return base::NumberToString(last_id_);
}

bool FakeSignalStrategy::IsSignInError() const {
  return is_sign_in_error_;
}

// static
void FakeSignalStrategy::DeliverMessageOnThread(
    scoped_refptr<base::SingleThreadTaskRunner> thread,
    base::WeakPtr<FakeSignalStrategy> target,
    SignalingMessage message) {
  thread->PostTask(
      FROM_HERE, base::BindOnce(&FakeSignalStrategy::OnIncomingMessage, target,
                                std::move(message)));
}

void FakeSignalStrategy::NotifyListeners(SignalingMessage message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SignalingMessage message_to_dispatch = std::move(message);
  SignalingAddress from;
  SignalingAddress to;

  std::optional<std::string> xml;
  if (const auto* ftl =
          std::get_if<ftl::ChromotingMessage>(&message_to_dispatch)) {
    if (ftl->has_xmpp() && ftl->xmpp().has_stanza()) {
      xml = ftl->xmpp().stanza();
    }
  } else if (const auto* corp = std::get_if<internal::PeerMessageStruct>(
                 &message_to_dispatch)) {
    if (const auto* iq =
            std::get_if<internal::IqStanzaStruct>(&corp->payload)) {
      xml = iq->xml;
    }
  }

  if (xml) {
    auto parsed_message = SignalStrategy::ParseStanzaXml(*xml);
    if (parsed_message) {
      message_to_dispatch = std::move(*parsed_message);
    }
  }

  if (const auto* jm = std::get_if<JingleMessage>(&message_to_dispatch)) {
    from = jm->from;
    to = jm->to;
  } else if (const auto* jmr =
                 std::get_if<JingleMessageReply>(&message_to_dispatch)) {
    from = jmr->from;
    to = jmr->to;
  }

  if (!to.empty() && to != address_) {
    LOG(WARNING) << "Dropping stanza that is addressed to " << to.id()
                 << ". Local address: " << address_.id();
    return;
  }

  received_messages_.push_back(message_to_dispatch);

  for (auto& listener : listeners_) {
    if (listener.OnSignalStrategyIncomingMessage(from, message_to_dispatch)) {
      break;
    }
  }
}

}  // namespace remoting
