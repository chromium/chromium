// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_REJECTING_AUTHENTICATOR_H_
#define REMOTING_PROTOCOL_REJECTING_AUTHENTICATOR_H_

#include <string>

#include "remoting/protocol/authenticator.h"

namespace remoting::protocol {

// Authenticator that accepts one message and rejects connection after that.
class RejectingAuthenticator : public Authenticator {
 public:
  RejectingAuthenticator(RejectionReason rejection_reason);

  RejectingAuthenticator(const RejectingAuthenticator&) = delete;
  RejectingAuthenticator& operator=(const RejectingAuthenticator&) = delete;

  ~RejectingAuthenticator() override;

  // Authenticator interface
  CredentialsType credentials_type() const override;
  const Authenticator& implementing_authenticator() const override;
  State state() const override;
  bool started() const override;
  RejectionReason rejection_reason() const override;
  void ProcessMessage(const jingle_xmpp::XmlElement* message,
                      base::OnceClosure resume_callback) override;
  std::unique_ptr<jingle_xmpp::XmlElement> GetNextMessage() override;
  const std::string& GetAuthKey() const override;
  const SessionPolicies* GetSessionPolicies() const override;
  std::unique_ptr<ChannelAuthenticator> CreateChannelAuthenticator()
      const override;

 private:
  RejectionReason rejection_reason_;
  State state_ = WAITING_MESSAGE;
  std::string auth_key_;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_REJECTING_AUTHENTICATOR_H_
