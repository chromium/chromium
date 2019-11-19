// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_NEGOTIATING_CLIENT_AUTHENTICATOR_H_
#define REMOTING_PROTOCOL_NEGOTIATING_CLIENT_AUTHENTICATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/client_authentication_config.h"
#include "remoting/protocol/negotiating_authenticator_base.h"
#include "remoting/protocol/third_party_client_authenticator.h"

namespace remoting {
namespace protocol {

// Client-side implementation of NegotiatingAuthenticatorBase.
// See comments in negotiating_authenticator_base.h for a general explanation.
class NegotiatingClientAuthenticator : public NegotiatingAuthenticatorBase {
 public:
  explicit NegotiatingClientAuthenticator(
      const std::string& local_id,
      const std::string& remote_id,
      const ClientAuthenticationConfig& config);
  ~NegotiatingClientAuthenticator() override;

  // Overriden from Authenticator.
  void ProcessMessage(const jingle_xmpp::XmlElement* message,
                      const base::Closure& resume_callback) override;
  std::unique_ptr<jingle_xmpp::XmlElement> GetNextMessage() override;

 private:
  // (Asynchronously) creates an authenticator, and stores it in
  // |current_authenticator_|. Authenticators that can be started in either
  // state will be created in |preferred_initial_state|.
  // |resume_callback| is called after |current_authenticator_| is set.
  void CreateAuthenticatorForCurrentMethod(
      Authenticator::State preferred_initial_state,
      const base::Closure& resume_callback);

  // If possible, create a preferred authenticator ready to send an
  // initial message optimistically to the host. The host is free to
  // ignore the client's preferred authenticator and initial message
  // and to instead reply with an alternative method. See the comments
  // in negotiating_authenticator_base.h for more details.
  //
  // Sets |current_authenticator_| and |current_method_| iff the client
  // has a preferred authenticator that can optimistically send an initial
  // message.
  void CreatePreferredAuthenticator();

  // Creates a shared-secret authenticator in state |initial_state| with the
  // given |shared_secret|, then runs |resume_callback|.
  void CreateSharedSecretAuthenticator(Authenticator::State initial_state,
                                       const base::Closure& resume_callback,
                                       const std::string& shared_secret);

  bool is_paired();

  std::string local_id_;
  std::string remote_id_;

  ClientAuthenticationConfig config_;

  // Internal NegotiatingClientAuthenticator data.
  bool method_set_by_host_ = false;
  base::WeakPtrFactory<NegotiatingClientAuthenticator> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NegotiatingClientAuthenticator);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_NEGOTIATING_CLIENT_AUTHENTICATOR_H_
