// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/fake_session.h"

#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/fake_authenticator.h"
#include "remoting/protocol/session_plugin.h"
#include "remoting/signaling/jingle_message_xml_converter.h"

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

void FakeSession::Close(ErrorCode error,
                        std::string_view error_details,
                        const SourceLocation& error_location) {
  closed_ = true;
  error_ = error;
  event_handler_->OnSessionStateChange(CLOSED);

  base::WeakPtr<FakeSession> peer = peer_;
  if (peer) {
    peer->peer_.reset();
    peer_.reset();

    if (signaling_delay_.is_zero()) {
      peer->Close(error, error_details, error_location);
    } else {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          // Cannot just bind `error_details` as a string view, since the
          // underlying data could be invalidated before the callback is run.
          // See: crbug.com/376675478
          base::BindOnce(&FakeSession::Close, peer, error,
                         std::string(error_details), error_location),
          signaling_delay_);
    }
  }
}

void FakeSession::SendTransportInfo(
    std::unique_ptr<JingleTransportInfo> transport_info) {
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
    std::unique_ptr<JingleTransportInfo> transport_info) {
  transport_->ProcessTransportInfo(*transport_info);
}

void FakeSession::AddPlugin(SessionPlugin* plugin) {
  DCHECK(plugin);
  for (const auto& attachment : attachments_) {
    if (attachment.host_attributes || attachment.host_config) {
      plugin->OnIncomingMessage(attachment);
    }
  }
}

void FakeSession::SetAttachment(size_t round, const Attachment& attachment) {
  if (attachments_.size() <= round) {
    attachments_.resize(round + 1);
  }
  attachments_[round] = attachment;
}

}  // namespace remoting::protocol
