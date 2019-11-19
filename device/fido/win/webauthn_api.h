// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_WIN_WEBAUTHN_API_H_
#define DEVICE_FIDO_WIN_WEBAUTHN_API_H_

#include <windows.h>
#include <functional>
#include <memory>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/fido_constants.h"
#include "third_party/microsoft_webauthn/webauthn.h"

namespace device {

// WinWebAuthnApi is a wrapper for the native Windows WebAuthn API.
//
// The default singleton instance can be obtained by calling |GetDefault|.
// Users must check the result of |IsAvailable| on the instance to verify that
// the native library was loaded successfully before invoking any of the other
// methods.
class COMPONENT_EXPORT(DEVICE_FIDO) WinWebAuthnApi {
 public:
  // Returns the default implementation of WinWebAuthnApi backed by
  // webauthn.dll. May return nullptr if webauthn.dll cannot be loaded.
  static WinWebAuthnApi* GetDefault();

  virtual ~WinWebAuthnApi();

  // Returns whether the API is available on this system. If this returns
  // false, none of the other methods on this instance may be called.
  virtual bool IsAvailable() const = 0;

  virtual HRESULT IsUserVerifyingPlatformAuthenticatorAvailable(
      BOOL* available) = 0;

  virtual HRESULT AuthenticatorMakeCredential(
      HWND h_wnd,
      PCWEBAUTHN_RP_ENTITY_INFORMATION rp,
      PCWEBAUTHN_USER_ENTITY_INFORMATION user,
      PCWEBAUTHN_COSE_CREDENTIAL_PARAMETERS cose_credential_parameters,
      PCWEBAUTHN_CLIENT_DATA client_data,
      PCWEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS options,
      PWEBAUTHN_CREDENTIAL_ATTESTATION* credential_attestation_ptr) = 0;

  virtual HRESULT AuthenticatorGetAssertion(
      HWND h_wnd,
      LPCWSTR rp_id,
      PCWEBAUTHN_CLIENT_DATA client_data,
      PCWEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS options,
      PWEBAUTHN_ASSERTION* assertion_ptr) = 0;

  virtual HRESULT CancelCurrentOperation(GUID* cancellation_id) = 0;

  virtual PCWSTR GetErrorName(HRESULT hr) = 0;

  virtual void FreeCredentialAttestation(PWEBAUTHN_CREDENTIAL_ATTESTATION) = 0;

  virtual void FreeAssertion(PWEBAUTHN_ASSERTION pWebAuthNAssertion) = 0;

  virtual int Version() = 0;

 protected:
  WinWebAuthnApi();
};

std::pair<CtapDeviceResponseCode,
          base::Optional<AuthenticatorMakeCredentialResponse>>
AuthenticatorMakeCredentialBlocking(WinWebAuthnApi* webauthn_api,
                                    HWND h_wnd,
                                    GUID cancellation_id,
                                    CtapMakeCredentialRequest request);

std::pair<CtapDeviceResponseCode,
          base::Optional<AuthenticatorGetAssertionResponse>>
AuthenticatorGetAssertionBlocking(WinWebAuthnApi* webauthn_api,
                                  HWND h_wnd,
                                  GUID cancellation_id,
                                  CtapGetAssertionRequest request);

}  // namespace device

#endif  // DEVICE_FIDO_WIN_WEBAUTHN_API_H_
