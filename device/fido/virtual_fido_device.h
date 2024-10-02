// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_VIRTUAL_FIDO_DEVICE_H_
#define DEVICE_FIDO_VIRTUAL_FIDO_DEVICE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_device.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/large_blob.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "third_party/boringssl/src/include/openssl/base.h"

namespace crypto {
class ECPrivateKey;
}

namespace device {

struct PublicKey;

constexpr size_t kMaxPinRetries = 8;

constexpr size_t kMaxUvRetries = 5;

class COMPONENT_EXPORT(DEVICE_FIDO) VirtualFidoDevice : public FidoDevice {
 public:
  // PrivateKey abstracts over the private key types supported by the virtual
  // authenticator.
  class COMPONENT_EXPORT(DEVICE_FIDO) PrivateKey {
   public:
    // FromPKCS8 attempts to parse |pkcs8_private_key| as an ASN.1, DER, PKCS#8
    // private key of a supported type and returns a |PrivateKey| instance
    // representing that key.
    static std::optional<std::unique_ptr<PrivateKey>> FromPKCS8(
        base::span<const uint8_t> pkcs8_private_key);

    // FreshP256Key returns a randomly generated P-256 PrivateKey.
    static std::unique_ptr<PrivateKey> FreshP256Key();

    // FreshRSAKey returns a randomly generated RSA PrivateKey.
    static std::unique_ptr<PrivateKey> FreshRSAKey();

    // FreshEd25519Key returns a randomly generated Ed25519 PrivateKey.
    static std::unique_ptr<PrivateKey> FreshEd25519Key();

    // FreshInvalidForTestingKey returns a dummy |PrivateKey| with a special
    // algorithm number that is used to test that unknown public keys are
    // handled correctly.
    static std::unique_ptr<PrivateKey> FreshInvalidForTestingKey();

    virtual ~PrivateKey();

    // Sign returns a signature over |message|.
    virtual std::vector<uint8_t> Sign(base::span<const uint8_t> message) = 0;

    // GetX962PublicKey returns the elliptic-curve public key encoded in X9.62
    // format. Only elliptic-curve based private keys can be represented in this
    // format and calling this function on other types of keys will crash.
    virtual std::vector<uint8_t> GetX962PublicKey() const;

    // GetPKCS8PrivateKey returns the private key encoded in ASN.1, DER, PKCS#8
    // format.
    virtual std::vector<uint8_t> GetPKCS8PrivateKey() const = 0;

    virtual std::unique_ptr<PublicKey> GetPublicKey() const = 0;
  };

  // Encapsulates information corresponding to one registered key on the virtual
  // authenticator device.
  struct COMPONENT_EXPORT(DEVICE_FIDO) RegistrationData {
    RegistrationData();
    explicit RegistrationData(const std::string& rp_id);
    RegistrationData(
        std::unique_ptr<PrivateKey> private_key,
        base::span<const uint8_t, kRpIdHashLength> application_parameter,
        uint32_t counter);

    RegistrationData(RegistrationData&& data);
    RegistrationData& operator=(RegistrationData&& other);

    RegistrationData(const RegistrationData&) = delete;
    RegistrationData& operator=(const RegistrationData&) = delete;

    ~RegistrationData();

    std::unique_ptr<PrivateKey> private_key = PrivateKey::FreshP256Key();
    std::array<uint8_t, kRpIdHashLength> application_parameter;
    uint32_t counter = 0;
    bool is_resident = false;
    bool backup_eligible = false;
    bool backup_state = false;
    // is_u2f is true if the credential was created via a U2F interface.
    bool is_u2f = false;
    device::CredProtect protection = device::CredProtect::kUVOptional;

    // user is only valid if |is_resident| is true.
    std::optional<device::PublicKeyCredentialUserEntity> user;
    // rp is only valid if |is_resident| is true.
    std::optional<device::PublicKeyCredentialRpEntity> rp;

    // hmac_key is present iff the credential has the hmac_secret extension
    // enabled. The first element of the pair is the HMAC key for non-UV, and
    // the second for when UV is used.
    std::optional<std::pair<std::array<uint8_t, 32>, std::array<uint8_t, 32>>>
        hmac_key;

