// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_WIN_AUTHENTICATOR_H_
#define DEVICE_FIDO_WIN_AUTHENTICATOR_H_

#include <Combaseapi.h>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "device/fido/fido_authenticator.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

  // ShowsPrivacyNotice returns true if the Windows native UI will show a
  // privacy notice dialog before a MakeCredential request that might create
  // a resident key or that requests attestation.
  bool ShowsPrivacyNotice() const;

 private:
  // FidoAuthenticator:
  void InitializeAuthenticator(base::OnceClosure callback) override;
  void MakeCredential(CtapMakeCredentialRequest request,
                      MakeCredentialOptions options,
                      MakeCredentialCallback callback) override;
  void GetAssertion(CtapGetAssertionRequest request,
                    CtapGetAssertionOptions options,
                    GetAssertionCallback callback) override;
  void GetCredentialInformationForRequest(
      const CtapGetAssertionRequest& request,
      base::OnceCallback<void(std::vector<DiscoverableCredentialMetadata>,
                              bool)> callback) override;
  void GetTouch(base::OnceClosure callback) override;
  void Cancel() override;
  Type GetType() const override;
  std::string GetId() const override;
  bool IsInPairingMode() const override;
  bool IsPaired() const override;
  bool RequiresBlePairingPin() const override;
  // SupportsCredProtectExtension returns whether the native API supports the
  // credProtect CTAP extension.
  bool SupportsCredProtectExtension() const override;
  bool SupportsHMACSecretExtension() const override;
  bool SupportsEnterpriseAttestation() const override;
  bool SupportsCredBlobOfSize(size_t num_bytes) const override;
  const absl::optional<AuthenticatorSupportedOptions>& Options() const override;
  absl::optional<FidoTransportProtocol> AuthenticatorTransport() const override;
  base::WeakPtr<FidoAuthenticator> GetWeakPtr() override;

  void MakeCredentialDone(
      MakeCredentialCallback callback,
      std::pair<CtapDeviceResponseCode,
                absl::optional<AuthenticatorMakeCredentialResponse>> result);
  void GetAssertionDone(
      GetAssertionCallback callback,
      std::pair<CtapDeviceResponseCode,
                absl::optional<AuthenticatorGetAssertionResponse>> result);

  HWND current_window_;

  bool is_pending_ = false;
  bool waiting_for_cancellation_ = false;
  GUID cancellation_id_ = {};
  // The pointee of |win_api_| is assumed to be a singleton that outlives
  // this instance.
  raw_ptr<WinWebAuthnApi> win_api_;

  // Verifies callbacks from |win_api_| are posted back onto the originating
  // sequence.
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<WinWebAuthnApiAuthenticator> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_FIDO_WIN_AUTHENTICATOR_H_
