// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_WIN_AUTHENTICATOR_H_
#define DEVICE_FIDO_WIN_AUTHENTICATOR_H_

#include <Combaseapi.h>

#include <memory>
#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_constants.h"

namespace device {

class WinWebAuthnApi;

// WinWebAuthnApiAuthenticator forwards WebAuthn requests to external
// authenticators via the native Windows WebAuthentication API
// (webauthn.dll).
//
// Callers must ensure that WinWebAuthnApi::IsAvailable() returns true
// before creating instances of this class.
class COMPONENT_EXPORT(DEVICE_FIDO) WinWebAuthnApiAuthenticator
    : public FidoAuthenticator {
 public:
  // This method is safe to call without checking WinWebAuthnApi::IsAvailable().
  // Returns false if |api| is nullptr.
  static void IsUserVerifyingPlatformAuthenticatorAvailable(
      WinWebAuthnApi* api,
      base::OnceCallback<void(bool is_available)>);

  // This method is safe to call without checking WinWebAuthnApi::IsAvailable().
  // Returns false if |api| is nullptr.
  static void IsConditionalMediationAvailable(
      WinWebAuthnApi* api,
      base::OnceCallback<void(bool is_available)>);

  // Get metadata for all credentials in the platform authenticator. If such
  // metadata is not available then the callback will be invoked with an empty
  // list.
  static void EnumeratePlatformCredentials(
      WinWebAuthnApi* api,
      base::OnceCallback<
          void(std::vector<device::DiscoverableCredentialMetadata>)> callback);

  // Deletes a credential from the platform authenticator. The callback will be
  // invoked with a boolean that indicates whether the deletion was successful.
  static void DeletePlatformCredential(WinWebAuthnApi* api,
                                       base::span<const uint8_t> credential_id,
                                       base::OnceCallback<void(bool)> callback);

  // Instantiates an authenticator that uses the default WinWebAuthnApi.
  //
  // Callers must ensure that WinWebAuthnApi::IsAvailable() returns true
  // before creating instances of this class.
  WinWebAuthnApiAuthenticator(HWND current_window, WinWebAuthnApi* win_api_);

  WinWebAuthnApiAuthenticator(const WinWebAuthnApiAuthenticator&) = delete;
  WinWebAuthnApiAuthenticator& operator=(const WinWebAuthnApiAuthenticator&) =
      delete;

  ~WinWebAuthnApiAuthenticator() override;

  // ShowsResidentCredentialNotice returns true if the Windows native UI will
  // show a privacy notice dialog before a MakeCredential request that might
  // create a resident key or that requests attestation.
  bool ShowsResidentCredentialNotice() const;

 private:
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
  void GetTouch(base::OnceClosure callback) override;
  void Cancel() override;
  AuthenticatorType GetType() const override;
  std::string GetId() const override;
  const AuthenticatorSupportedOptions& Options() const override;
  std::optional<FidoTransportProtocol> AuthenticatorTransport() const override;
  base::WeakPtr<FidoAuthenticator> GetWeakPtr() override;

  void MakeCredentialDone(
      MakeCredentialCallback callback,
      std::pair<MakeCredentialStatus,
                std::optional<AuthenticatorMakeCredentialResponse>> result);
  void GetAssertionDone(
      GetAssertionCallback callback,
      std::pair<GetAssertionStatus,
                std::optional<AuthenticatorGetAssertionResponse>> result);

  // options_ is per-instance because the capabilities of `win_api_` can
  // change at run-time in tests.
  const AuthenticatorSupportedOptions options_;
  HWND current_window_;
  bool is_pending_ = false;
  bool waiting_for_cancellation_ = false;
  GUID cancellation_id_ = {};
  // The pointee of |win_api_| is assumed to be a singleton that outlives
  // this instance.
  raw_ptr<WinWebAuthnApi, DanglingUntriaged> win_api_;

  // Verifies callbacks from |win_api_| are posted back onto the originating
  // sequence.
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<WinWebAuthnApiAuthenticator> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_FIDO_WIN_AUTHENTICATOR_H_
