// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_AUTHENTICATOR_REQUEST_CLIENT_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_AUTHENTICATOR_REQUEST_CLIENT_DELEGATE_H_

#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"

#if defined(OS_MACOSX)
#include "device/fido/mac/authenticator_config.h"
#endif

namespace device {
class FidoAuthenticator;
}

namespace url {
class Origin;
}

namespace content {

// Interface that the embedder should implement to provide the //content layer
// with embedder-specific configuration for a single Web Authentication API [1]
// request serviced in a given RenderFrame.
//
// [1]: See https://www.w3.org/TR/webauthn/.
class CONTENT_EXPORT AuthenticatorRequestClientDelegate
    : public device::FidoRequestHandlerBase::Observer {
 public:
  // Failure reasons that might be of interest to the user, so the embedder may
  // decide to inform the user.
  enum class InterestingFailureReason {
    kTimeout,
    kKeyNotRegistered,
    kKeyAlreadyRegistered,
    kSoftPINBlock,
    kHardPINBlock,
    kAuthenticatorRemovedDuringPINEntry,
    kAuthenticatorMissingResidentKeys,
    kAuthenticatorMissingUserVerification,
    // kStorageFull indicates that a resident credential could not be created
    // because the authenticator has insufficient storage.
    kStorageFull,
    kUserConsentDenied,
  };

  AuthenticatorRequestClientDelegate();
  ~AuthenticatorRequestClientDelegate() override;

  // Called when the request fails for the given |reason|.
  //
  // Embedders may return true if they want AuthenticatorImpl to hold off from
  // resolving the WebAuthn request with an error, e.g. because they want the
  // user to dismiss an error dialog first. In this case, embedders *must*
  // eventually invoke the FidoRequestHandlerBase::CancelCallback in order to
  // resolve the request. Returning false causes AuthenticatorImpl to resolve
  // the request with the error right away.
  virtual bool DoesBlockRequestOnFailure(InterestingFailureReason reason);

  // Supplies callbacks that the embedder can invoke to initiate certain
  // actions, namely: cancel the request, start the request over, initiate BLE
  // pairing process, cancel WebAuthN request, and dispatch request to connected
  // authenticators.
  virtual void RegisterActionCallbacks(
      base::OnceClosure cancel_callback,
      base::Closure start_over_callback,
      device::FidoRequestHandlerBase::RequestCallback request_callback,
      base::RepeatingClosure bluetooth_adapter_power_on_callback,
      device::FidoRequestHandlerBase::BlePairingCallback ble_pairing_callback);

  // Returns true if the given relying party ID is permitted to receive
  // individual attestation certificates. This:
  //  a) triggers a signal to the security key that returning individual
  //     attestation certificates is permitted, and
  //  b) skips any permission prompt for attestation.
  virtual bool ShouldPermitIndividualAttestation(
      const std::string& relying_party_id);

  // Invokes |callback| with |true| if the given relying party ID is permitted
  // to receive attestation certificates from the provided FidoAuthenticator.
  // Otherwise invokes |callback| with |false|.
  //
  // Since these certificates may uniquely identify the authenticator, the
  // embedder may choose to show a permissions prompt to the user, and only
  // invoke |callback| afterwards. This may hairpin |callback|.
  virtual void ShouldReturnAttestation(
      const std::string& relying_party_id,
      const device::FidoAuthenticator* authenticator,
      base::OnceCallback<void(bool)> callback);

  // SupportsResidentKeys returns true if this implementation of
  // |AuthenticatorRequestClientDelegate| supports resident keys. If false then
  // requests to create or get assertions will be immediately rejected and
  // |SelectAccount| will never be called.
  virtual bool SupportsResidentKeys();

  // SetMightCreateResidentCredential indicates whether activating an
  // authenticator may cause a resident credential to be created. A resident
  // credential may be discovered by someone with physical access to the
  // authenticator and thus has privacy implications.
  void SetMightCreateResidentCredential(bool v) override;

  // ShouldPermitCableExtension returns true if the given |origin| may set a
  // caBLE extension. This extension contains website-chosen BLE pairing
  // information that will be broadcast by the device and so should not be
  // accepted if the embedder UI does not indicate that this is happening.
  virtual bool ShouldPermitCableExtension(const url::Origin& origin);

  // SetCableTransportInfo configures the embedder for handling Cloud-assisted
  // Bluetooth Low Energy transports (i.e. using a phone as an authenticator).
  // The |cable_extension_provided| argument is true if the site provided
  // explicit caBLE discovery information. This is a hint that the UI may wish
  // to advance to directly to guiding the user to check their phone as the site
  // is strongly indicating that it will work.
  //
  // have_paired_phones is true if a previous call to |GetCablePairings|
  // returned one or more caBLE pairings.
  //
  // |qr_generator_key| is a random AES-256 key that can be used to
  // encrypt a coarse timestamp with |CableDiscoveryData::DeriveQRKeyMaterial|.
  // The UI may display a QR code with the resulting secret which, if
  // decoded and transmitted over BLE by an authenticator, will be accepted for
  // caBLE pairing.
  //
  // This function returns true if the embedder will provide UI support for
  // caBLE. If it returns false, all caBLE will be disabled because BLE
  // broadcasting should not occur without user notification and accepting QR
  // handshakes is irrelevant if the UI is not displaying the QR codes.
  virtual bool SetCableTransportInfo(
      bool cable_extension_provided,
      bool have_paired_phones,
      base::Optional<device::QRGeneratorKey> qr_generator_key);

  // GetCablePairings returns any known caBLE pairing data. For example, the
  // embedder may know of pairings because it configured the
  // |FidoDiscoveryFactory| (using |CustomizeDiscoveryFactory|) to make a
  // callback when a phone offered long-term pairing data. Additionally, it may
  // know of pairings via some cloud-based service or sync feature.
  virtual std::vector<device::CableDiscoveryData> GetCablePairings();

  // SelectAccount is called to allow the embedder to select between one or more
  // accounts. This is triggered when the web page requests an unspecified
  // credential (by passing an empty allow-list). In this case, any accounts
  // will come from the authenticator's storage and the user should confirm the
  // use of any specific account before it is returned. The callback takes the
  // selected account, or else |cancel_callback| can be called.
  //
  // This is only called if |SupportsResidentKeys| returns true.
  virtual void SelectAccount(
      std::vector<device::AuthenticatorGetAssertionResponse> responses,
      base::OnceCallback<void(device::AuthenticatorGetAssertionResponse)>
          callback);

  // Returns whether the WebContents corresponding to |render_frame_host| is the
  // active tab in the focused window. We do not want to allow
  // authenticatorMakeCredential operations to be triggered by background tabs.
  //
  // Note that the default implementation of this function, and the
  // implementation in ChromeContentBrowserClient for Android, return |true| so
  // that testing is possible.
  virtual bool IsFocused();

#if defined(OS_MACOSX)
  using TouchIdAuthenticatorConfig = device::fido::mac::AuthenticatorConfig;

  // Returns configuration data for the built-in Touch ID platform
  // authenticator. May return nullopt if the authenticator is not used or not
  // available.
  virtual base::Optional<TouchIdAuthenticatorConfig>
  GetTouchIdAuthenticatorConfig();
#endif  // defined(OS_MACOSX)

  // Returns true if a user verifying platform authenticator is available and
  // configured.
  virtual bool IsUserVerifyingPlatformAuthenticatorAvailable();

  // Returns a FidoDiscoveryFactory that has been configured for the current
  // environment.
  virtual device::FidoDiscoveryFactory* GetDiscoveryFactory();

  // Saves transport type the user used during WebAuthN API so that the
  // WebAuthN UI will default to the same transport type during next API call.
  virtual void UpdateLastTransportUsed(device::FidoTransportProtocol transport);

  // Disables the UI (needed in cases when called by other components, like
  // cryptotoken).
  virtual void DisableUI();

  virtual bool IsWebAuthnUIEnabled();

  // device::FidoRequestHandlerBase::Observer:
  void OnTransportAvailabilityEnumerated(
      device::FidoRequestHandlerBase::TransportAvailabilityInfo data) override;
  // If true, the request handler will defer dispatch of its request onto the
  // given authenticator to the embedder. The embedder needs to call
  // |StartAuthenticatorRequest| when it wants to initiate request dispatch.
  //
  // This method is invoked before |FidoAuthenticatorAdded|, and may be
  // invoked multiple times for the same authenticator. Depending on the
  // result, the request handler might decide not to make the authenticator
  // available, in which case it never gets passed to
  // |FidoAuthenticatorAdded|.
  bool EmbedderControlsAuthenticatorDispatch(
      const device::FidoAuthenticator& authenticator) override;
  void BluetoothAdapterPowerChanged(bool is_powered_on) override;
  void FidoAuthenticatorAdded(
      const device::FidoAuthenticator& authenticator) override;
  void FidoAuthenticatorRemoved(base::StringPiece device_id) override;
  void FidoAuthenticatorIdChanged(base::StringPiece old_authenticator_id,
                                  std::string new_authenticator_id) override;
  void FidoAuthenticatorPairingModeChanged(
      base::StringPiece authenticator_id,
      bool is_in_pairing_mode,
      base::string16 display_name) override;
  bool SupportsPIN() const override;
  void CollectPIN(
      base::Optional<int> attempts,
      base::OnceCallback<void(std::string)> provide_pin_cb) override;
  void FinishCollectPIN() override;

 protected:
  // CustomizeDiscoveryFactory may be overridden in order to configure
  // |discovery_factory|.
  virtual void CustomizeDiscoveryFactory(
      device::FidoDiscoveryFactory* discovery_factory);

 private:
#if !defined(OS_ANDROID)
  std::unique_ptr<device::FidoDiscoveryFactory> discovery_factory_;
#endif  // !defined(OS_ANDROID)

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorRequestClientDelegate);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_AUTHENTICATOR_REQUEST_CLIENT_DELEGATE_H_
