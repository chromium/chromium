// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_AUTHENTICATOR_H_
#define DEVICE_FIDO_MAC_AUTHENTICATOR_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/mac/availability.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece_forward.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/mac/operation.h"

namespace device {
namespace fido {
namespace mac {

struct AuthenticatorConfig;

class COMPONENT_EXPORT(DEVICE_FIDO) TouchIdAuthenticator
    : public FidoAuthenticator {
 public:
  // IsAvailable returns true iff Touch ID is available and
  // enrolled on the current device and the current binary carries
  // a keychain-access-groups entitlement that matches the one set
  // in |config|.
  //
  // Note that this may differ from the result of
  // AuthenticatorImpl::IsUserVerifyingPlatformAuthenticatorAvailable(),
  // which also checks whether the embedder supports this
  // authenticator, and if the request occurs from an
  // off-the-record/incognito context.
  static bool IsAvailable(const AuthenticatorConfig& config);

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
                    GetAssertionCallback callback) override;
  void GetNextAssertion(GetAssertionCallback callback) override;
  void Cancel() override;
  std::string GetId() const override;
  base::string16 GetDisplayName() const override;
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

  // The keychain access group under which credentials are stored in the macOS
  // keychain for access control. The set of all access groups that the
  // application belongs to is stored in the entitlements file that gets
  // embedded into the application during code signing. For more information
  // see
  // https://developer.apple.com/documentation/security/ksecattraccessgroup?language=objc.
  std::string keychain_access_group_;

  std::string metadata_secret_;

  std::unique_ptr<Operation> operation_;

  base::WeakPtrFactory<TouchIdAuthenticator> weak_factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TouchIdAuthenticator);
};

}  // namespace mac
}  // namespace fido
}  // namespace device

#endif  // DEVICE_FIDO_MAC_AUTHENTICATOR_H_
