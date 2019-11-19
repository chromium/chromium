// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PAIRING_HOST_AUTHENTICATOR_H_
#define REMOTING_PROTOCOL_PAIRING_HOST_AUTHENTICATOR_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "remoting/protocol/pairing_authenticator_base.h"
#include "remoting/protocol/pairing_registry.h"

namespace remoting {
namespace protocol {

class PairingRegistry;

class PairingHostAuthenticator : public PairingAuthenticatorBase {
 public:
  PairingHostAuthenticator(
      scoped_refptr<PairingRegistry> pairing_registry,
      const CreateBaseAuthenticatorCallback& create_base_authenticator_callback,
      const std::string& pin);
  ~PairingHostAuthenticator() override;

  // Initialize the authenticator with the given |client_id| in
  // |preferred_initial_state|.
  void Initialize(const std::string& client_id,
                  Authenticator::State preferred_initial_state,
                  const base::Closure& resume_callback);

  // Authenticator interface.
  State state() const override;
  RejectionReason rejection_reason() const override;

 private:
  // PairingAuthenticatorBase overrides.
  void CreateSpakeAuthenticatorWithPin(
      State initial_state,
      const base::Closure& resume_callback) override;

  // Continue initializing once the pairing information for the client id has
  // been received.
  void InitializeWithPairing(Authenticator::State preferred_initial_state,
                             const base::Closure& resume_callback,
                             PairingRegistry::Pairing pairing);

  // Protocol state.
  scoped_refptr<PairingRegistry> pairing_registry_;
  CreateBaseAuthenticatorCallback create_base_authenticator_callback_;
  std::string pin_;
  bool protocol_error_ = false;
  bool waiting_for_paired_secret_ = false;

  base::WeakPtrFactory<PairingHostAuthenticator> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PairingHostAuthenticator);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PAIRING_AUTHENTICATOR_H_
