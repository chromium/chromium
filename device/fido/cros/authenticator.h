// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CROS_AUTHENTICATOR_H_
#define DEVICE_FIDO_CROS_AUTHENTICATOR_H_

#include <string>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
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
  explicit ChromeOSAuthenticator(
      base::RepeatingCallback<uint32_t()> generate_request_id_callback);
  ~ChromeOSAuthenticator() override;

  bool HasCredentialForGetAssertionRequest(
      const CtapGetAssertionRequest& request);

  // Returns whether the platform authenticator is available, which is true if
  // the current user has a PIN set up or biometrics enrolled.
  //
  // Since this call makes a (quick) dbus call, it is potentially blocking and
  // should not run on the main thread/sequence.
  //
  // TODO(crbug.com/1154063): Refactor IsUVPAA() to be async.
  static bool IsUVPlatformAuthenticatorAvailableBlocking();

  // FidoAuthenticator
  void InitializeAuthenticator(base::OnceClosure callback) override;
  void MakeCredential(CtapMakeCredentialRequest request,
                      MakeCredentialCallback callback) override;
  void GetAssertion(CtapGetAssertionRequest request,
                    CtapGetAssertionOptions options,
                    GetAssertionCallback callback) override;
  void GetNextAssertion(GetAssertionCallback callback) override {}
  void Cancel() override;
  std::string GetId() const override;
  base::string16 GetDisplayName() const override;
  const base::Optional<AuthenticatorSupportedOptions>& Options() const override;

  base::Optional<FidoTransportProtocol> AuthenticatorTransport() const override;

  bool IsInPairingMode() const override;
  bool IsPaired() const override;
  bool RequiresBlePairingPin() const override;

  bool IsChromeOSAuthenticator() const override;

  void GetTouch(base::OnceClosure callback) override {}
  base::WeakPtr<FidoAuthenticator> GetWeakPtr() override;

 private:
  void OnMakeCredentialResp(CtapMakeCredentialRequest request,
                            MakeCredentialCallback callback,
                            dbus::Response* dbus_response,
                            dbus::ErrorResponse* error);

  void OnGetAssertionResp(CtapGetAssertionRequest request,
                          GetAssertionCallback callback,
                          dbus::Response* dbus_response,
                          dbus::ErrorResponse* error);

  void OnCancelResp(dbus::Response* dbus_response);

  // Current request_id, used for cancelling the request.
  uint32_t current_request_id_ = 0u;

  // Callback to set request_id in the window property.
  base::RepeatingCallback<uint32_t()> generate_request_id_callback_;
  base::WeakPtrFactory<ChromeOSAuthenticator> weak_factory_;
};

}  // namespace device

#endif  // DEVICE_FIDO_CROS_AUTHENTICATOR_H_
