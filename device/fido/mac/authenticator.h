// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_AUTHENTICATOR_H_
#define DEVICE_FIDO_MAC_AUTHENTICATOR_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/mac/credential_store.h"
#include "device/fido/mac/operation.h"

namespace device::fido::mac {

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
  // IsAvailable runs |callback| with a bool indicating whether the
  // authenticator is available, i.e. whether the device has a Secure Enclave
  // and the current binary carries a keychain-access-groups entitlement that
  // matches the one set in |config|.
  static void IsAvailable(AuthenticatorConfig config,
                          base::OnceCallback<void(bool is_available)> callback);

  // CreateIfAvailable returns a TouchIdAuthenticator. Callers must check
  // IsAvailable() first.
  static std::unique_ptr<TouchIdAuthenticator> Create(
      AuthenticatorConfig config);

  TouchIdAuthenticator(const TouchIdAuthenticator&) = delete;
  TouchIdAuthenticator& operator=(const TouchIdAuthenticator&) = delete;

  ~TouchIdAuthenticator() override;

  // FidoAuthenticator:
  void InitializeAuthenticator(base::OnceClosure callback) override;
  void MakeCredential(CtapMakeCredentialRequest request,
                      MakeCredentialOptions options,
                      MakeCredentialCallback callback) override;
  void GetAssertion(CtapGetAssertionRequest request,
                    CtapGetAssertionOptions options,
                    GetAssertionCallback callback) override;
  void GetPlatformCredentialInfoForRequest(
      const CtapGetAssertionRequest& request,
      const CtapGetAssertionOptions& options,
      GetPlatformCredentialInfoForRequestCallback callback) override;
  void Cancel() override;
  AuthenticatorType GetType() const override;
  std::string GetId() const override;
  const AuthenticatorSupportedOptions& Options() const override;
  std::optional<FidoTransportProtocol> AuthenticatorTransport() const override;
  void GetTouch(base::OnceClosure callback) override;
  base::WeakPtr<FidoAuthenticator> GetWeakPtr() override;

 private:
  TouchIdAuthenticator(std::string keychain_access_group,
                       std::string metadata_secret);

  TouchIdCredentialStore credential_store_;

  std::unique_ptr<Operation> operation_;

  base::WeakPtrFactory<TouchIdAuthenticator> weak_factory_;
};

}  // namespace device::fido::mac

#endif  // DEVICE_FIDO_MAC_AUTHENTICATOR_H_
