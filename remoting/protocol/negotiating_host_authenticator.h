// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_NEGOTIATING_HOST_AUTHENTICATOR_H_
#define REMOTING_PROTOCOL_NEGOTIATING_HOST_AUTHENTICATOR_H_

#include <memory>
#include <string>
#include <string_view>

#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/negotiating_authenticator_base.h"

namespace remoting::protocol {

struct HostAuthenticationConfig;

// Host-side implementation of NegotiatingAuthenticatorBase.
// See comments in negotiating_authenticator_base.h for a general explanation.
class NegotiatingHostAuthenticator : public NegotiatingAuthenticatorBase {
 public:
  explicit NegotiatingHostAuthenticator(
      std::string_view local_id,
      std::string_view remote_id,
      std::unique_ptr<HostAuthenticationConfig> config);

  NegotiatingHostAuthenticator(const NegotiatingHostAuthenticator&) = delete;
  NegotiatingHostAuthenticator& operator=(const NegotiatingHostAuthenticator&) =
      delete;

  ~NegotiatingHostAuthenticator() override;

  // NegotiatingAuthenticatorBase:
  void ProcessMessage(const jingle_xmpp::XmlElement* message,
                      base::OnceClosure resume_callback) override;
  std::unique_ptr<jingle_xmpp::XmlElement> GetNextMessage() override;

 private:
  // (Asynchronously) creates an authenticator, and stores it in
  // |current_authenticator_|. Authenticators that can be started in either
  // state will be created in |preferred_initial_state|.
  void CreateAuthenticator(Authenticator::State preferred_initial_state,
                           base::OnceClosure resume_callback);

  std::string local_id_;
  std::string remote_id_;
  std::string client_id_;
  std::unique_ptr<HostAuthenticationConfig> config_;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_NEGOTIATING_HOST_AUTHENTICATOR_H_
