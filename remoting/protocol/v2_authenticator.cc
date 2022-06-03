// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/v2_authenticator.h"

#include <utility>

#include "base/base64.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "remoting/base/constants.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/protocol/ssl_hmac_channel_authenticator.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

using crypto::P224EncryptedKeyExchange;

namespace remoting {
namespace protocol {

namespace {

const jingle_xmpp::StaticQName kEkeTag = { kChromotingXmlNamespace,
                                    "eke-message" };
const jingle_xmpp::StaticQName kCertificateTag = { kChromotingXmlNamespace,
                                            "certificate" };

}  // namespace

// static
bool V2Authenticator::IsEkeMessage(const jingle_xmpp::XmlElement* message) {
  return message->FirstNamed(kEkeTag) != nullptr;
}

// static
std::unique_ptr<Authenticator> V2Authenticator::CreateForClient(
    const std::string& shared_secret,
    Authenticator::State initial_state) {
  return base::WrapUnique(new V2Authenticator(
      P224EncryptedKeyExchange::kPeerTypeClient, shared_secret, initial_state));
}

// static
std::unique_ptr<Authenticator> V2Authenticator::CreateForHost(
    const std::string& local_cert,
    scoped_refptr<RsaKeyPair> key_pair,
    const std::string& shared_secret,
    Authenticator::State initial_state) {
  std::unique_ptr<V2Authenticator> result(new V2Authenticator(
      P224EncryptedKeyExchange::kPeerTypeServer, shared_secret, initial_state));
  result->local_cert_ = local_cert;
  result->local_key_pair_ = key_pair;
  return std::move(result);
}

V2Authenticator::V2Authenticator(
    crypto::P224EncryptedKeyExchange::PeerType type,
    const std::string& shared_secret,
    Authenticator::State initial_state)
    : certificate_sent_(false),
      key_exchange_impl_(type, shared_secret),
      state_(initial_state),
      started_(false),
      rejection_reason_(INVALID_CREDENTIALS) {
  pending_messages_.push(key_exchange_impl_.GetNextMessage());
}

V2Authenticator::~V2Authenticator() = default;

Authenticator::State V2Authenticator::state() const {
  if (state_ == ACCEPTED && !pending_messages_.empty())
    return MESSAGE_READY;
  return state_;
}

bool V2Authenticator::started() const {
  return started_;
}

Authenticator::RejectionReason V2Authenticator::rejection_reason() const {
  DCHECK_EQ(state(), REJECTED);
  return rejection_reason_;
}

void V2Authenticator::ProcessMessage(const jingle_xmpp::XmlElement* message,
                                     base::OnceClosure resume_callback) {
  ProcessMessageInternal(message);
  std::move(resume_callback).Run();
}

void V2Authenticator::ProcessMessageInternal(const jingle_xmpp::XmlElement* message) {
  DCHECK_EQ(state(), WAITING_MESSAGE);

  // Parse the certificate.
  std::string base64_cert = message->TextNamed(kCertificateTag);
  if (!base64_cert.empty()) {
    if (!base::Base64Decode(base64_cert, &remote_cert_)) {
      LOG(WARNING) << "Failed to decode certificate received from the peer.";
      remote_cert_.clear();
    }
  }

  // Client always expect certificate in the first message.
  if (!is_host_side() && remote_cert_.empty()) {
    LOG(WARNING) << "No valid host certificate.";
    state_ = REJECTED;
    rejection_reason_ = PROTOCOL_ERROR;
    return;
  }

  const jingle_xmpp::XmlElement* eke_element = message->FirstNamed(kEkeTag);
  if (!eke_element) {
    LOG(WARNING) << "No eke-message found.";
    state_ = REJECTED;
    rejection_reason_ = PROTOCOL_ERROR;
    return;
  }

  for (; eke_element; eke_element = eke_element->NextNamed(kEkeTag)) {
    std::string base64_message = eke_element->BodyText();
    std::string spake_message;
    if (base64_message.empty() ||
        !base::Base64Decode(base64_message, &spake_message)) {
      LOG(WARNING) << "Failed to decode auth message received from the peer.";
      state_ = REJECTED;
      rejection_reason_ = PROTOCOL_ERROR;
      return;
    }

    P224EncryptedKeyExchange::Result result =
        key_exchange_impl_.ProcessMessage(spake_message);
    started_ = true;
    switch (result) {
      case P224EncryptedKeyExchange::kResultPending:
        pending_messages_.push(key_exchange_impl_.GetNextMessage());
        break;

      case P224EncryptedKeyExchange::kResultFailed:
        state_ = REJECTED;
        rejection_reason_ = INVALID_CREDENTIALS;
        return;

      case P224EncryptedKeyExchange::kResultSuccess:
        auth_key_ = key_exchange_impl_.GetKey();
        state_ = ACCEPTED;
        return;
    }
  }
  state_ = MESSAGE_READY;
}

std::unique_ptr<jingle_xmpp::XmlElement> V2Authenticator::GetNextMessage() {
  DCHECK_EQ(state(), MESSAGE_READY);

  std::unique_ptr<jingle_xmpp::XmlElement> message = CreateEmptyAuthenticatorMessage();

  DCHECK(!pending_messages_.empty());
  while (!pending_messages_.empty()) {
    const std::string& spake_message = pending_messages_.front();
    std::string base64_message;
    base::Base64Encode(spake_message, &base64_message);

    jingle_xmpp::XmlElement* eke_tag = new jingle_xmpp::XmlElement(kEkeTag);
    eke_tag->SetBodyText(base64_message);
    message->AddElement(eke_tag);

    pending_messages_.pop();
  }

  if (!local_cert_.empty() && !certificate_sent_) {
    jingle_xmpp::XmlElement* certificate_tag = new jingle_xmpp::XmlElement(kCertificateTag);
    std::string base64_cert;
    base::Base64Encode(local_cert_, &base64_cert);
    certificate_tag->SetBodyText(base64_cert);
    message->AddElement(certificate_tag);
    certificate_sent_ = true;
  }

  if (state_ != ACCEPTED) {
    state_ = WAITING_MESSAGE;
  }
  return message;
}

const std::string& V2Authenticator::GetAuthKey() const {
  return auth_key_;
}

std::unique_ptr<ChannelAuthenticator>
V2Authenticator::CreateChannelAuthenticator() const {
  DCHECK_EQ(state(), ACCEPTED);
  CHECK(!auth_key_.empty());

  if (is_host_side()) {
    return SslHmacChannelAuthenticator::CreateForHost(
        local_cert_, local_key_pair_, auth_key_);
  } else {
    return SslHmacChannelAuthenticator::CreateForClient(
        remote_cert_, auth_key_);
  }
}

bool V2Authenticator::is_host_side() const {
  return local_key_pair_.get() != nullptr;
}

}  // namespace protocol
}  // namespace remoting
