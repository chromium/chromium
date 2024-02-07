// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CROS_AUTHENTICATOR_H_
#define DEVICE_FIDO_CROS_AUTHENTICATOR_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/u2f/u2f_interface.pb.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "device/fido/authenticator_supported_options.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_transport_protocol.h"

namespace device {

class COMPONENT_EXPORT(DEVICE_FIDO) ChromeOSAuthenticator
    : public FidoAuthenticator {
 public:
  struct COMPONENT_EXPORT(DEVICE_FIDO) Config {
    // Whether uv platform authenticator is available (PIN or fingerprint
    // enrolled).
    bool uv_available;
    // Whether using the power button for user presence checking is enabled.
    bool power_button_enabled;
  };

  ChromeOSAuthenticator(
      base::RepeatingCallback<std::string()> generate_request_id_callback,
      Config config);
  ~ChromeOSAuthenticator() override;

  static void HasLegacyU2fCredentialForGetAssertionRequest(
      const CtapGetAssertionRequest& request,
      base::OnceCallback<void(bool has_credential)> callback);

  // Invokes |callback| with a bool indicating whether the platform
  // authenticator is available, which is true if the current user has a PIN set
  // up or biometrics enrolled.
  static void IsUVPlatformAuthenticatorAvailable(
      base::OnceCallback<void(bool is_available)> callback);

  // Invokes |callback| with a bool indicating whether the legacy U2F
  // authenticator, which uses the power button for user presence checking, is
  // enabled in the OS either via the DeviceSecondFactorAuthentication
  // enterprise policy or debug u2f_flags.
  static void IsPowerButtonModeEnabled(
      base::OnceCallback<void(bool is_enabled)> callback);

  // Invokes |callback| with a bool indicating whether u2fd supports WebAuthn
  // requests from the lacros browser.
  static void IsLacrosSupported(
      base::OnceCallback<void(bool supported)> callback);

  // FidoAuthenticator

  // Calls the u2fd API `GetAlgorithms` and cache the result.
  void InitializeAuthenticator(base::OnceClosure callback) override;

  // Since this method is synchronous, it will simply return the GetAlgorithms
  // result obtained during `InitializeAuthenticator`.
  std::optional<base::span<const int32_t>> GetAlgorithms() override;

  void MakeCredential(CtapMakeCredentialRequest request,
                      MakeCredentialOptions request_options,
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

  void GetTouch(base::OnceClosure callback) override {}
  base::WeakPtr<FidoAuthenticator> GetWeakPtr() override;

 private:
  // Cache the supported algorithms in response, and run the barrier callback
  // of `InitializeAuthenticator`.
  void OnGetAlgorithmsResponse(
      base::OnceClosure callback,
      std::optional<u2f::GetAlgorithmsResponse> response);
  // Cache whether power button is enabled, and run the barrier callback
  // of `InitializeAuthenticator`.
  void OnIsPowerButtonModeEnabled(base::OnceClosure callback, bool enabled);
  void OnHasCredentialInformationForRequest(
      GetPlatformCredentialInfoForRequestCallback callback,
      std::optional<u2f::HasCredentialsResponse> response);
  void OnMakeCredentialResponse(
      CtapMakeCredentialRequest request,
      MakeCredentialCallback callback,
      std::optional<u2f::MakeCredentialResponse> response);
  void OnGetAssertionResponse(
      CtapGetAssertionRequest request,
      GetAssertionCallback callback,
      std::optional<u2f::GetAssertionResponse> response);
  void OnCancelResponse(
      std::optional<u2f::CancelWebAuthnFlowResponse> response);

  // Current request_id, used for cancelling the request.
  std::string current_request_id_;

  // Callback to set request_id in the window property.
  base::RepeatingCallback<std::string()> generate_request_id_callback_;
  const Config config_;
  std::optional<std::vector<int32_t>> supported_algorithms_;
  bool u2f_enabled_ = false;
  base::WeakPtrFactory<ChromeOSAuthenticator> weak_factory_;
};

}  // namespace device

#endif  // DEVICE_FIDO_CROS_AUTHENTICATOR_H_
