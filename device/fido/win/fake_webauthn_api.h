// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_WIN_FAKE_WEBAUTHN_API_H_
#define DEVICE_FIDO_WIN_FAKE_WEBAUTHN_API_H_

#include <stdint.h>
#include <map>
#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "device/fido/fido_types.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "device/fido/virtual_fido_device.h"
#include "device/fido/win/webauthn_api.h"
#include "third_party/microsoft_webauthn/webauthn.h"

namespace device {

// FakeWinWebAuthnApi is a test fake to use instead of the real Windows WebAuthn
// API implemented by webauthn.dll.
//
// The fake supports injecting discoverable and non-discoverable credentials
// that can be challenged via AuthenticatorGetAssertion().
// AuthenticatorMakeCredential() returns a mock response and does not actually
// create a credential.
//
// Tests can inject a FakeWinWebAuthnApi via VirtualFidoDeviceFactory.
class COMPONENT_EXPORT(DEVICE_FIDO) FakeWinWebAuthnApi : public WinWebAuthnApi {
 public:
  using RegistrationData = VirtualFidoDevice::RegistrationData;

  FakeWinWebAuthnApi();
  ~FakeWinWebAuthnApi() override;

  // Injects a non-discoverable credential that can be challenged with
  // AuthenticatorGetAssertion().
  bool InjectNonDiscoverableCredential(base::span<const uint8_t> credential_id,
                                       const std::string& relying_party_id);

  // Injects a discoverable credential that can be challenged with
  // AuthenticatorGetAssertion().
  bool InjectDiscoverableCredential(base::span<const uint8_t> credential_id,
                                    device::PublicKeyCredentialRpEntity rp,
                                    device::PublicKeyCredentialUserEntity user);

  // Inject the return value for WinWebAuthnApi::IsAvailable().
  void set_available(bool available) { is_available_ = available; }

  // Injects an HRESULT to return from AuthenticatorMakeCredential() and
  // AuthenticatorGetAssertion(). If set to anything other than |S_OK|,
  // AuthenticatorGetAssertion() will immediately terminate the request with
  // that value and not return a WEBAUTHN_ASSERTION.
  void set_hresult(HRESULT result) { result_override_ = result; }

  // Inject the return value for
  // WinWebAuthnApi::IsUserverifyingPlatformAuthenticatorAvailable().
  void set_is_uvpaa(bool is_uvpaa) { is_uvpaa_ = is_uvpaa; }

  void set_supports_silent_discovery(bool supports_silent_discovery) {
    supports_silent_discovery_ = supports_silent_discovery;
  }

  // Overrides the result of large blob operations to return
  // |large_blob_result|. Setting this to anything other than
  // WEBAUTHN_CRED_LARGE_BLOB_STATUS_SUCCESS will prevent large blob operations
  // from making any changes.
  void set_large_blob_result(DWORD large_blob_result) {
    large_blob_result_ = large_blob_result;
  }

  // Sets whether large blob is reported to be supported on a make credential
  // request. This only has an effect for version >= 3.
  void set_large_blob_supported(bool supported) {
    large_blob_supported_ = supported;
  }

  void set_version(int version) { version_ = version; }

  // Returns a pointer to a copy of the last get credentials options passed to
  // the fake.
  WEBAUTHN_GET_CREDENTIALS_OPTIONS* last_get_credentials_options() {
    return last_get_credentials_options_.get();
  }

  // Returns a pointer to a copy of the last make credential options passed to
  // the fake.
  WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS*
  last_make_credential_options() {
    return last_make_credential_options_.get();
  }

  // Sets the transport to be reported by the API for cross-platform requests.
  void set_transport(int transport) { transport_ = transport; }

  // Sets the attachment to use when servicing a request with
  // attachment=undefined.
  void set_preferred_attachment(int preferred_attachment) {
    preferred_attachment_ = preferred_attachment;
  }

