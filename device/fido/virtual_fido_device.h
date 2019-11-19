// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_VIRTUAL_FIDO_DEVICE_H_
#define DEVICE_FIDO_VIRTUAL_FIDO_DEVICE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "crypto/ec_private_key.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_device.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "net/cert/x509_util.h"
#include "third_party/boringssl/src/include/openssl/base.h"

namespace crypto {
class ECPrivateKey;
}  // namespace crypto

namespace device {

class COMPONENT_EXPORT(DEVICE_FIDO) VirtualFidoDevice : public FidoDevice {
 public:
  // Encapsulates information corresponding to one registered key on the virtual
  // authenticator device.
  struct COMPONENT_EXPORT(DEVICE_FIDO) RegistrationData {
    RegistrationData();
    RegistrationData(
        std::unique_ptr<crypto::ECPrivateKey> private_key,
        base::span<const uint8_t, kRpIdHashLength> application_parameter,
        uint32_t counter);

    RegistrationData(RegistrationData&& data);
    RegistrationData& operator=(RegistrationData&& other);

    ~RegistrationData();

    std::unique_ptr<crypto::ECPrivateKey> private_key;
    std::array<uint8_t, kRpIdHashLength> application_parameter;
    uint32_t counter = 0;
    bool is_resident = false;
    // is_u2f is true if the credential was created via a U2F interface.
    bool is_u2f = false;
    base::Optional<device::CredProtect> protection;

    // user is only valid if |is_resident| is true.
    base::Optional<device::PublicKeyCredentialUserEntity> user;
    // rp is only valid if |is_resident| is true.
    base::Optional<device::PublicKeyCredentialRpEntity> rp;

    DISALLOW_COPY_AND_ASSIGN(RegistrationData);
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

    // Number of PIN retries remaining.
    int retries = 8;
    // The number of failed PIN attempts since the token was "inserted".
    int retries_since_insertion = 0;
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

    // Whether a device with internal-UV support has fingerprints enrolled.
    bool fingerprints_enrolled = false;

    // Whether a device with bio enrollment support has been provisioned.
    bool bio_enrollment_provisioned = false;
    // Current template ID being enrolled, if any.
    base::Optional<uint8_t> bio_current_template_id;
    // Number of remaining samples in current enrollment.
    uint8_t bio_remaining_samples = 4;
    // Backing storage for enrollments and their friendly names.
    std::map<uint8_t, std::string> bio_templates;

    // pending_assertions contains the second and subsequent assertions
    // resulting from a GetAssertion call. These values are awaiting a
    // GetNextAssertion request.
    std::vector<std::vector<uint8_t>> pending_assertions;

    // pending_rps contains the remaining RPs to return a previous
    // authenticatorCredentialManagement command.
    std::list<device::PublicKeyCredentialRpEntity> pending_rps;

    // pending_registrations contains the remaining |is_resident| registration
    // to return from a previous authenticatorCredentialManagement command.
    std::list<cbor::Value::MapValue> pending_registrations;

    FidoTransportProtocol transport =
        FidoTransportProtocol::kUsbHumanInterfaceDevice;

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
                           std::unique_ptr<crypto::ECPrivateKey> private_key);

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
                           base::Optional<std::string> user_name,
                           base::Optional<std::string> user_display_name);

   private:
    friend class base::RefCounted<State>;
    ~State();

    DISALLOW_COPY_AND_ASSIGN(State);
  };

  // Constructs an object with ephemeral state. In order to have the state of
  // the device persist between operations, use the constructor that takes a
  // scoped_refptr<State>.
  VirtualFidoDevice();

  // Constructs an object that will read from, and write to, |state|.
  explicit VirtualFidoDevice(scoped_refptr<State> state);

  ~VirtualFidoDevice() override;

  State* mutable_state() const { return state_.get(); }

 protected:
  static std::vector<uint8_t> GetAttestationKey();

  scoped_refptr<State> NewReferenceToState() const { return state_; }

  static bool Sign(crypto::ECPrivateKey* private_key,
                   base::span<const uint8_t> sign_buffer,
                   std::vector<uint8_t>* signature);

  // Constructs certificate encoded in X.509 format to be used for packed
  // attestation statement and FIDO-U2F attestation statement.
  // https://w3c.github.io/webauthn/#defined-attestation-formats
  base::Optional<std::vector<uint8_t>> GenerateAttestationCertificate(
      bool individual_attestation_requested) const;

  void StoreNewKey(base::span<const uint8_t> key_handle,
                   VirtualFidoDevice::RegistrationData registration_data);

  RegistrationData* FindRegistrationData(
      base::span<const uint8_t> key_handle,
      base::span<const uint8_t, kRpIdHashLength> application_parameter);

  // FidoDevice:
  void TryWink(base::OnceClosure cb) override;
  std::string GetId() const override;
  FidoTransportProtocol DeviceTransport() const override;

 private:
  scoped_refptr<State> state_ = base::MakeRefCounted<State>();

  DISALLOW_COPY_AND_ASSIGN(VirtualFidoDevice);
};

}  // namespace device

#endif  // DEVICE_FIDO_VIRTUAL_FIDO_DEVICE_H_