    // large_blob stores associated large blob data when the largeBlob extension
    // is used. It is not pertinent when the largeBlob command and largeBlobKey
    // extension are used.
    std::optional<LargeBlob> large_blob;
    std::optional<std::array<uint8_t, 32>> large_blob_key;
    std::optional<std::vector<uint8_t>> cred_blob;
  };

  using Credential = std::pair<base::span<const uint8_t>, RegistrationData*>;

  class COMPONENT_EXPORT(DEVICE_FIDO) Observer : public base::CheckedObserver {
   public:
    virtual void OnCredentialCreated(const Credential& credential) = 0;
    virtual void OnCredentialDeleted(
        base::span<const uint8_t> credential_id) = 0;
    virtual void OnCredentialUpdated(const Credential& credential) = 0;
    virtual void OnAssertion(const Credential& credential) = 0;
  };

  // Stores the state of the device. Since |U2fDevice| objects only persist for
  // the lifetime of a single request, keeping state in an external object is
  // necessary in order to provide continuity between requests.
  class COMPONENT_EXPORT(DEVICE_FIDO) State : public base::RefCounted<State> {
   public:
    using RegistrationsMap = std::map<std::vector<uint8_t>,
                                      RegistrationData,
                                      fido_parsing_utils::RangeLess>;
    using SimulatePressCallback =
        base::RepeatingCallback<bool(VirtualFidoDevice*)>;

    State();

    State(const State&) = delete;
    State& operator=(const State&) = delete;

    // The common name in the attestation certificate.
    std::string attestation_cert_common_name;

    // The common name in the attestation certificate if individual attestation
    // is requested.
    std::string individual_attestation_cert_common_name;

    // Registered keys. Keyed on key handle (a.k.a. "credential ID").
    RegistrationsMap registrations;

    // If set, this callback is called whenever a "press" is required. Returning
    // `true` will simulate a press and continue the request, returning `false`
    // simulates the user not pressing the device and leaves the request idle.
    SimulatePressCallback simulate_press_callback;

    // If true, causes the response from the device to be invalid.
    bool simulate_invalid_response = false;

    // If true, return a packed self-attestation rather than a generated
    // certificate. This only has an effect for a CTAP2 device as
    // self-attestation is not defined for CTAP1.
    bool self_attestation = false;

    // Only valid if |self_attestation| is true. Causes the AAGUID to be non-
    // zero, in violation of the rules for self-attestation.
    bool non_zero_aaguid_with_self_attestation = false;

    // u2f_invalid_signature causes the signature in an assertion response to be
    // invalid. (U2F only.)
    bool u2f_invalid_signature = false;

    // u2f_invalid_public_key causes the public key in a registration response
    // to be invalid. (U2F only.)
    bool u2f_invalid_public_key = false;

    // ctap2_invalid_signature causes a bogus signature to be returned if true.
    bool ctap2_invalid_signature = false;
    // If true, UV bit is always set to 0 in the response.
    bool unset_uv_bit = false;
    // If true, UP bit is always set to 0 in the response.
    bool unset_up_bit = false;
    // default_backup_eligibility determines the default value of the
    // credential's BE (Backup Eligible) flag. This applies to both credentials
    // created by invoking the CTAP make credential command (in which case the
    // BE flag will also be reflected on make credential returned authenticator
    // data) and by calling one of the |Inject*| functions below.
    bool default_backup_eligibility = false;
    // default_backup_state determines the default value of the credential's BS
    // (Backup State) flag. This applies to both credentials created by invoking
    // the CTAP make credential command (in which case the BS flag will also be
    // reflected on make credential returned authenticator data) and by calling
    // one of the |Inject*| functions below.
    bool default_backup_state = false;

    // Number of PIN retries remaining.
    int pin_retries = kMaxPinRetries;
    // The number of failed PIN attempts since the token was "inserted".
    int pin_retries_since_insertion = 0;
    // True if the token is soft-locked due to too many failed PIN attempts
    // since "insertion".
    bool soft_locked = false;
    // The PIN for the device, or an empty string if no PIN is set.
    std::string pin;
    // The elliptic-curve key. (Not expected to be set externally.)
    bssl::UniquePtr<EC_KEY> ecdh_key;
    // The random PIN token that is returned as a placeholder for the PIN
    // itself.
    uint8_t pin_token[32];
    // The permissions parameter for |pin_token|.
    uint8_t pin_uv_token_permissions = 0;
    // The permissions RPID for |pin_token|.
    std::optional<std::string> pin_uv_token_rpid;
    // If true, fail all PinUvAuthToken requests until a new PIN is set.
    bool force_pin_change = false;
    // The minimum PIN length as unicode code points.
    uint32_t min_pin_length = kMinPinLength;

