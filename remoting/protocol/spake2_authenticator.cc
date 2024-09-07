// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/spake2_authenticator.h"

#include <utility>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/byte_conversions.h"
#include "crypto/hmac.h"
#include "crypto/secure_util.h"
#include "remoting/base/constants.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/protocol/ssl_hmac_channel_authenticator.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting::protocol {

namespace {

// Each peer sends 2 messages: <spake-message> and <verification-hash>. The
// content of <spake-message> is the output of SPAKE2_generate_msg() and must
// be passed to SPAKE2_process_msg() on the other end. This is enough to
// generate authentication key. <verification-hash> is sent to confirm that both
// ends get the same authentication key (which means they both know the
// password). This verification hash is calculated in
// CalculateVerificationHash() as follows:
//    HMAC_SHA256(auth_key, ("host"|"client") + local_jid.length() + local_jid +
//                remote_jid.length() + remote_jid)
// where auth_key is the key produced by SPAKE2.

const jingle_xmpp::StaticQName kSpakeMessageTag = {kChromotingXmlNamespace,
                                                   "spake-message"};
const jingle_xmpp::StaticQName kVerificationHashTag = {kChromotingXmlNamespace,
                                                       "verification-hash"};
const jingle_xmpp::StaticQName kCertificateTag = {kChromotingXmlNamespace,
                                                  "certificate"};

std::unique_ptr<jingle_xmpp::XmlElement> EncodeBinaryValueToXml(
    const jingle_xmpp::StaticQName& qname,
    const std::string& content) {
  std::string content_base64 = base::Base64Encode(content);

  std::unique_ptr<jingle_xmpp::XmlElement> result(
      new jingle_xmpp::XmlElement(qname));
  result->SetBodyText(content_base64);
  return result;
}

// Finds tag named |qname| in base_message and decodes it from base64 and stores
// in |data|. If the element is not present then found is set to false otherwise
// it's set to true. If the element is there and it's content couldn't be
// decoded then false is returned.
bool DecodeBinaryValueFromXml(const jingle_xmpp::XmlElement* message,
                              const jingle_xmpp::QName& qname,
                              bool* found,
                              std::string* data) {
  const jingle_xmpp::XmlElement* element = message->FirstNamed(qname);
  *found = element != nullptr;
  if (!*found) {
    return true;
  }

  if (!base::Base64Decode(element->BodyText(), data)) {
    LOG(WARNING) << "Failed to parse " << qname.LocalPart();
    return false;
  }

  return !data->empty();
}

std::string PrefixWithLength(const std::string& str) {
  std::string out;
  out += base::as_string_view(base::numerics::U32ToBigEndian(str.size()));
  out += str;
  return out;
}

}  // namespace

// static
std::unique_ptr<Authenticator> Spake2Authenticator::CreateForClient(
    const std::string& local_id,
    const std::string& remote_id,
    const std::string& shared_secret,
    Authenticator::State initial_state) {
  return base::WrapUnique(new Spake2Authenticator(
      local_id, remote_id, shared_secret, false, initial_state));
}

// static
std::unique_ptr<Authenticator> Spake2Authenticator::CreateForHost(
    const std::string& local_id,
    const std::string& remote_id,
    const std::string& local_cert,
    scoped_refptr<RsaKeyPair> key_pair,
    const std::string& shared_secret,
    Authenticator::State initial_state) {
  std::unique_ptr<Spake2Authenticator> result(new Spake2Authenticator(
      local_id, remote_id, shared_secret, true, initial_state));
  result->local_cert_ = local_cert;
  result->local_key_pair_ = key_pair;
  return std::move(result);
}

Spake2Authenticator::Spake2Authenticator(const std::string& local_id,
                                         const std::string& remote_id,
                                         const std::string& shared_secret,
                                         bool is_host,
                                         Authenticator::State initial_state)
    : local_id_(local_id),
      remote_id_(remote_id),
      shared_secret_(shared_secret),
      is_host_(is_host),
      state_(initial_state) {
  spake2_context_ = SPAKE2_CTX_new(
      is_host ? spake2_role_bob : spake2_role_alice,
      reinterpret_cast<const uint8_t*>(local_id_.data()), local_id_.size(),
      reinterpret_cast<const uint8_t*>(remote_id_.data()), remote_id_.size());

  // Generate first message and push it to |pending_messages_|.
  uint8_t message[SPAKE2_MAX_MSG_SIZE];
  size_t message_size;
  int result = SPAKE2_generate_msg(
      spake2_context_, message, &message_size, sizeof(message),
      reinterpret_cast<const uint8_t*>(shared_secret_.data()),
      shared_secret_.size());
  CHECK(result);
  local_spake_message_.assign(reinterpret_cast<char*>(message), message_size);
}

Spake2Authenticator::~Spake2Authenticator() {
  SPAKE2_CTX_free(spake2_context_);
}

CredentialsType Spake2Authenticator::credentials_type() const {
  return CredentialsType::SHARED_SECRET;
}

const Authenticator& Spake2Authenticator::implementing_authenticator() const {
  return *this;
}

Authenticator::State Spake2Authenticator::state() const {
  if (state_ == ACCEPTED && !outgoing_verification_hash_.empty()) {
    return MESSAGE_READY;
  }
  return state_;
}

bool Spake2Authenticator::started() const {
  return started_;
}

Authenticator::RejectionReason Spake2Authenticator::rejection_reason() const {
  DCHECK_EQ(state(), REJECTED);
  return rejection_reason_;
}

void Spake2Authenticator::ProcessMessage(const jingle_xmpp::XmlElement* message,
                                         base::OnceClosure resume_callback) {
  ProcessMessageInternal(message);
  std::move(resume_callback).Run();
}

