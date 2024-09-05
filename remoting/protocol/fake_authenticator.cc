// const  Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/fake_authenticator.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/p2p_stream_socket.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting::protocol {

FakeChannelAuthenticator::FakeChannelAuthenticator(bool accept, bool async)
    : result_(accept ? net::OK : net::ERR_FAILED), async_(async) {}

FakeChannelAuthenticator::~FakeChannelAuthenticator() = default;

void FakeChannelAuthenticator::SecureAndAuthenticate(
    std::unique_ptr<P2PStreamSocket> socket,
    DoneCallback done_callback) {
  socket_ = std::move(socket);

  done_callback_ = std::move(done_callback);

  if (async_) {
    if (result_ != net::OK) {
      // Don't write anything if we are going to reject auth to make test
      // ordering deterministic.
      did_write_bytes_ = true;
    } else {
      auto write_buf = base::MakeRefCounted<net::IOBufferWithSize>(1);
      write_buf->data()[0] = 0;
      int result = socket_->Write(
          write_buf.get(), 1,
          base::BindOnce(&FakeChannelAuthenticator::OnAuthBytesWritten,
                         weak_factory_.GetWeakPtr()),
          TRAFFIC_ANNOTATION_FOR_TESTS);
      if (result != net::ERR_IO_PENDING) {
        // This will not call the callback because |did_read_bytes_| is
        // still set to false.
        OnAuthBytesWritten(result);
      }
    }

    auto read_buf = base::MakeRefCounted<net::IOBufferWithSize>(1);
    int result =
        socket_->Read(read_buf.get(), 1,
                      base::BindOnce(&FakeChannelAuthenticator::OnAuthBytesRead,
                                     weak_factory_.GetWeakPtr()));
    if (result != net::ERR_IO_PENDING) {
      OnAuthBytesRead(result);
    }
  } else {
    CallDoneCallback();
  }
}

void FakeChannelAuthenticator::OnAuthBytesWritten(int result) {
  EXPECT_EQ(1, result);
  EXPECT_FALSE(did_write_bytes_);
  did_write_bytes_ = true;
  if (did_read_bytes_) {
    CallDoneCallback();
  }
}

void FakeChannelAuthenticator::OnAuthBytesRead(int result) {
  EXPECT_EQ(1, result);
  EXPECT_FALSE(did_read_bytes_);
  did_read_bytes_ = true;
  if (did_write_bytes_) {
    CallDoneCallback();
  }
}

void FakeChannelAuthenticator::CallDoneCallback() {
  if (result_ != net::OK) {
    socket_.reset();
  }
  std::move(done_callback_).Run(result_, std::move(socket_));
}

FakeAuthenticator::Config::Config() = default;
FakeAuthenticator::Config::Config(Action action) : action(action) {}
FakeAuthenticator::Config::Config(int round_trips, Action action, bool async)
    : round_trips(round_trips), action(action), async(async) {}

FakeAuthenticator::FakeAuthenticator(Type type,
                                     FakeAuthenticator::Config config,
                                     const std::string& local_id,
                                     const std::string& remote_id)
    : type_(type), config_(config), local_id_(local_id), remote_id_(remote_id) {
  EXPECT_TRUE((!local_id_.empty() && !remote_id_.empty()) ||
              config.round_trips == 0);
}

FakeAuthenticator::FakeAuthenticator(Action action)
    : FakeAuthenticator(CLIENT,
                        FakeAuthenticator::Config(0, action, true),
                        std::string(),
                        std::string()) {}

FakeAuthenticator::~FakeAuthenticator() = default;

void FakeAuthenticator::set_messages_till_started(int messages) {
  messages_till_started_ = messages;
}

void FakeAuthenticator::Resume() {
  std::move(resume_closure_).Run();
}

CredentialsType FakeAuthenticator::credentials_type() const {
  return config_.credentials_type;
}

const Authenticator& FakeAuthenticator::implementing_authenticator() const {
  return *this;
}

Authenticator::State FakeAuthenticator::state() const {
  EXPECT_LE(messages_, config_.round_trips * 2);

  if (messages_ == pause_message_index_ && !resume_closure_.is_null()) {
    return PROCESSING_MESSAGE;
  }

  if (messages_ >= config_.round_trips * 2) {
    if (config_.action == REJECT) {
      return REJECTED;
    } else {
      return ACCEPTED;
    }
  }

  // Don't send the last message if this is a host that wants to
  // reject a connection.
  if (messages_ == config_.round_trips * 2 - 1 && type_ == HOST &&
      config_.action == REJECT) {
    return REJECTED;
  }

  // We are not done yet. process next message.
  if ((messages_ % 2 == 0 && type_ == CLIENT) ||
      (messages_ % 2 == 1 && type_ == HOST)) {
    return MESSAGE_READY;
  } else {
    return WAITING_MESSAGE;
  }
}

