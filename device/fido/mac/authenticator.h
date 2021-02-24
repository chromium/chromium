// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_AUTHENTICATOR_H_
#define DEVICE_FIDO_MAC_AUTHENTICATOR_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece_forward.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/mac/credential_store.h"
#include "device/fido/mac/operation.h"

namespace device {
namespace fido {
namespace mac {

struct AuthenticatorConfig;

// TouchIdAuthenticator is a platform authenticator on macOS. It persists Secure
// Enclave backed credentials along with metadata in the macOS keychain. Each
// Chrome profile maintains its own set of credentials.
//
// Despite the name, any local login factor can be used for User Verification
// (e.g. Touch ID, password, Apple Watch).
class COMPONENT_EXPORT(DEVICE_FIDO) TouchIdAuthenticator
    : public FidoAuthenticator {
 public:
  // IsAvailable runs |callback| with a bool incidating whether the
  // authenticator is available, i.e. whether the device has a Secure Enclave
  // and the current binary carries a keychain-access-groups entitlement that
  // matches the one set in |config|.
  static void IsAvailable(AuthenticatorConfig config,
                          base::OnceCallback<void(bool is_available)> callback);

  // CreateIfAvailable returns a TouchIdAuthenticator. Callers must check
  // IsAvailable() first.
  static std::unique_ptr<TouchIdAuthenticator> Create(
      AuthenticatorConfig config);

  ~TouchIdAuthenticator() override;

  bool HasCredentialForGetAssertionRequest(
      const CtapGetAssertionRequest& request);

  // FidoAuthenticator
  void InitializeAuthenticator(base::OnceClosure callback) override;
  void MakeCredential(CtapMakeCredentialRequest request,
                      MakeCredentialCallback callback) override;
  void GetAssertion(CtapGetAssertionRequest request,
                    CtapGetAssertionOptions options,
                    GetAssertionCallback callback) override;
  void GetNextAssertion(GetAssertionCallback callback) override;
  void Cancel() override;
  std::string GetId() const override;
  const base::Optional<AuthenticatorSupportedOptions>& Options() const override;
  base::Optional<FidoTransportProtocol> AuthenticatorTransport() const override;
  bool IsInPairingMode() const override;
  bool IsPaired() const override;
  bool RequiresBlePairingPin() const override;
  bool IsTouchIdAuthenticator() const override;
  void GetTouch(base::OnceClosure callback) override;
  base::WeakPtr<FidoAuthenticator> GetWeakPtr() override;

 private:
  TouchIdAuthenticator(std::string keychain_access_group,
                       std::string metadata_secret);

  TouchIdCredentialStore credential_store_;

  std::unique_ptr<Operation> operation_;

  base::WeakPtrFactory<TouchIdAuthenticator> weak_factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TouchIdAuthenticator);
};

}  // namespace mac
}  // namespace fido
}  // namespace device

#endif  // DEVICE_FIDO_MAC_AUTHENTICATOR_H_
