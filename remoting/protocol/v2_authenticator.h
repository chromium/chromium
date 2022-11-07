// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_V2_AUTHENTICATOR_H_
#define REMOTING_PROTOCOL_V2_AUTHENTICATOR_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/containers/queue.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "crypto/p224_spake.h"
#include "remoting/protocol/authenticator.h"

namespace remoting {

class RsaKeyPair;

namespace protocol {

class V2Authenticator : public Authenticator {
 public:
  static bool IsEkeMessage(const jingle_xmpp::XmlElement* message);

  static std::unique_ptr<Authenticator> CreateForClient(
      const std::string& shared_secret,
      State initial_state);

  static std::unique_ptr<Authenticator> CreateForHost(
      const std::string& local_cert,
      scoped_refptr<RsaKeyPair> key_pair,
      const std::string& shared_secret,
      State initial_state);

  V2Authenticator(const V2Authenticator&) = delete;
  V2Authenticator& operator=(const V2Authenticator&) = delete;

  ~V2Authenticator() override;

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
  FRIEND_TEST_ALL_PREFIXES(V2AuthenticatorTest, InvalidSecret);

  V2Authenticator(crypto::P224EncryptedKeyExchange::PeerType type,
                  const std::string& shared_secret,
                  State initial_state);

  virtual void ProcessMessageInternal(const jingle_xmpp::XmlElement* message);

  bool is_host_side() const;

  // Used only for host authenticators.
  std::string local_cert_;
  scoped_refptr<RsaKeyPair> local_key_pair_;
  bool certificate_sent_;

  // Used only for client authenticators.
  std::string remote_cert_;

  // Used for both host and client authenticators.
  crypto::P224EncryptedKeyExchange key_exchange_impl_;
  State state_;
  bool started_;
  RejectionReason rejection_reason_;
  base::queue<std::string> pending_messages_;
  std::string auth_key_;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_V2_AUTHENTICATOR_H_
