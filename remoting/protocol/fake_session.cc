// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/fake_session.h"

#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/fake_authenticator.h"
#include "remoting/protocol/session_plugin.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting::protocol {

const char kTestJid[] = "host1@gmail.com/chromoting123";
const char kTestAuthKey[] = "test_auth_key";

FakeSession::FakeSession()
    : config_(SessionConfig::ForTest()), jid_(kTestJid) {}
FakeSession::~FakeSession() = default;

void FakeSession::SimulateConnection(FakeSession* peer) {
  peer_ = peer->weak_factory_.GetWeakPtr();
  peer->peer_ = weak_factory_.GetWeakPtr();

  event_handler_->OnSessionStateChange(CONNECTING);
  peer->event_handler_->OnSessionStateChange(ACCEPTING);
  peer->event_handler_->OnSessionStateChange(ACCEPTED);
  event_handler_->OnSessionStateChange(ACCEPTED);
  event_handler_->OnSessionStateChange(AUTHENTICATING);
  peer->event_handler_->OnSessionStateChange(AUTHENTICATING);

  // Initialize transport and authenticator on the client.
  authenticator_ =
      std::make_unique<FakeAuthenticator>(FakeAuthenticator::ACCEPT);
  authenticator_->set_auth_key(kTestAuthKey);
  transport_->Start(authenticator_.get(),
                    base::BindRepeating(&FakeSession::SendTransportInfo,
                                        weak_factory_.GetWeakPtr()));

  // Initialize transport and authenticator on the host.
  peer->authenticator_ =
      std::make_unique<FakeAuthenticator>(FakeAuthenticator::ACCEPT);
  peer->authenticator_->set_auth_key(kTestAuthKey);
  peer->transport_->Start(
      peer->authenticator_.get(),
      base::BindRepeating(&FakeSession::SendTransportInfo, peer_));

  peer->event_handler_->OnSessionStateChange(AUTHENTICATED);
  event_handler_->OnSessionStateChange(AUTHENTICATED);
}

void FakeSession::SetEventHandler(EventHandler* event_handler) {
  event_handler_ = event_handler;
}

ErrorCode FakeSession::error() const {
  return error_;
}

const std::string& FakeSession::jid() {
  return jid_;
}

const SessionConfig& FakeSession::config() {
  return *config_;
}

const Authenticator& FakeSession::authenticator() const {
  return *authenticator_;
}

void FakeSession::SetTransport(Transport* transport) {
  transport_ = transport;
}

void FakeSession::Close(ErrorCode error) {
  closed_ = true;
  error_ = error;
  event_handler_->OnSessionStateChange(CLOSED);

  base::WeakPtr<FakeSession> peer = peer_;
  if (peer) {
    peer->peer_.reset();
    peer_.reset();

    if (signaling_delay_.is_zero()) {
      peer->Close(error);
    } else {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, base::BindOnce(&FakeSession::Close, peer, error),
          signaling_delay_);
    }
  }
}

void FakeSession::SendTransportInfo(
    std::unique_ptr<jingle_xmpp::XmlElement> transport_info) {
  if (!peer_) {
    return;
  }

  if (signaling_delay_.is_zero()) {
    peer_->ProcessTransportInfo(std::move(transport_info));
  } else {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FakeSession::ProcessTransportInfo, peer_,
                       std::move(transport_info)),
        signaling_delay_);
  }
}

void FakeSession::ProcessTransportInfo(
    std::unique_ptr<jingle_xmpp::XmlElement> transport_info) {
  transport_->ProcessTransportInfo(transport_info.get());
}

void FakeSession::AddPlugin(SessionPlugin* plugin) {
  DCHECK(plugin);
  for (const auto& message : attachments_) {
    if (message) {
      JingleMessage jingle_message;
      jingle_message.AddAttachment(
          std::make_unique<jingle_xmpp::XmlElement>(*message));
      plugin->OnIncomingMessage(*(jingle_message.attachments));
    }
  }
}

void FakeSession::SetAttachment(
    size_t round,
    std::unique_ptr<jingle_xmpp::XmlElement> attachment) {
  while (attachments_.size() <= round) {
    attachments_.emplace_back();
  }
  attachments_[round] = std::move(attachment);
}

}  // namespace remoting::protocol
