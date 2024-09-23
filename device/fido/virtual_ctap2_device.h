// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_VIRTUAL_CTAP2_DEVICE_H_
#define DEVICE_FIDO_VIRTUAL_CTAP2_DEVICE_H_

#include <stdint.h>

#include <list>
#include <memory>
#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "components/cbor/values.h"
#include "device/fido/attested_credential_data.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/authenticator_supported_options.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_types.h"
#include "device/fido/large_blob.h"
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
    bool prf_support = false;
    bool large_blob_support = false;
    // large_blob_extension_support indicates support for the single-extension
    // form of largeBlob. This form is implemented by hybrid authenticators and
    // is mutually exclusive with `large_blob_support`. If this value is
    // present then the extension will be implement, but if it's present with
    // the value false then the authenticator will report that makeCredential
    // didn't enable a large blob.
    std::optional<bool> large_blob_extension_support;
    // Support for setting a min PIN length and forcing pin change.
    bool min_pin_length_support = false;
    // min_pin_length_extension_support, if true, enables support for the
    // minPinLength extension. See
    // https://fidoalliance.org/specs/fido-v2.1-ps-20210615/fido-client-to-authenticator-protocol-v2.1-ps-20210615.html#sctn-minpinlength-extension
    bool min_pin_length_extension_support = false;
    bool always_uv = false;
    // always_uv_for_up_false applies the alwaysUv logic for getAssertion, even
    // when up=false. This does't seem correct, per CTAP 2.2, but some
    // authenticators do it.
    bool always_uv_for_up_false = false;
    // The space available to store a large blob. In real authenticators this
    // may change depending on the number of resident credentials. We treat this
    // as a fixed size area for the large blob.
    size_t available_large_blob_storage = kMinLargeBlobSize;
    bool cred_blob_support = false;
    // none_attestation causes a "none" attestation statement to be returned
    // from makeCredential calls.
    bool none_attestation = false;
    // include_transports_in_attestation_certificate controls whether a
    // transports extension will be included in the attestation certificate
    // returned from a makeCredential operation.
    bool include_transports_in_attestation_certificate = true;
    // transports_in_get_info, if not empty, contains the transports that will
    // be reported via getInfo. Otherwise no transports will be reported.
    std::vector<FidoTransportProtocol> transports_in_get_info;

    IncludeCredential include_credential_in_assertion_response =
        IncludeCredential::ONLY_IF_NEEDED;

    // force_cred_protect, if set and if |cred_protect_support| is true, is a
    // credProtect level that will be forced for all registrations. This
    // overrides any level requested in the makeCredential.
    std::optional<device::CredProtect> force_cred_protect;

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

    // Some authenticators will return CTAP2_ERR_NO_CREDENTIALS when enumerating
    // RPs if there are no credentials present. Setting this to `true` emulates
    // that behaviour.
    bool return_err_no_credentials_on_empty_rp_enumeration = false;

    // advertised_algorithms is the contents of the algorithms field in the
    // getInfo. If empty then no such field is reported. The virtual
    // authenticator only enables the algorithms listed here, unless the list is
    // empty in which case all algorithms except for |kInvalidForTesting| are
    // enabled.
    std::vector<device::CoseAlgorithmIdentifier> advertised_algorithms = {
        device::CoseAlgorithmIdentifier::kEs256,
    };

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

    // omit_user_entity_on_allow_credentials_requests causes get assertion
    // requests to omit the user entity for non empty allow lists, even if the
    // credential is discoverable. This matches the behaviour of some Android
    // devices.
    bool omit_user_entity_on_allow_credentials_requests = false;

    // pin_protocol is the PIN protocol version that this authenticator supports
    // and reports in the pinProtocols field of the authenticatorGetInfo
    // response.
    PINUVAuthProtocol pin_protocol = PINUVAuthProtocol::kV1;

    // internal_account_chooser indicates that the authenticator has a screen
    // and thus presents the account chooser for discoverable credential
    // assertions itself. This causes userSelected to be asserted on those
    // responses.
    bool internal_account_chooser = false;

    // override_response_map allows overriding the response for a given command
    // with a given code. The actual command won't be executed.
    base::flat_map<CtapRequestCommand, CtapDeviceResponseCode>
        override_response_map;

    // allow_non_resident_credential_creation_without_uv corresponds to the
    // make_cred_uv_not_required field in AuthenticatorSupportedOptions.
    bool allow_non_resident_credential_creation_without_uv = false;

    // reject_empty_display_name will cause a device to error out with
    // kCtap1ErrInvalidLength if the display name is present by empty, mirroring
    // the behaviour of some security keys.
    bool reject_empty_display_name = false;

    // reject_missing_display_name will cause a device to error out with
    // kCtap2ErrInvalidCBOR if the display name is not present, simulating the
    // behaviour of iPhones.
    bool reject_missing_display_name = false;
  };

  VirtualCtap2Device();
  VirtualCtap2Device(scoped_refptr<State> state, const Config& config);

  VirtualCtap2Device(const VirtualCtap2Device&) = delete;
  VirtualCtap2Device& operator=(const VirtualCtap2Device&) = delete;

  ~VirtualCtap2Device() override;

  // Configures and sets a PIN on the authenticator.
  void SetPin(std::string pin);

  // Sets whether to force a PIN change before accepting pinUvAuthToken
  // requests.
  void SetForcePinChange(bool force_pin_change);

  // Sets the minimum accepted PIN length.
  void SetMinPinLength(uint32_t min_pin_length);

  // FidoDevice:
  void Cancel(CancelToken) override;
  CancelToken DeviceTransact(std::vector<uint8_t> command,
                             DeviceCallback cb) override;
  base::WeakPtr<FidoDevice> GetWeakPtr() override;

 private:
  // RequestState encapsulates state for what CTAP 2.1 calls "stateful commands"
  // (https://drafts.fidoalliance.org/fido-2/latest/fido-client-to-authenticator-protocol-v2.1-rd-20210308.html#authenticator-api).
  struct RequestState {
    RequestState();
    RequestState(RequestState&) = delete;
    RequestState& operator=(RequestState&) = delete;
    ~RequestState();

    // Reset should be called at the beginning of every authenticator operation
    // that is not a direct continuation of another stateful operation. CTAP 2.1
    // specifies that authenticators may assume that stateful commands are never
    // interleaved by other operations.
    void Reset() {
      pending_assertions.clear();
      pending_rps.clear();
      pending_registrations.clear();
      large_blob_buffer.clear();
      large_blob_expected_next_offset = 0;
      large_blob_expected_length = 0;
    }

    // pending_assertions contains the second and subsequent assertions
    // resulting from a GetAssertion call. These values are awaiting a
    // GetNextAssertion request.
    std::vector<std::vector<uint8_t>> pending_assertions;

    // pending_rps contains the remaining RPs to return from a previous
    // authenticatorCredentialManagement/enumerateRPs command.
    std::list<device::PublicKeyCredentialRpEntity> pending_rps;

    // pending_registrations contains the remaining |is_resident| registrations
    // to return from a previous
    // authenticatorCredentialManagement/enumerateCredentials command.
    std::list<cbor::Value::MapValue> pending_registrations;

    // Buffer that gets progressively filled with large blob fragments until
    // committed.
    std::vector<uint8_t> large_blob_buffer;
    uint64_t large_blob_expected_next_offset = 0;
    uint64_t large_blob_expected_length = 0;
  };

  // Init performs initialization that's common across the constructors.
  void Init(std::vector<ProtocolVersion> versions);

  // CheckUserVerification implements the first, common steps of
  // makeCredential and getAssertion from the CTAP2 spec.
  enum class CheckUserVerificationMode {
    kGetAssertion,
    kMakeCredential,
    kMakeCredentialUvNotRequired,
  };
  std::optional<CtapDeviceResponseCode> CheckUserVerification(
      CheckUserVerificationMode mode,
      const AuthenticatorGetInfoResponse& authenticator_info,
      const std::string& rp_id,
      const std::optional<std::vector<uint8_t>>& pin_auth,
      const std::optional<PINUVAuthProtocol>& pin_protocol,
      base::span<const uint8_t> client_data_hash,
      UserVerificationRequirement user_verification,
      bool user_presence_required,
      bool* out_user_verified);
  std::optional<CtapDeviceResponseCode> OnMakeCredential(
      base::span<const uint8_t> request,
      std::vector<uint8_t>* response);
  std::optional<CtapDeviceResponseCode> OnGetAssertion(
      base::span<const uint8_t> request,
      std::vector<uint8_t>* response);
  CtapDeviceResponseCode OnGetNextAssertion(base::span<const uint8_t> request,
                                            std::vector<uint8_t>* response);
  std::optional<CtapDeviceResponseCode> OnPINCommand(
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

  size_t remaining_resident_credentials() const;
  bool SupportsAtLeast(Ctap2Version ctap2_version) const;

  std::unique_ptr<VirtualU2fDevice> u2f_device_;

  const Config config_;
  RequestState request_state_;
  base::WeakPtrFactory<FidoDevice> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_FIDO_VIRTUAL_CTAP2_DEVICE_H_