    // Number of internal UV retries remaining.
    int uv_retries = kMaxUvRetries;

    // Whether a device with internal-UV support has fingerprints enrolled.
    bool fingerprints_enrolled = false;

    // Whether a device with bio enrollment support has been provisioned.
    bool bio_enrollment_provisioned = false;

    // Current template ID being enrolled, if any.
    std::optional<uint8_t> bio_current_template_id;

    // Number of remaining samples in current enrollment.
    uint8_t bio_remaining_samples = 4;

    // Backing storage for enrollments and their friendly names.
    std::map<uint8_t, std::string> bio_templates;

    // Whether the next authenticatorBioEnrollment command with a
    // enrollCaptureNextSample subCommand should return a
    // CTAP2_ENROLL_FEEDBACK_TOO_HIGH response. Will be reset to false upon
    // returning the error.
    bool bio_enrollment_next_sample_error = false;

    // Whether the next authenticatorBioEnrollment command with a
    // enrollCaptureNextSample subCommand should return a
    // CTAP2_ENROLL_FEEDBACK_NO_USER_ACTIVITY response. Will be reset to false
    // upon returning the error.
    bool bio_enrollment_next_sample_timeout = false;

    // allow_list_history contains the allow_list values that have been seen in
    // assertion requests. This is for tests to confirm that the expected
    // sequence of requests was sent.
    std::vector<std::vector<PublicKeyCredentialDescriptor>> allow_list_history;

    // exclude_list_history contains the exclude_list values that have been seen
    // in registration requests. This is for tests to confirm that the expected
    // sequence of requests was sent.
    std::vector<std::vector<PublicKeyCredentialDescriptor>>
        exclude_list_history;

    // |cancel_response_code| is the response code the authenticator will return
    // when cancelling a pending request. Normally authenticators return
    // CTAP2_ERR_KEEP_ALIVE_CANCEL, but some authenticators incorrectly return
    // other codes.
    CtapDeviceResponseCode cancel_response_code =
        CtapDeviceResponseCode::kCtap2ErrKeepAliveCancel;

    // The large-blob array.
    std::vector<uint8_t> large_blob;

    FidoTransportProtocol transport =
        FidoTransportProtocol::kUsbHumanInterfaceDevice;

    // transact_callback contains the outstanding callback in the event that
    // |simulate_press_callback| returned false. This can be used to inject a
    // response after simulating an unsatisfied touch for CTAP2 authenticators.
    FidoDevice::DeviceCallback transact_callback;

    // device_id_override can be used to inject a return value for `GetId()` in
    // unit tests where a stable device identifier is required.
    std::optional<std::string> device_id_override;

    // Observer methods.
    void AddObserver(Observer* observer);
    void RemoveObserver(Observer* observer);
    void NotifyCredentialCreated(
        const std::pair<base::span<const uint8_t>, RegistrationData*>&
            credential);
    void NotifyCredentialDeleted(base::span<const uint8_t> credential_id);
    void NotifyCredentialUpdated(
        const std::pair<base::span<const uint8_t>, RegistrationData*>&
            credential);
    void NotifyAssertion(const std::pair<base::span<const uint8_t>,
                                         RegistrationData*>& credential);

    // Adds a new credential to the authenticator. Returns true on success,
    // false if there already exists a credential with the given ID.
    bool InjectRegistration(base::span<const uint8_t> credential_id,
                            RegistrationData registration);

    // Adds a registration for the specified credential ID with the application
    // parameter set to be valid for the given relying party ID (which would
    // typically be a domain, e.g. "example.com").
    //
    // Returns true on success. Will fail if there already exists a credential
    // with the given ID or if private-key generation fails.
    bool InjectRegistration(base::span<const uint8_t> credential_id,
                            const std::string& relying_party_id);

    // Adds a resident credential with the specified values.
    // Returns false if there already exists a resident credential for the same
    // (RP ID, user ID) pair, or for the same credential ID. Otherwise returns
    // true.
    bool InjectResidentKey(base::span<const uint8_t> credential_id,
                           device::PublicKeyCredentialRpEntity rp,
                           device::PublicKeyCredentialUserEntity user,
                           int32_t signature_counter,
                           std::unique_ptr<PrivateKey> private_key);