void Spake2Authenticator::ProcessMessageInternal(
    const jingle_xmpp::XmlElement* message) {
  DCHECK_EQ(state(), WAITING_MESSAGE);

  // Parse the certificate.
  bool cert_present;
  if (!DecodeBinaryValueFromXml(message, kCertificateTag, &cert_present,
                                &remote_cert_)) {
    state_ = REJECTED;
    rejection_reason_ = RejectionReason::PROTOCOL_ERROR;
    return;
  }

  // Client always expects certificate in the first message.
  if (!is_host_ && remote_cert_.empty()) {
    LOG(WARNING) << "No valid host certificate.";
    state_ = REJECTED;
    rejection_reason_ = RejectionReason::PROTOCOL_ERROR;
    return;
  }

  bool spake_message_present = false;
  std::string spake_message;
  bool verification_hash_present = false;
  std::string verification_hash;
  if (!DecodeBinaryValueFromXml(message, kSpakeMessageTag,
                                &spake_message_present, &spake_message) ||
      !DecodeBinaryValueFromXml(message, kVerificationHashTag,
                                &verification_hash_present,
                                &verification_hash)) {
    state_ = REJECTED;
    rejection_reason_ = RejectionReason::PROTOCOL_ERROR;
    return;
  }

  // |auth_key_| is generated when <spake-message> is received.
  if (auth_key_.empty()) {
    if (!spake_message_present) {
      LOG(WARNING) << "<spake-message> not found.";
      state_ = REJECTED;
      rejection_reason_ = RejectionReason::PROTOCOL_ERROR;
      return;
    }
    uint8_t key[SPAKE2_MAX_KEY_SIZE];
    size_t key_size;
    started_ = true;
    int result = SPAKE2_process_msg(
        spake2_context_, key, &key_size, sizeof(key),
        reinterpret_cast<const uint8_t*>(spake_message.data()),
        spake_message.size());
    if (!result) {
      state_ = REJECTED;
      rejection_reason_ = RejectionReason::INVALID_CREDENTIALS;
      return;
    }
    CHECK(key_size);
    auth_key_.assign(reinterpret_cast<char*>(key), key_size);

    outgoing_verification_hash_ =
        CalculateVerificationHash(is_host_, local_id_, remote_id_);
    expected_verification_hash_ =
        CalculateVerificationHash(!is_host_, remote_id_, local_id_);
  } else if (spake_message_present) {
    LOG(WARNING) << "Received duplicate <spake-message>.";
    state_ = REJECTED;
    rejection_reason_ = RejectionReason::PROTOCOL_ERROR;
    return;
  }

  if (spake_message_sent_ && !verification_hash_present) {
    LOG(WARNING) << "Didn't receive <verification-hash> when expected.";
    state_ = REJECTED;
    rejection_reason_ = RejectionReason::PROTOCOL_ERROR;
    return;
  }

  if (verification_hash_present) {
    if (verification_hash.size() != expected_verification_hash_.size() ||
        !crypto::SecureMemEqual(verification_hash.data(),
                                expected_verification_hash_.data(),
                                verification_hash.size())) {
      state_ = REJECTED;
      rejection_reason_ = RejectionReason::INVALID_CREDENTIALS;
      return;
    }
    state_ = ACCEPTED;
    return;
  }

  state_ = MESSAGE_READY;
}

std::unique_ptr<jingle_xmpp::XmlElement> Spake2Authenticator::GetNextMessage() {
  DCHECK_EQ(state(), MESSAGE_READY);

  std::unique_ptr<jingle_xmpp::XmlElement> message =
      CreateEmptyAuthenticatorMessage();

  if (!spake_message_sent_) {
    if (!local_cert_.empty()) {
      message->AddElement(
          EncodeBinaryValueToXml(kCertificateTag, local_cert_).release());
    }

    message->AddElement(
        EncodeBinaryValueToXml(kSpakeMessageTag, local_spake_message_)
            .release());

    spake_message_sent_ = true;
  }

  if (!outgoing_verification_hash_.empty()) {
    message->AddElement(EncodeBinaryValueToXml(kVerificationHashTag,
                                               outgoing_verification_hash_)
                            .release());
    outgoing_verification_hash_.clear();
  }

  if (state_ != ACCEPTED) {
    state_ = WAITING_MESSAGE;
  }
  return message;
}

const std::string& Spake2Authenticator::GetAuthKey() const {
  return auth_key_;
}

const SessionPolicies* Spake2Authenticator::GetSessionPolicies() const {
  return nullptr;
}

std::unique_ptr<ChannelAuthenticator>
Spake2Authenticator::CreateChannelAuthenticator() const {
  DCHECK_EQ(state(), ACCEPTED);
  CHECK(!auth_key_.empty());

  if (is_host_) {
    return SslHmacChannelAuthenticator::CreateForHost(
        local_cert_, local_key_pair_, auth_key_);
  } else {
    return SslHmacChannelAuthenticator::CreateForClient(remote_cert_,
                                                        auth_key_);
  }
}

std::string Spake2Authenticator::CalculateVerificationHash(
    bool from_host,
    const std::string& local_id,
    const std::string& remote_id) {
  std::string message = (from_host ? "host" : "client") +
                        PrefixWithLength(local_id) +
                        PrefixWithLength(remote_id);
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  std::string result(hmac.DigestLength(), '\0');
  if (!hmac.Init(auth_key_) ||
      !hmac.Sign(message, reinterpret_cast<uint8_t*>(&result[0]),
                 result.length())) {
    LOG(FATAL) << "Failed to calculate HMAC.";
  }
  return result;
}

}  // namespace remoting::protocol
