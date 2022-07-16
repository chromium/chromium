// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CROS_AUTHENTICATOR_H_
#define DEVICE_FIDO_CROS_AUTHENTICATOR_H_

#include <string>

#include "base/component_export.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"

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
      base::RepeatingCallback<uint32_t()> generate_request_id_callback,
      Config config);
  ~ChromeOSAuthenticator() override;

  static void HasCredentialForGetAssertionRequest(
      const CtapGetAssertionRequest& request,
      base::OnceCallback<void(bool has_credential)> callback);

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

  // FidoAuthenticator
  void InitializeAuthenticator(base::OnceClosure callback) override;
  void MakeCredential(CtapMakeCredentialRequest request,
                      MakeCredentialOptions request_options,
                      MakeCredentialCallback callback) override;
  void GetAssertion(CtapGetAssertionRequest request,
                    CtapGetAssertionOptions options,
                    GetAssertionCallback callback) override;
  void GetNextAssertion(GetAssertionCallback callback) override {}
  void Cancel() override;
  std::string GetId() const override;
  const absl::optional<AuthenticatorSupportedOptions>& Options() const override;

  absl::optional<FidoTransportProtocol> AuthenticatorTransport() const override;

  bool IsInPairingMode() const override;
  bool IsPaired() const override;
  bool RequiresBlePairingPin() const override;

  bool IsChromeOSAuthenticator() const override;

  void GetTouch(base::OnceClosure callback) override {}
  base::WeakPtr<FidoAuthenticator> GetWeakPtr() override;

 private:
  void OnMakeCredentialResponse(
      CtapMakeCredentialRequest request,
      MakeCredentialCallback callback,
      absl::optional<u2f::MakeCredentialResponse> response);
  void OnGetAssertionResponse(
      CtapGetAssertionRequest request,
      GetAssertionCallback callback,
      absl::optional<u2f::GetAssertionResponse> response);
  void OnHasLegacyCredentialsResponse(
      base::OnceCallback<void(bool has_credential)> callback,
      absl::optional<u2f::HasCredentialsResponse> response);
  void OnCancelResponse(
      absl::optional<u2f::CancelWebAuthnFlowResponse> response);

  // Current request_id, used for cancelling the request.
  uint32_t current_request_id_ = 0u;

  // Callback to set request_id in the window property.
  base::RepeatingCallback<uint32_t()> generate_request_id_callback_;
  const Config config_;
  base::WeakPtrFactory<ChromeOSAuthenticator> weak_factory_;
};

}  // namespace device

#endif  // DEVICE_FIDO_CROS_AUTHENTICATOR_H_