    // Adds a resident credential with the specified values, creating a new
    // private key.
    // Returns false if there already exists a resident credential for the same
    // (RP ID, user ID) pair, or for the same credential ID. Otherwise returns
    // true.
    bool InjectResidentKey(base::span<const uint8_t> credential_id,
                           device::PublicKeyCredentialRpEntity rp,
                           device::PublicKeyCredentialUserEntity user);

    // Version of InjectResidentKey that takes values for constructing an RP and
    // user entity.
    bool InjectResidentKey(base::span<const uint8_t> credential_id,
                           const std::string& relying_party_id,
                           base::span<const uint8_t> user_id,
                           std::optional<std::string> user_name,
                           std::optional<std::string> user_display_name);

    // Returns the large blob associated with the credential, if any.
    std::optional<LargeBlob> GetLargeBlob(const RegistrationData& credential);

    // Injects a large blob for the credential. If the credential already has an
    // associated large blob, replaces it. If the |large_blob| is malformed,
    // completely replaces its contents. (If `large_blob_extension_support` is
    // set then this method shouldn't be called. Just set the `large_blob`
    // member of `RegistrationData` directly.)
    void InjectLargeBlob(RegistrationData* credential, LargeBlob blob);

    // Injects an opaque large blob. |blob| does not need to conform to the CTAP
    // large-blob CBOR structure. (If `large_blob_extension_support` is set
    // then this method shouldn't be called.)
    void InjectOpaqueLargeBlob(cbor::Value blob);

    // Clears all large blobs resetting |large_blob| to its default value. (If
    // `large_blob_extension_support` is set then this method shouldn't be
    // called.)
    void ClearLargeBlobs();

   private:
    base::ObserverList<Observer> observers_;
    friend class base::RefCounted<State>;
    ~State();
  };

  // Constructs an object with ephemeral state. In order to have the state of
  // the device persist between operations, use the constructor that takes a
  // scoped_refptr<State>.
  VirtualFidoDevice();

  // Constructs an object that will read from, and write to, |state|.
  explicit VirtualFidoDevice(scoped_refptr<State> state);

  VirtualFidoDevice(const VirtualFidoDevice&) = delete;
  VirtualFidoDevice& operator=(const VirtualFidoDevice&) = delete;

  ~VirtualFidoDevice() override;

  State* mutable_state() const { return state_.get(); }

  // FidoDevice:
  std::string GetId() const override;

 protected:
  static std::vector<uint8_t> GetAttestationKey();

  scoped_refptr<State> NewReferenceToState() const { return state_; }

  static bool Sign(crypto::ECPrivateKey* private_key,
                   base::span<const uint8_t> sign_buffer,
                   std::vector<uint8_t>* signature);

  // Constructs certificate encoded in X.509 format to be used for packed
  // attestation statement and FIDO-U2F attestation statement.
  // https://w3c.github.io/webauthn/#defined-attestation-formats
  std::optional<std::vector<uint8_t>> GenerateAttestationCertificate(
      bool individual_attestation_requested,
      bool include_transports) const;

  void StoreNewKey(base::span<const uint8_t> key_handle,
                   VirtualFidoDevice::RegistrationData registration_data);

  RegistrationData* FindRegistrationData(
      base::span<const uint8_t> key_handle,
      base::span<const uint8_t, kRpIdHashLength> application_parameter);

  // Simulates flashing the device for a press and potentially receiving one.
  // Returns true if the "user" pressed the device (and the request must
  // continue) or false if the user didn't, and the request must be dropped.
  // Internally calls |state_->simulate_press_callback|, so |this| may be
  // destroyed after calling this method, in which case it will return false.
  bool SimulatePress();

  // FidoDevice:
  void TryWink(base::OnceClosure cb) override;
  FidoTransportProtocol DeviceTransport() const override;

 private:
  static std::string MakeVirtualFidoDeviceId();

  const std::string id_ = MakeVirtualFidoDeviceId();
  scoped_refptr<State> state_ = base::MakeRefCounted<State>();
};

}  // namespace device

#endif  // DEVICE_FIDO_VIRTUAL_FIDO_DEVICE_H_
