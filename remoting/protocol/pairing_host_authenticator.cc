// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/pairing_host_authenticator.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/channel_authenticator.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting::protocol {

PairingHostAuthenticator::PairingHostAuthenticator(
    scoped_refptr<PairingRegistry> pairing_registry,
    const CreateBaseAuthenticatorCallback& create_base_authenticator_callback,
    const std::string& pin)
    : pairing_registry_(pairing_registry),
      create_base_authenticator_callback_(create_base_authenticator_callback),
      pin_(pin) {}

void PairingHostAuthenticator::Initialize(
    const std::string& client_id,
    Authenticator::State preferred_initial_state,
    base::OnceClosure resume_callback) {
  DCHECK(!spake2_authenticator_);

  if (client_id.empty()) {
    using_paired_secret_ = false;
    error_message_ = "client-id-unknown";
    spake2_authenticator_ =
        create_base_authenticator_callback_.Run(pin_, MESSAGE_READY);
    std::move(resume_callback).Run();
    return;
  }

  using_paired_secret_ = true;
  waiting_for_paired_secret_ = true;
  pairing_registry_->GetPairing(
      client_id,
      base::BindOnce(&PairingHostAuthenticator::InitializeWithPairing,
                     weak_factory_.GetWeakPtr(), preferred_initial_state,
                     std::move(resume_callback)));
}

PairingHostAuthenticator::~PairingHostAuthenticator() = default;

Authenticator::State PairingHostAuthenticator::state() const {
  if (protocol_error_) {
    return REJECTED;
  } else if (waiting_for_paired_secret_) {
    return PROCESSING_MESSAGE;
  }
  return PairingAuthenticatorBase::state();
}

Authenticator::RejectionReason PairingHostAuthenticator::rejection_reason()
    const {
  if (protocol_error_) {
    return RejectionReason::PROTOCOL_ERROR;
  }
  return PairingAuthenticatorBase::rejection_reason();
}

void PairingHostAuthenticator::CreateSpakeAuthenticatorWithPin(
    State initial_state,
    base::OnceClosure resume_callback) {
  spake2_authenticator_ =
      create_base_authenticator_callback_.Run(pin_, initial_state);
  std::move(resume_callback).Run();
}

void PairingHostAuthenticator::InitializeWithPairing(
    Authenticator::State preferred_initial_state,
    base::OnceClosure resume_callback,
    PairingRegistry::Pairing pairing) {
  DCHECK(waiting_for_paired_secret_);
  waiting_for_paired_secret_ = false;
  std::string pairing_secret = pairing.shared_secret();
  if (pairing_secret.empty()) {
    VLOG(0) << "Unknown client id";
    error_message_ = "unknown-client-id";
    using_paired_secret_ = false;
    // If pairing wasn't found then always start in the MESSAGE_READY state.
    spake2_authenticator_ =
        create_base_authenticator_callback_.Run(pin_, MESSAGE_READY);
  } else {
    using_paired_secret_ = true;
    spake2_authenticator_ = create_base_authenticator_callback_.Run(
        pairing_secret, preferred_initial_state);
  }
  std::move(resume_callback).Run();
}

}  // namespace remoting::protocol