bool FakeAuthenticator::started() const {
  return messages_ > messages_till_started_;
}

Authenticator::RejectionReason FakeAuthenticator::rejection_reason() const {
  EXPECT_EQ(REJECTED, state());
  return RejectionReason::INVALID_CREDENTIALS;
}

void FakeAuthenticator::ProcessMessage(const jingle_xmpp::XmlElement* message,
                                       base::OnceClosure resume_callback) {
  EXPECT_EQ(WAITING_MESSAGE, state());
  std::string id =
      message->TextNamed(jingle_xmpp::QName(kChromotingXmlNamespace, "id"));
  EXPECT_EQ(id, base::NumberToString(messages_));

  // On the client receive the key in the last message.
  if (type_ == CLIENT && messages_ == config_.round_trips * 2 - 1) {
    std::string key_base64 =
        message->TextNamed(jingle_xmpp::QName(kChromotingXmlNamespace, "key"));
    EXPECT_TRUE(!key_base64.empty());
    EXPECT_TRUE(base::Base64Decode(key_base64, &auth_key_));
  }

  // Receive peer's id.
  if (messages_ < 2) {
    EXPECT_EQ(remote_id_, message->Attr(jingle_xmpp::QName("", "id")));
  }

  ++messages_;
  SubscribeRejectedAfterAcceptedIfNecessary();
  if (messages_ == pause_message_index_) {
    resume_closure_ = std::move(resume_callback);
    return;
  }
  std::move(resume_callback).Run();
}

std::unique_ptr<jingle_xmpp::XmlElement> FakeAuthenticator::GetNextMessage() {
  EXPECT_EQ(MESSAGE_READY, state());

  std::unique_ptr<jingle_xmpp::XmlElement> result(new jingle_xmpp::XmlElement(
      jingle_xmpp::QName(kChromotingXmlNamespace, "authentication")));
  jingle_xmpp::XmlElement* id = new jingle_xmpp::XmlElement(
      jingle_xmpp::QName(kChromotingXmlNamespace, "id"));
  id->AddText(base::NumberToString(messages_));
  result->AddElement(id);

  // Send local id in the first outgoing message.
  if (messages_ < 2) {
    result->AddAttr(jingle_xmpp::QName("", "id"), local_id_);
  }

  // Add authentication key in the last message sent from host to client.
  if (type_ == HOST && messages_ == config_.round_trips * 2 - 1) {
    auth_key_ = base::RandBytesAsString(16);
    jingle_xmpp::XmlElement* key = new jingle_xmpp::XmlElement(
        jingle_xmpp::QName(kChromotingXmlNamespace, "key"));
    std::string key_base64 = base::Base64Encode(auth_key_);
    key->AddText(key_base64);
    result->AddElement(key);
  }

  ++messages_;
  SubscribeRejectedAfterAcceptedIfNecessary();
  return result;
}

const std::string& FakeAuthenticator::GetAuthKey() const {
  EXPECT_EQ(ACCEPTED, state());
  DCHECK(!auth_key_.empty());
  return auth_key_;
}

const SessionPolicies* FakeAuthenticator::GetSessionPolicies() const {
  EXPECT_EQ(ACCEPTED, state());
  return nullptr;
}

std::unique_ptr<ChannelAuthenticator>
FakeAuthenticator::CreateChannelAuthenticator() const {
  EXPECT_EQ(ACCEPTED, state());
  return std::make_unique<FakeChannelAuthenticator>(
      config_.action != REJECT_CHANNEL, config_.async);
}

void FakeAuthenticator::SubscribeRejectedAfterAcceptedIfNecessary() {
  if (state() == ACCEPTED && config_.reject_after_accepted) {
    reject_after_accepted_subscription_ =
        config_.reject_after_accepted->Add(base::BindRepeating(
            [](FakeAuthenticator* self) {
              self->config_.action = REJECT;
              self->NotifyStateChangeAfterAccepted();
            },
            base::Unretained(this)));
  }
}

FakeHostAuthenticatorFactory::FakeHostAuthenticatorFactory(
    int messages_till_started,
    FakeAuthenticator::Config config)
    : messages_till_started_(messages_till_started), config_(config) {}
FakeHostAuthenticatorFactory::~FakeHostAuthenticatorFactory() = default;

std::unique_ptr<Authenticator>
FakeHostAuthenticatorFactory::CreateAuthenticator(
    const std::string& local_jid,
    const std::string& remote_jid) {
  std::unique_ptr<FakeAuthenticator> authenticator(new FakeAuthenticator(
      FakeAuthenticator::HOST, config_, local_jid, remote_jid));
  authenticator->set_messages_till_started(messages_till_started_);
  return std::move(authenticator);
}

}  // namespace remoting::protocol
