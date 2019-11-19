// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/pairing_client_authenticator.h"

#include "base/bind.h"
#include "base/logging.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/auth_util.h"
#include "remoting/protocol/channel_authenticator.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting {
namespace protocol {

PairingClientAuthenticator::PairingClientAuthenticator(
    const ClientAuthenticationConfig& client_auth_config,
    const CreateBaseAuthenticatorCallback& create_base_authenticator_callback)
    : client_auth_config_(client_auth_config),
      create_base_authenticator_callback_(create_base_authenticator_callback) {}

PairingClientAuthenticator::~PairingClientAuthenticator() = default;

void PairingClientAuthenticator::Start(State initial_state,
                                       const base::Closure& resume_callback) {
  if (client_auth_config_.pairing_client_id.empty() ||
      client_auth_config_.pairing_secret.empty()) {
    // Send pairing error to make it clear that PIN is being used instead of
    // pairing secret.
    error_message_ = "not-paired";
    using_paired_secret_ = false;
    CreateSpakeAuthenticatorWithPin(initial_state, resume_callback);
  } else {
    StartPaired(initial_state);
    resume_callback.Run();
  }
}

void PairingClientAuthenticator::StartPaired(State initial_state) {
  DCHECK(!client_auth_config_.pairing_client_id.empty());
  DCHECK(!client_auth_config_.pairing_secret.empty());

  using_paired_secret_ = true;
  spake2_authenticator_ = create_base_authenticator_callback_.Run(
      client_auth_config_.pairing_secret, initial_state);
}

Authenticator::State PairingClientAuthenticator::state() const {
  if (waiting_for_pin_)
    return PROCESSING_MESSAGE;
  return PairingAuthenticatorBase::state();
}

void PairingClientAuthenticator::CreateSpakeAuthenticatorWithPin(
    State initial_state,
    const base::Closure& resume_callback) {
  DCHECK(!waiting_for_pin_);
  waiting_for_pin_ = true;
  client_auth_config_.fetch_secret_callback.Run(
      true,
      base::Bind(&PairingClientAuthenticator::OnPinFetched,
                 weak_factory_.GetWeakPtr(), initial_state, resume_callback));
}

void PairingClientAuthenticator::OnPinFetched(
    State initial_state,
    const base::Closure& resume_callback,
    const std::string& pin) {
  DCHECK(waiting_for_pin_);
  DCHECK(!spake2_authenticator_);
  waiting_for_pin_ = false;
  spake2_authenticator_ = create_base_authenticator_callback_.Run(
      GetSharedSecretHash(client_auth_config_.host_id, pin), initial_state);
  resume_callback.Run();
}

}  // namespace protocol
}  // namespace remoting