  // WinWebAuthnApi:
  bool IsAvailable() const override;
  bool SupportsSilentDiscovery() const override;
  HRESULT IsUserVerifyingPlatformAuthenticatorAvailable(
      BOOL* available) override;
  HRESULT AuthenticatorMakeCredential(
      HWND h_wnd,
      PCWEBAUTHN_RP_ENTITY_INFORMATION rp,
      PCWEBAUTHN_USER_ENTITY_INFORMATION user,
      PCWEBAUTHN_COSE_CREDENTIAL_PARAMETERS cose_credential_parameters,
      PCWEBAUTHN_CLIENT_DATA client_data,
      PCWEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS options,
      PWEBAUTHN_CREDENTIAL_ATTESTATION* credential_attestation_ptr) override;
  HRESULT AuthenticatorGetAssertion(
      HWND h_wnd,
      LPCWSTR rp_id,
      PCWEBAUTHN_CLIENT_DATA client_data,
      PCWEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS options,
      PWEBAUTHN_ASSERTION* assertion_ptr) override;
  HRESULT CancelCurrentOperation(GUID* cancellation_id) override;
  HRESULT GetPlatformCredentialList(
      PCWEBAUTHN_GET_CREDENTIALS_OPTIONS options,
      PWEBAUTHN_CREDENTIAL_DETAILS_LIST* credentials) override;
  HRESULT DeletePlatformCredential(
      base::span<const uint8_t> credential_id) override;
  PCWSTR GetErrorName(HRESULT hr) override;
  void FreeCredentialAttestation(PWEBAUTHN_CREDENTIAL_ATTESTATION) override;
  void FreeAssertion(PWEBAUTHN_ASSERTION pWebAuthNAssertion) override;
  void FreePlatformCredentialList(
      PWEBAUTHN_CREDENTIAL_DETAILS_LIST credentials) override;
  int Version() override;

 private:
  struct CredentialInfo;
  struct CredentialInfoList;
  struct WebAuthnAttestation;
  struct WebAuthnAssertionEx;

  static WEBAUTHN_CREDENTIAL_ATTESTATION FakeAttestation();

  bool is_available_ = true;
  bool is_uvpaa_ = false;
  bool supports_silent_discovery_ = false;
  int version_ = WEBAUTHN_API_VERSION_2;
  int transport_ = WEBAUTHN_CTAP_TRANSPORT_USB;
  int large_blob_result_ = WEBAUTHN_CRED_LARGE_BLOB_STATUS_SUCCESS;
  bool large_blob_supported_ = true;
  int preferred_attachment_ = WEBAUTHN_AUTHENTICATOR_ATTACHMENT_CROSS_PLATFORM;
  HRESULT result_override_ = S_OK;

  // Owns a copy of the last get credentials options to have been passed to the
  // fake.
  std::unique_ptr<WEBAUTHN_GET_CREDENTIALS_OPTIONS>
      last_get_credentials_options_;

  // Owns a copy of the last make credential options to have been passed to the
  // fake.
  std::unique_ptr<WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS>
      last_make_credential_options_;

  // Owns the attestations returned by AuthenticatorMakeCredential().
  std::vector<std::unique_ptr<WebAuthnAttestation>> returned_attestations_;

  // Owns assertions returned by AuthenticatorGetAssertion().
  std::vector<std::unique_ptr<WebAuthnAssertionEx>> returned_assertions_;

  // Owns lists of credentials returned by GetPlatformCredentialList().
  std::vector<std::unique_ptr<CredentialInfoList>> returned_credential_lists_;

  std::
      map<std::vector<uint8_t>, RegistrationData, fido_parsing_utils::RangeLess>
          registrations_;

  // A map of credential IDs to large blobs.
  base::flat_map<std::vector<uint8_t>, std::vector<uint8_t>> large_blobs_;
};

}  // namespace device

#endif  // DEVICE_FIDO_WIN_FAKE_WEBAUTHN_API_H_
