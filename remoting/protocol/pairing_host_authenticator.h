// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PAIRING_HOST_AUTHENTICATOR_H_
#define REMOTING_PROTOCOL_PAIRING_HOST_AUTHENTICATOR_H_

#include "base/memory/weak_ptr.h"
#include "remoting/protocol/pairing_authenticator_base.h"
#include "remoting/protocol/pairing_registry.h"

namespace remoting::protocol {

class PairingRegistry;

class PairingHostAuthenticator : public PairingAuthenticatorBase {
 public:
  PairingHostAuthenticator(
      scoped_refptr<PairingRegistry> pairing_registry,
      const CreateBaseAuthenticatorCallback& create_base_authenticator_callback,
      const std::string& pin);

  PairingHostAuthenticator(const PairingHostAuthenticator&) = delete;
  PairingHostAuthenticator& operator=(const PairingHostAuthenticator&) = delete;

  ~PairingHostAuthenticator() override;

  // Initialize the authenticator with the given |client_id| in
  // |preferred_initial_state|.
  void Initialize(const std::string& client_id,
                  Authenticator::State preferred_initial_state,
                  base::OnceClosure resume_callback);

  // Authenticator interface.
  State state() const override;
  RejectionReason rejection_reason() const override;

 private:
  // PairingAuthenticatorBase overrides.
  void CreateSpakeAuthenticatorWithPin(
      State initial_state,
      base::OnceClosure resume_callback) override;

  // Continue initializing once the pairing information for the client id has
  // been received.
  void InitializeWithPairing(Authenticator::State preferred_initial_state,
                             base::OnceClosure resume_callback,
                             PairingRegistry::Pairing pairing);

  // Protocol state.
  scoped_refptr<PairingRegistry> pairing_registry_;
  CreateBaseAuthenticatorCallback create_base_authenticator_callback_;
  std::string pin_;
  bool protocol_error_ = false;
  bool waiting_for_paired_secret_ = false;

  base::WeakPtrFactory<PairingHostAuthenticator> weak_factory_{this};
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_PAIRING_HOST_AUTHENTICATOR_H_
