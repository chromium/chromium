// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_VIRTUAL_CTAP2_DEVICE_H_
#define DEVICE_FIDO_VIRTUAL_CTAP2_DEVICE_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "components/cbor/values.h"
#include "device/fido/attested_credential_data.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/authenticator_supported_options.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/fido_constants.h"
#include "device/fido/virtual_fido_device.h"

namespace device {

class VirtualU2fDevice;

class COMPONENT_EXPORT(DEVICE_FIDO) VirtualCtap2Device
    : public VirtualFidoDevice {
 public:
  struct COMPONENT_EXPORT(DEVICE_FIDO) Config {
    // IncludeCredential enumerates possible behaviours when deciding whether to
    // return credential information in an assertion response.
    enum class IncludeCredential {
      // ONLY_IF_NEEDED causes the credential information to be included when
      // the
      // allowlist has zero or several entries.
      ONLY_IF_NEEDED,
      // ALWAYS causes credential information to always be returned. This is
      // a valid behaviour per the CTAP2 spec.
      ALWAYS,
      // NEVER causes credential information to never be returned. This is
      // invalid behaviour whenever the allowlist is not of length one.
      NEVER,
    };

    Config();
    Config(const Config&);
    Config& operator=(const Config&);
    ~Config();

    base::flat_set<Ctap2Version> ctap2_versions = {
        std::begin(kCtap2Versions2_0), std::end(kCtap2Versions2_0)};
    // u2f_support, if true, makes this device a dual-protocol (i.e. CTAP2 and
    // U2F) device.
    bool u2f_support = false;
    bool pin_support = false;
    bool is_platform_authenticator = false;
    bool internal_uv_support = false;
    bool pin_uv_auth_token_support = false;
    bool resident_key_support = false;
    bool credential_management_support = false;
    bool bio_enrollment_support = false;
    bool bio_enrollment_preview_support = false;
    uint8_t bio_enrollment_capacity = 10;
    uint8_t bio_enrollment_samples_required = 4;
    bool cred_protect_support = false;
    bool hmac_secret_support = false;
    bool large_blob_support = false;
    // The space available to store a large blob. In real authenticators this
    // may change depending on the number of resident credentials. We treat this
    // as a fixed size area for the large blob.
    size_t available_large_blob_storage = 1024;

    IncludeCredential include_credential_in_assertion_response =
        IncludeCredential::ONLY_IF_NEEDED;

    // force_cred_protect, if set and if |cred_protect_support| is true, is a
    // credProtect level that will be forced for all registrations. This
    // overrides any level requested in the makeCredential.
    base::Optional<device::CredProtect> force_cred_protect;

    // default_cred_protect, if |cred_protect_support| is true, is the
    // credProtect level that will be set for makeCredential requests that do
    // not specify one.
    device::CredProtect default_cred_protect = device::CredProtect::kUVOptional;

    // max_credential_count_in_list, if non-zero, is the value to return for
    // maxCredentialCountInList in the authenticatorGetInfo reponse.
    // CTAP2_ERR_LIMIT_EXCEEDED will be returned for requests with an allow or
    // exclude list exceeding this limit. Note that the request handler
    // implementations require maxCredentialIdLength be set in order for
    // maxCredentialCountInList to be respected.
    uint32_t max_credential_count_in_list = 0;

    // max_credential_id_length, if non-zero, is the value to return for
    // maxCredentialIdLength in the authenticatorGetInfo reponse.
    // CTAP2_ERR_LIMIT_EXCEEDED will be returned for requests with an allow or
    // exclude list containing a credential ID exceeding this limit.
    uint32_t max_credential_id_length = 0;

    // resident_credential_storage is the number of resident credentials that
    // the device will store before returning KEY_STORE_FULL.
    size_t resident_credential_storage = 3;

    // return_immediate_invalid_credential_error causes an INVALID_CREDENTIAL
    // error to be returned from GetAssertion, before getting a touch, when no
    // credentials are recognised. This behaviour is exhibited by some CTAP2
    // authenticators in the wild.
    bool return_immediate_invalid_credential_error = false;

    // return_attested_cred_data_in_get_assertion_response causes
    // attestedCredentialData to be included in the authenticator data of an
    // GetAssertion response.
    bool return_attested_cred_data_in_get_assertion_response = false;

    // reject_large_allow_and_exclude_lists causes the authenticator to respond
    // with an error if an allowList or an excludeList contains more than one
    // credential ID. This can be used to simulate errors with oversized
    // credential lists in an authenticator that does not support batching (i.e.
    // maxCredentialCountInList and maxCredentialIdSize).
    bool reject_large_allow_and_exclude_lists = false;

    // reject_silent_authenticator_requests causes the authenticator to return
    // an error if a up=false assertion request is received.
    bool reject_silent_authentication_requests = false;

    // Whether internal user verification should succeed or not.
    bool user_verification_succeeds = true;

    // allow_invalid_utf8_in_credential_entities indicates whether
    // InjectResidentKey() may be called with a PublicKeyCredentialRpEntity and
    // PublicKeyCredentialUserEntity containing a trailing partial UTF-8
    // sequence. This is used to simulate a security key that truncates strings
    // at a pre-defined byte length without concern for UTF-8 validity of the
    // result.
    bool allow_invalid_utf8_in_credential_entities = false;

    // add_extra_extension causes an unsolicited extension to be added in the
    // authenticator extensions output.
    bool add_extra_extension = false;

    // reject_all_extensions causes the authenticator to return a CTAP error if
    // a makeCredential or getAssertion request carries any extension.
    bool reject_all_extensions = false;

    // Support a non-standard CTAP extension that lets the platform supply an
    // unhashed client data for the authenticator to assemble and hash instead
    // of using the regular, already hashed value.
    bool support_android_client_data_extension = false;

    // Support a non-standard CTAP extension that lets the platform supply an
    // unhashed client data for the authenticator to assemble and hash instead
    // of using the regular, already hashed value.
    bool send_unsolicited_android_client_data_extension = false;

    // support_invalid_for_testing_algorithm causes the
    // |CoseAlgorithmIdentifier::kInvalidForTesting| public-key algorithm to be
    // advertised and supported to aid testing of unknown public-key types.
    bool support_invalid_for_testing_algorithm = false;

    // support_enterprise_attestation indicates whether enterprise attestation
    // support will be advertised in the getInfo response and whether requests
    // will be honored during makeCredential.
    bool support_enterprise_attestation = false;

    // always_return_enterprise_attestation causes the authenticator to,
    // invalidly, always signal that the returned attestation is an enterprise
    // attestation, even when it wasn't requested.
    bool always_return_enterprise_attestation = false;

    // enterprise_attestation_rps enumerates the RP IDs that will trigger
    // enterprise attestation when the platform requests ep=1.
    std::vector<std::string> enterprise_attestation_rps;

    // ignore_u2f_credentials causes credentials created over the
    // authenticator's U2F interface not to be available over CTAP2 for
    // assertions.
    bool ignore_u2f_credentials = false;
  };

  VirtualCtap2Device();
  VirtualCtap2Device(scoped_refptr<State> state, const Config& config);
  ~VirtualCtap2Device() override;

  // Configures and sets a PIN on the authenticator.
  void SetPin(std::string pin);

  // FidoDevice:
  void Cancel(CancelToken) override;
  CancelToken DeviceTransact(std::vector<uint8_t> command,
                             DeviceCallback cb) override;
  base::WeakPtr<FidoDevice> GetWeakPtr() override;

 private:
  // Init performs initialization that's common across the constructors.
  void Init(std::vector<ProtocolVersion> versions);

  // CheckUserVerification implements the first, common steps of
  // makeCredential and getAssertion from the CTAP2 spec.
  base::Optional<CtapDeviceResponseCode> CheckUserVerification(
      bool is_make_credential,
      const AuthenticatorSupportedOptions& options,
      const std::string& rp_id,
      const base::Optional<std::vector<uint8_t>>& pin_auth,
      const base::Optional<uint8_t>& pin_protocol,
      base::span<const uint8_t> pin_token,
      base::span<const uint8_t> client_data_hash,
      UserVerificationRequirement user_verification,
      bool* out_user_verified);
  base::Optional<CtapDeviceResponseCode> OnMakeCredential(
      base::span<const uint8_t> request,
      std::vector<uint8_t>* response);
  base::Optional<CtapDeviceResponseCode> OnGetAssertion(
      base::span<const uint8_t> request,
      std::vector<uint8_t>* response);
  CtapDeviceResponseCode OnGetNextAssertion(base::span<const uint8_t> request,
                                            std::vector<uint8_t>* response);
  base::Optional<CtapDeviceResponseCode> OnPINCommand(
      base::span<const uint8_t> request,
      std::vector<uint8_t>* response);
  CtapDeviceResponseCode OnCredentialManagement(
      base::span<const uint8_t> request,
      std::vector<uint8_t>* response);
  CtapDeviceResponseCode OnBioEnrollment(base::span<const uint8_t> request,
                                         std::vector<uint8_t>* response);
  CtapDeviceResponseCode OnLargeBlobs(base::span<const uint8_t> request,
                                      std::vector<uint8_t>* response);
  CtapDeviceResponseCode OnAuthenticatorGetInfo(
      std::vector<uint8_t>* response) const;

  void InitPendingRPs();
  void GetNextRP(cbor::Value::MapValue* response_map);
  void InitPendingRegistrations(base::span<const uint8_t> rp_id_hash);
  void RegenerateKeyAgreementKey();

  AttestedCredentialData ConstructAttestedCredentialData(
      base::span<const uint8_t> key_handle,
      std::unique_ptr<PublicKey> public_key);

  std::unique_ptr<VirtualU2fDevice> u2f_device_;

  const Config config_;
  base::WeakPtrFactory<FidoDevice> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(VirtualCtap2Device);
};

}  // namespace device

#endif  // DEVICE_FIDO_VIRTUAL_CTAP2_DEVICE_H_
