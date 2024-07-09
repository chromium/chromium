// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_NEGOTIATING_CLIENT_AUTHENTICATOR_H_
#define REMOTING_PROTOCOL_NEGOTIATING_CLIENT_AUTHENTICATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/client_authentication_config.h"
#include "remoting/protocol/negotiating_authenticator_base.h"

namespace remoting::protocol {

// Client-side implementation of NegotiatingAuthenticatorBase.
// See comments in negotiating_authenticator_base.h for a general explanation.
class NegotiatingClientAuthenticator : public NegotiatingAuthenticatorBase {
 public:
  explicit NegotiatingClientAuthenticator(
      const std::string& local_id,
      const std::string& remote_id,
      const ClientAuthenticationConfig& config);

  NegotiatingClientAuthenticator(const NegotiatingClientAuthenticator&) =
      delete;
  NegotiatingClientAuthenticator& operator=(
      const NegotiatingClientAuthenticator&) = delete;

  ~NegotiatingClientAuthenticator() override;

  // NegotiatingAuthenticatorBase:
  void ProcessMessage(const jingle_xmpp::XmlElement* message,
                      base::OnceClosure resume_callback) override;
  std::unique_ptr<jingle_xmpp::XmlElement> GetNextMessage() override;

 private:
  // (Asynchronously) creates an authenticator, and stores it in
  // |current_authenticator_|. Authenticators that can be started in either
  // state will be created in |preferred_initial_state|.
  // |resume_callback| is called after |current_authenticator_| is set.
  void CreateAuthenticatorForCurrentMethod(
      Authenticator::State preferred_initial_state,
      base::OnceClosure resume_callback);

  // Creates a shared-secret authenticator in state |initial_state| with the
  // given |shared_secret|, then runs |resume_callback|.
  void CreateSharedSecretAuthenticator(Authenticator::State initial_state,
                                       base::OnceClosure resume_callback,
                                       const std::string& shared_secret);

  bool is_paired();

  std::string local_id_;
  std::string remote_id_;

  ClientAuthenticationConfig config_;

  // Internal NegotiatingClientAuthenticator data.
  bool method_set_by_host_ = false;
  base::WeakPtrFactory<NegotiatingClientAuthenticator> weak_factory_{this};
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_NEGOTIATING_CLIENT_AUTHENTICATOR_H_
