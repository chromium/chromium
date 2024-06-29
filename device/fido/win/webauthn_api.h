// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_WIN_WEBAUTHN_API_H_
#define DEVICE_FIDO_WIN_WEBAUTHN_API_H_

#include <windows.h>

#include <functional>
#include <memory>
#include <optional>
#include <string_view>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/fido_authenticator.h"
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
  // ScopedOverride, while in scope, overrides the result of `GetDefault`,
  // below. Only one can be in scope at a time.
  class COMPONENT_EXPORT(DEVICE_FIDO) ScopedOverride {
   public:
    explicit ScopedOverride(WinWebAuthnApi* api);
    ~ScopedOverride();

    ScopedOverride(const ScopedOverride&) = delete;
    ScopedOverride(const ScopedOverride&&) = delete;
    ScopedOverride& operator=(const ScopedOverride&) = delete;
    ScopedOverride& operator=(const ScopedOverride&&) = delete;
  };

  // Returns the default implementation of WinWebAuthnApi backed by
  // webauthn.dll. May return nullptr if webauthn.dll cannot be loaded.
  static WinWebAuthnApi* GetDefault();

  virtual ~WinWebAuthnApi();

  // Returns whether the API is available on this system. If this returns
  // false, none of the other methods on this instance may be called.
  virtual bool IsAvailable() const = 0;

  // Returns whether the API is available and supports the following methods:
  //   |GetPlatformCredentialList|
  //   |FreePlatformCredentialList|
  //   |DeletePlatformCredential|
  //
  // This should be preferred to checking the API version.
  virtual bool SupportsSilentDiscovery() const = 0;

  // Returns true if webauthn.dll supports hybrid.
  bool SupportsHybrid();

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

  virtual HRESULT GetPlatformCredentialList(
      PCWEBAUTHN_GET_CREDENTIALS_OPTIONS options,
      PWEBAUTHN_CREDENTIAL_DETAILS_LIST* credentials) = 0;

  virtual HRESULT DeletePlatformCredential(
      base::span<const uint8_t> credential_id) = 0;

  virtual PCWSTR GetErrorName(HRESULT hr) = 0;

  virtual void FreeCredentialAttestation(PWEBAUTHN_CREDENTIAL_ATTESTATION) = 0;

  virtual void FreeAssertion(PWEBAUTHN_ASSERTION pWebAuthNAssertion) = 0;

  virtual void FreePlatformCredentialList(
      PWEBAUTHN_CREDENTIAL_DETAILS_LIST credentials) = 0;

  virtual int Version() = 0;

 protected:
  WinWebAuthnApi();
};

std::pair<MakeCredentialStatus,
          std::optional<AuthenticatorMakeCredentialResponse>>
AuthenticatorMakeCredentialBlocking(WinWebAuthnApi* webauthn_api,
                                    HWND h_wnd,
                                    GUID cancellation_id,
                                    CtapMakeCredentialRequest request,
                                    MakeCredentialOptions request_options);

std::pair<GetAssertionStatus, std::optional<AuthenticatorGetAssertionResponse>>
AuthenticatorGetAssertionBlocking(WinWebAuthnApi* webauthn_api,
                                  HWND h_wnd,
                                  GUID cancellation_id,
                                  CtapGetAssertionRequest request,
                                  CtapGetAssertionOptions request_options);

// Returns a list of credentials known to the platform authenticator. If rp_id
// is non-null, only credentials scoped to a matching RP ID are returned.
//
// Returns a boolean indicating success, which will be false if the API doesn't
// support enumeration or if an unexpected error occurred (but will be true if
// there are no matching credentials); and the list of matching credentials, if
// any.
std::pair<bool, std::vector<DiscoverableCredentialMetadata>>
AuthenticatorEnumerateCredentialsBlocking(WinWebAuthnApi* webauthn_api,
                                          std::u16string_view rp_id,
                                          bool is_incognito);

}  // namespace device

#endif  // DEVICE_FIDO_WIN_WEBAUTHN_API_H_
