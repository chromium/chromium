// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/spake2_authenticator.h"

#include <utility>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/byte_conversions.h"
#include "base/strings/string_view_util.h"
#include "crypto/hmac.h"
#include "crypto/secure_util.h"
#include "remoting/base/constants.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/ssl_hmac_channel_authenticator.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"

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

Authenticator::RejectionDetails Spake2Authenticator::rejection_details() const {
  DCHECK_EQ(state(), REJECTED);
  return rejection_details_;
}

void Spake2Authenticator::ProcessMessage(const JingleAuthentication& message,
                                         base::OnceClosure resume_callback) {
  ProcessMessageInternal(message);
  std::move(resume_callback).Run();
}

void Spake2Authenticator::ProcessMessageInternal(
    const JingleAuthentication& message) {
  DCHECK_EQ(state(), WAITING_MESSAGE);

  // Only update |remote_cert_| if a certificate is provided in the message.
  // This prevents overwriting a valid certificate with an empty value in
  // multi-step exchanges where the certificate is only provided in the first
  // message.
  if (!message.certificate.empty()) {
    remote_cert_.assign(
        reinterpret_cast<const char*>(message.certificate.data()),
        message.certificate.size());
  }

  // Client always expects certificate in the first message.
  if (!is_host_ && remote_cert_.empty()) {
    state_ = REJECTED;
    rejection_reason_ = RejectionReason::INVALID_STATE;
    rejection_details_ = RejectionDetails("No valid host certificate.");
    return;
  }

  // |auth_key_| is generated when <spake-message> is received.
  if (auth_key_.empty()) {
    if (message.spake_message.empty()) {
      state_ = REJECTED;
      rejection_reason_ = RejectionReason::INVALID_ARGUMENT;
      rejection_details_ = RejectionDetails("<spake-message> not found.");
      return;
    }
    uint8_t key[SPAKE2_MAX_KEY_SIZE];
    size_t key_size;
    started_ = true;
    int result = SPAKE2_process_msg(
        spake2_context_, key, &key_size, sizeof(key),
        reinterpret_cast<const uint8_t*>(message.spake_message.data()),
        message.spake_message.size());
    if (!result) {
      state_ = REJECTED;
      rejection_reason_ = RejectionReason::INVALID_CREDENTIALS;
      rejection_details_ =
          RejectionDetails("Failed to process SPAKE2 message.");
      return;
    }
    CHECK(key_size);
    auth_key_.assign(reinterpret_cast<char*>(key), key_size);

    outgoing_verification_hash_ =
        CalculateVerificationHash(is_host_, local_id_, remote_id_);
    expected_verification_hash_ =
        CalculateVerificationHash(!is_host_, remote_id_, local_id_);
  } else if (!message.spake_message.empty()) {
    state_ = REJECTED;
    rejection_reason_ = RejectionReason::INVALID_STATE;
    rejection_details_ =
        RejectionDetails("Received duplicate <spake-message>.");
    return;
  }

  if (spake_message_sent_ && message.verification_hash.empty()) {
    state_ = REJECTED;
    rejection_reason_ = RejectionReason::INVALID_STATE;
    rejection_details_ =
        RejectionDetails("Didn't receive <verification-hash> when expected.");
    return;
  }

  if (!message.verification_hash.empty()) {
    if (!crypto::SecureMemEqual(
            base::as_byte_span(message.verification_hash),
            base::as_byte_span(expected_verification_hash_))) {
      state_ = REJECTED;
      rejection_reason_ = RejectionReason::INVALID_CREDENTIALS;
      rejection_details_ = RejectionDetails("Verification hash mismatched.");
      return;
    }
    state_ = ACCEPTED;
    return;
  }

  state_ = MESSAGE_READY;
}

JingleAuthentication Spake2Authenticator::GetNextMessage() {
  DCHECK_EQ(state(), MESSAGE_READY);

  JingleAuthentication message;

  if (!spake_message_sent_) {
    if (!local_cert_.empty()) {
      message.certificate.assign(local_cert_.begin(), local_cert_.end());
    }

    message.spake_message.assign(local_spake_message_.begin(),
                                 local_spake_message_.end());

    spake_message_sent_ = true;
  }

  if (!outgoing_verification_hash_.empty()) {
    message.verification_hash.assign(outgoing_verification_hash_.begin(),
                                     outgoing_verification_hash_.end());
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
  crypto::hmac::HmacSigner signer(crypto::hash::kSha256,
                                  base::as_byte_span(auth_key_));
  std::string_view direction = from_host ? "host" : "client";
  signer.Update(base::as_byte_span(direction));
  signer.Update(base::U32ToBigEndian(local_id.size()));
  signer.Update(base::as_byte_span(local_id));
  signer.Update(base::U32ToBigEndian(remote_id.size()));
  signer.Update(base::as_byte_span(remote_id));
  std::array<uint8_t, crypto::hash::kSha256Size> result;
  signer.Finish(result);
  return std::string(base::as_string_view(result));
}

}  // namespace remoting::protocol
