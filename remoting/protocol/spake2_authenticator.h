// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_SPAKE2_AUTHENTICATOR_H_
#define REMOTING_PROTOCOL_SPAKE2_AUTHENTICATOR_H_

#include <memory>
#include <queue>
#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "remoting/protocol/authenticator.h"
#include "third_party/boringssl/src/include/openssl/base.h"

namespace remoting {

class RsaKeyPair;

namespace protocol {

// Authenticator that uses SPAKE2 implementation from BoringSSL. It
// implements SPAKE2 over Curve25519.
class Spake2Authenticator : public Authenticator {
 public:
  static std::unique_ptr<Authenticator> CreateForClient(
      const std::string& local_id,
      const std::string& remote_id,
      const std::string& shared_secret,
      State initial_state);

  static std::unique_ptr<Authenticator> CreateForHost(
      const std::string& local_id,
      const std::string& remote_id,
      const std::string& local_cert,
      scoped_refptr<RsaKeyPair> key_pair,
      const std::string& shared_secret,
      State initial_state);

  ~Spake2Authenticator() override;

  // Authenticator interface.
  State state() const override;
  bool started() const override;
  RejectionReason rejection_reason() const override;
  void ProcessMessage(const jingle_xmpp::XmlElement* message,
                      base::OnceClosure resume_callback) override;
  std::unique_ptr<jingle_xmpp::XmlElement> GetNextMessage() override;
  const std::string& GetAuthKey() const override;
  std::unique_ptr<ChannelAuthenticator> CreateChannelAuthenticator()
      const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(Spake2AuthenticatorTest, InvalidSecret);

  Spake2Authenticator(const std::string& local_id,
                      const std::string& remote_id,
                      const std::string& shared_secret,
                      bool is_host,
                      State initial_state);

  virtual void ProcessMessageInternal(const jingle_xmpp::XmlElement* message);

  std::string CalculateVerificationHash(bool from_host,
                                        const std::string& local_id,
                                        const std::string& remote_id);

  const std::string local_id_;
  const std::string remote_id_;
  const std::string shared_secret_;
  const bool is_host_;

  // Used only for host authenticators.
  std::string local_cert_;
  scoped_refptr<RsaKeyPair> local_key_pair_;

  // Used only for client authenticators.
  std::string remote_cert_;

  // Used for both host and client authenticators.
  SPAKE2_CTX* spake2_context_;
  State state_;
  bool started_ = false;
  RejectionReason rejection_reason_ = INVALID_CREDENTIALS;
  std::string local_spake_message_;
  bool spake_message_sent_ = false;
  std::string outgoing_verification_hash_;
  std::string auth_key_;
  std::string expected_verification_hash_;

  DISALLOW_COPY_AND_ASSIGN(Spake2Authenticator);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_SPAKE2_AUTHENTICATOR_H_
