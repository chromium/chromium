// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/virtual_fido_device.h"

#include <algorithm>
#include <tuple>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "crypto/hash.h"
#include "crypto/keypair.h"
#include "crypto/sign.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/large_blob.h"
#include "device/fido/p256_public_key.h"
#include "device/fido/public_key.h"
#include "net/cert/x509_util.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
// #include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"

namespace device {

namespace {

// The example attestation private key from the U2F spec at
// https://fidoalliance.org/specs/fido-u2f-v1.2-ps-20170411/fido-u2f-raw-message-formats-v1.2-ps-20170411.html#registration-example
//
// PKCS.8 encoded without encryption.
constexpr uint8_t kAttestationKey[]{
    0x30, 0x81, 0x87, 0x02, 0x01, 0x00, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86,
    0x48, 0xce, 0x3d, 0x02, 0x01, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d,
    0x03, 0x01, 0x07, 0x04, 0x6d, 0x30, 0x6b, 0x02, 0x01, 0x01, 0x04, 0x20,
    0xf3, 0xfc, 0xcc, 0x0d, 0x00, 0xd8, 0x03, 0x19, 0x54, 0xf9, 0x08, 0x64,
    0xd4, 0x3c, 0x24, 0x7f, 0x4b, 0xf5, 0xf0, 0x66, 0x5c, 0x6b, 0x50, 0xcc,
    0x17, 0x74, 0x9a, 0x27, 0xd1, 0xcf, 0x76, 0x64, 0xa1, 0x44, 0x03, 0x42,
    0x00, 0x04, 0x8d, 0x61, 0x7e, 0x65, 0xc9, 0x50, 0x8e, 0x64, 0xbc, 0xc5,
    0x67, 0x3a, 0xc8, 0x2a, 0x67, 0x99, 0xda, 0x3c, 0x14, 0x46, 0x68, 0x2c,
    0x25, 0x8c, 0x46, 0x3f, 0xff, 0xdf, 0x58, 0xdf, 0xd2, 0xfa, 0x3e, 0x6c,
    0x37, 0x8b, 0x53, 0xd7, 0x95, 0xc4, 0xa4, 0xdf, 0xfb, 0x41, 0x99, 0xed,
    0xd7, 0x86, 0x2f, 0x23, 0xab, 0xaf, 0x02, 0x03, 0xb4, 0xb8, 0x91, 0x1b,
    0xa0, 0x56, 0x99, 0x94, 0xe1, 0x01};

// The default large-blob array. This is an empty CBOR array (0x80) followed by
// LEFT(SHA-256(h'80'), 16).
constexpr std::array<uint8_t, 17> kDefaultLargeBlobArray = {
    0x80, 0x76, 0xbe, 0x8b, 0x52, 0x8d, 0x00, 0x75, 0xf7,
    0xaa, 0xe9, 0x8d, 0x6f, 0xa5, 0x7a, 0x6d, 0x3c};

class P256PrivateKey : public VirtualFidoDevice::PrivateKey {
 public:
  explicit P256PrivateKey(crypto::keypair::PrivateKey key) : PrivateKey(key) {}
  ~P256PrivateKey() override = default;

  std::unique_ptr<PublicKey> GetPublicKey() const override {
    return P256PublicKey::ParseX962Uncompressed(
        static_cast<int32_t>(CoseAlgorithmIdentifier::kEs256),
        GetX962PublicKey());
  }
};

class RSAPrivateKey : public VirtualFidoDevice::PrivateKey {
 public:
  explicit RSAPrivateKey(crypto::keypair::PrivateKey key) : PrivateKey(key) {}
  ~RSAPrivateKey() override = default;

  std::unique_ptr<PublicKey> GetPublicKey() const override {
    const RSA* rsa = EVP_PKEY_get0_RSA(key_.key());
    const BIGNUM* n = RSA_get0_n(rsa);
    const BIGNUM* e = RSA_get0_e(rsa);

    std::vector<uint8_t> modulus(BN_num_bytes(n));
    BN_bn2bin(n, modulus.data());

    std::vector<uint8_t> public_exponent(BN_num_bytes(e));
    BN_bn2bin(e, public_exponent.data());

    cbor::Value::MapValue map;
    map.emplace(static_cast<int64_t>(CoseKeyKey::kAlg),
                static_cast<int64_t>(CoseAlgorithmIdentifier::kRs256));
    map.emplace(static_cast<int64_t>(CoseKeyKey::kKty),
                static_cast<int64_t>(CoseKeyTypes::kRSA));
    map.emplace(static_cast<int64_t>(CoseKeyKey::kRSAModulus),
                std::move(modulus));
    map.emplace(static_cast<int64_t>(CoseKeyKey::kRSAPublicExponent),
                std::move(public_exponent));

    std::optional<std::vector<uint8_t>> cbor_bytes(
        cbor::Writer::Write(cbor::Value(std::move(map))));

    return std::make_unique<PublicKey>(
        static_cast<int32_t>(CoseAlgorithmIdentifier::kRs256), *cbor_bytes,
        key_.ToSubjectPublicKeyInfo());
  }
};

class Ed25519PrivateKey : public VirtualFidoDevice::PrivateKey {
 public:
  explicit Ed25519PrivateKey(crypto::keypair::PrivateKey key)
      : PrivateKey(key) {}
  ~Ed25519PrivateKey() override = default;

  std::unique_ptr<PublicKey> GetPublicKey() const override {
    cbor::Value::MapValue map;
    map.emplace(static_cast<int64_t>(CoseKeyKey::kAlg),
                static_cast<int64_t>(CoseAlgorithmIdentifier::kEdDSA));
    map.emplace(static_cast<int64_t>(CoseKeyKey::kKty),
                static_cast<int64_t>(CoseKeyTypes::kOKP));
    map.emplace(static_cast<int64_t>(CoseKeyKey::kEllipticCurve),
                static_cast<int64_t>(CoseCurves::kEd25519));
    map.emplace(static_cast<int64_t>(CoseKeyKey::kEllipticX),
                key_.ToEd25519PublicKey());

    std::optional<std::vector<uint8_t>> cbor_bytes(
        cbor::Writer::Write(cbor::Value(std::move(map))));

    return std::make_unique<PublicKey>(
        static_cast<int32_t>(CoseAlgorithmIdentifier::kEdDSA), *cbor_bytes,
        key_.ToSubjectPublicKeyInfo());
  }
};

class InvalidForTestingPrivateKey : public VirtualFidoDevice::PrivateKey {
 public:
  // Even though this is an invalid key, the underlying PrivateKey can't be
  // empty, so fill it with a random P-256 key that won't be used.
  InvalidForTestingPrivateKey()
      : PrivateKey(crypto::keypair::PrivateKey::GenerateEcP256()) {}

  std::vector<uint8_t> Sign(base::span<const uint8_t> message) override {
    return {'s', 'i', 'g'};
  }

  std::unique_ptr<PublicKey> GetPublicKey() const override {
    cbor::Value::MapValue map;
    map.emplace(
        static_cast<int64_t>(CoseKeyKey::kAlg),
        static_cast<int64_t>(CoseAlgorithmIdentifier::kInvalidForTesting));
    map.emplace(static_cast<int64_t>(CoseKeyKey::kKty),
                static_cast<int64_t>(CoseKeyTypes::kInvalidForTesting));

    std::optional<std::vector<uint8_t>> cbor_bytes(
        cbor::Writer::Write(cbor::Value(std::move(map))));

    return std::make_unique<PublicKey>(
        static_cast<int32_t>(CoseAlgorithmIdentifier::kInvalidForTesting),
        *cbor_bytes, std::nullopt);
  }
};

crypto::sign::SignatureKind SignatureKindForKey(
    const crypto::keypair::PrivateKey& key) {
  if (key.IsEc()) {
    return crypto::sign::ECDSA_SHA256;
  } else if (key.IsRsa()) {
    return crypto::sign::RSA_PKCS1_SHA256;
  } else if (key.IsEd25519()) {
    return crypto::sign::ED25519;
  } else {
    NOTREACHED();
  }
}

}  // namespace

// VirtualFidoDevice::PrivateKey ----------------------------------------------

VirtualFidoDevice::PrivateKey::~PrivateKey() = default;

// static
std::optional<std::unique_ptr<VirtualFidoDevice::PrivateKey>>
VirtualFidoDevice::PrivateKey::FromPKCS8(
    base::span<const uint8_t> pkcs8_private_key) {
  std::optional<crypto::keypair::PrivateKey> key =
      crypto::keypair::PrivateKey::FromPrivateKeyInfo(pkcs8_private_key);
  if (!key) {
    return std::nullopt;
  }

  if (key->IsEc()) {
    return std::make_unique<P256PrivateKey>(*key);
  } else if (key->IsRsa()) {
    return std::make_unique<RSAPrivateKey>(*key);
  } else if (key->IsEd25519()) {
    return std::make_unique<Ed25519PrivateKey>(*key);
  } else {
    NOTREACHED();
  }
}

// static
std::unique_ptr<VirtualFidoDevice::PrivateKey>
VirtualFidoDevice::PrivateKey::FreshP256Key() {
  return std::make_unique<P256PrivateKey>(
      crypto::keypair::PrivateKey::GenerateEcP256());
}

// static
std::unique_ptr<VirtualFidoDevice::PrivateKey>
VirtualFidoDevice::PrivateKey::FreshRSAKey() {
  return std::make_unique<RSAPrivateKey>(
      crypto::keypair::PrivateKey::GenerateRsa2048());
}

// static
std::unique_ptr<VirtualFidoDevice::PrivateKey>
VirtualFidoDevice::PrivateKey::FreshEd25519Key() {
  return std::make_unique<Ed25519PrivateKey>(
      crypto::keypair::PrivateKey::GenerateEd25519());
}

// static
std::unique_ptr<VirtualFidoDevice::PrivateKey>
VirtualFidoDevice::PrivateKey::FreshInvalidForTestingKey() {
  return std::make_unique<InvalidForTestingPrivateKey>();
}

std::vector<uint8_t> VirtualFidoDevice::PrivateKey::Sign(
    base::span<const uint8_t> message) {
  return crypto::sign::Sign(SignatureKindForKey(key_), key_, message);
}

std::vector<uint8_t> VirtualFidoDevice::PrivateKey::GetX962PublicKey() const {
  if (key_.IsEc()) {
    return key_.ToUncompressedX962Point();
  } else {
    NOTREACHED();
  }
}

std::vector<uint8_t> VirtualFidoDevice::PrivateKey::GetPKCS8PrivateKey() const {
  return key_.ToPrivateKeyInfo();
}

VirtualFidoDevice::PrivateKey::PrivateKey(crypto::keypair::PrivateKey key)
    : key_(std::move(key)) {}

// VirtualFidoDevice::RegistrationData ----------------------------------------

VirtualFidoDevice::RegistrationData::RegistrationData() = default;
VirtualFidoDevice::RegistrationData::RegistrationData(const std::string& rp_id)
    : application_parameter(crypto::hash::Sha256(rp_id)) {}
VirtualFidoDevice::RegistrationData::RegistrationData(
    std::unique_ptr<PrivateKey> private_key,
    base::span<const uint8_t, kRpIdHashLength> application_parameter,
    uint32_t counter)
    : private_key(std::move(private_key)),
      application_parameter(
          fido_parsing_utils::Materialize(application_parameter)),
      counter(counter) {}
VirtualFidoDevice::RegistrationData::RegistrationData(RegistrationData&& data) =
    default;
VirtualFidoDevice::RegistrationData::~RegistrationData() = default;

VirtualFidoDevice::RegistrationData&
VirtualFidoDevice::RegistrationData::operator=(RegistrationData&& other) =
    default;

// VirtualFidoDevice::State ---------------------------------------------------

VirtualFidoDevice::State::State()
    : attestation_cert_common_name("Batch Certificate"),
      individual_attestation_cert_common_name("Individual Certificate") {
  large_blob.assign(kDefaultLargeBlobArray.begin(),
                    kDefaultLargeBlobArray.end());
}
VirtualFidoDevice::State::~State() = default;

void VirtualFidoDevice::State::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void VirtualFidoDevice::State::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void VirtualFidoDevice::State::NotifyCredentialCreated(
    const Credential& credential) {
  for (Observer& observer : observers_) {
    observer.OnCredentialCreated(credential);
  }
}

void VirtualFidoDevice::State::NotifyCredentialDeleted(
    base::span<const uint8_t> credential_id) {
  for (Observer& observer : observers_) {
    observer.OnCredentialDeleted(credential_id);
  }
}

void VirtualFidoDevice::State::NotifyCredentialUpdated(
    const std::pair<base::span<const uint8_t>, RegistrationData*>& credential) {
  for (Observer& observer : observers_) {
    observer.OnCredentialUpdated(credential);
  }
}

void VirtualFidoDevice::State::NotifyAssertion(const Credential& credential) {
  for (Observer& observer : observers_) {
    observer.OnAssertion(credential);
  }
}

bool VirtualFidoDevice::State::InjectRegistration(
    base::span<const uint8_t> credential_id,
    RegistrationData registration) {
  bool was_inserted;
  std::tie(std::ignore, was_inserted) = registrations.emplace(
      fido_parsing_utils::Materialize(credential_id), std::move(registration));
  return was_inserted;
}

bool VirtualFidoDevice::State::InjectRegistration(
    base::span<const uint8_t> credential_id,
    const std::string& relying_party_id) {
  RegistrationData registration(relying_party_id);
  registration.backup_eligible = default_backup_eligibility;
  registration.backup_state = default_backup_state;
  return InjectRegistration(credential_id, std::move(registration));
}

bool VirtualFidoDevice::State::InjectResidentKey(
    base::span<const uint8_t> credential_id,
    device::PublicKeyCredentialRpEntity rp,
    device::PublicKeyCredentialUserEntity user,
    int32_t signature_counter,
    std::unique_ptr<PrivateKey> private_key) {
  auto application_parameter = crypto::hash::Sha256(rp.id);

  // Cannot create a duplicate credential for the same (RP ID, user ID) pair.
  for (const auto& registration : registrations) {
    if (registration.second.is_resident &&
        application_parameter == registration.second.application_parameter &&
        user.id == registration.second.user->id) {
      return false;
    }
  }

  RegistrationData registration(std::move(private_key),
                                std::move(application_parameter),
                                signature_counter);
  registration.is_resident = true;
  registration.rp = std::move(rp);
  registration.user = std::move(user);
  registration.backup_eligible = default_backup_eligibility;
  registration.backup_state = default_backup_state;

  bool was_inserted;
  std::tie(std::ignore, was_inserted) = registrations.emplace(
      fido_parsing_utils::Materialize(credential_id), std::move(registration));
  return was_inserted;
}

bool VirtualFidoDevice::State::InjectResidentKey(
    base::span<const uint8_t> credential_id,
    device::PublicKeyCredentialRpEntity rp,
    device::PublicKeyCredentialUserEntity user) {
  return InjectResidentKey(credential_id, std::move(rp), std::move(user),
                           /*signature_counter=*/0, PrivateKey::FreshP256Key());
}

bool VirtualFidoDevice::State::InjectResidentKey(
    base::span<const uint8_t> credential_id,
    const std::string& relying_party_id,
    base::span<const uint8_t> user_id,
    std::optional<std::string> user_name,
    std::optional<std::string> user_display_name) {
  return InjectResidentKey(
      credential_id, PublicKeyCredentialRpEntity(std::move(relying_party_id)),
      PublicKeyCredentialUserEntity(fido_parsing_utils::Materialize(user_id),
                                    std::move(user_name),
                                    std::move(user_display_name)));
}

std::optional<LargeBlob> VirtualFidoDevice::State::GetLargeBlob(
    const RegistrationData& credential) {
  if (credential.large_blob) {
    return credential.large_blob;
  }
  if (!credential.large_blob_key) {
    return std::nullopt;
  }
  LargeBlobArrayReader reader;
  reader.Append(large_blob);
  std::optional<cbor::Value::ArrayValue> large_blob_array =
      reader.Materialize();
  if (!large_blob_array) {
    return std::nullopt;
  }
  for (const cbor::Value& blob_cbor : *large_blob_array) {
    std::optional<LargeBlobData> data = LargeBlobData::Parse(blob_cbor);
    if (!data.has_value()) {
      continue;
    }

    std::optional<LargeBlob> blob = data->Decrypt(*credential.large_blob_key);
    if (blob) {
      return blob;
    }
  }
  return std::nullopt;
}

void VirtualFidoDevice::State::InjectLargeBlob(RegistrationData* credential,
                                               LargeBlob blob) {
  LargeBlobArrayReader reader;
  reader.Append(large_blob);
  cbor::Value::ArrayValue large_blob_array =
      reader.Materialize().value_or(cbor::Value::ArrayValue());

  if (credential->large_blob_key) {
    std::erase_if(
        large_blob_array, [&credential](const cbor::Value& blob_cbor) {
          std::optional<LargeBlobData> blob = LargeBlobData::Parse(blob_cbor);
          return blob && blob->Decrypt(*credential->large_blob_key).has_value();
        });
  } else {
    credential->large_blob_key.emplace();
    base::RandBytes(*credential->large_blob_key);
  }

  large_blob_array.emplace_back(
      LargeBlobData(*credential->large_blob_key, std::move(blob)).AsCBOR());
  LargeBlobArrayWriter writer(std::move(large_blob_array));
  large_blob = writer.Pop(writer.size()).bytes;
}

void VirtualFidoDevice::State::InjectOpaqueLargeBlob(cbor::Value blob) {
  LargeBlobArrayReader reader;
  reader.Append(large_blob);
  cbor::Value::ArrayValue large_blob_array =
      reader.Materialize().value_or(cbor::Value::ArrayValue());
  large_blob_array.emplace_back(std::move(blob));
  LargeBlobArrayWriter writer(std::move(large_blob_array));
  large_blob = writer.Pop(writer.size()).bytes;
}

void VirtualFidoDevice::State::ClearLargeBlobs() {
  large_blob.assign(kDefaultLargeBlobArray.begin(),
                    kDefaultLargeBlobArray.end());
}

// VirtualFidoDevice ----------------------------------------------------------

VirtualFidoDevice::VirtualFidoDevice() = default;

VirtualFidoDevice::VirtualFidoDevice(scoped_refptr<State> state)
    : state_(std::move(state)) {}

VirtualFidoDevice::~VirtualFidoDevice() = default;

std::string VirtualFidoDevice::GetId() const {
  return state_->device_id_override.value_or(id_);
}

// static
std::vector<uint8_t> VirtualFidoDevice::GetAttestationKey() {
  return fido_parsing_utils::Materialize(kAttestationKey);
}

std::optional<std::vector<uint8_t>>
VirtualFidoDevice::GenerateAttestationCertificate(
    bool individual_attestation_requested,
    bool include_transports) const {
  auto attestation_private_key =
      crypto::keypair::PrivateKey::FromPrivateKeyInfo(GetAttestationKey());
  constexpr uint32_t kAttestationCertSerialNumber = 1;

  // https://fidoalliance.org/specs/fido-u2f-v1.2-ps-20170411/fido-u2f-authenticator-transports-extension-v1.2-ps-20170411.html#fido-u2f-certificate-transports-extension
  static constexpr uint8_t kTransportTypesOID[] = {
      0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0xe5, 0x1c, 0x02, 0x01, 0x01};
  uint8_t transport_bit;
  switch (DeviceTransport()) {
    case FidoTransportProtocol::kBluetoothLowEnergy:
    case FidoTransportProtocol::kHybrid:
      transport_bit = 1;
      break;
    case FidoTransportProtocol::kUsbHumanInterfaceDevice:
      transport_bit = 2;
      break;
    case FidoTransportProtocol::kNearFieldCommunication:
      transport_bit = 3;
      break;
    case FidoTransportProtocol::kInternal:
      transport_bit = 4;
      break;
    case FidoTransportProtocol::kDeprecatedAoa:
      NOTREACHED();
  }
  const uint8_t kTransportTypesContents[] = {
      3,                                            // BIT STRING
      2,                                            // two bytes long
      static_cast<uint8_t>(8 - transport_bit - 1),  // trailing bits unused
      static_cast<uint8_t>(0b10000000 >> transport_bit),
      // transport
  };

  // https://www.w3.org/TR/webauthn/#sctn-packed-attestation-cert-requirements
  // The Basic Constraints extension MUST have the CA component set to false.
  // Since that is the default value, DER requires omitting it. Simply include
  // an empty sequence.
  static constexpr uint8_t kBasicContraintsOID[] = {0x55, 0x1d, 0x13};
  static constexpr uint8_t kBasicContraintsContents[] = {
      0x30,  // SEQUENCE
      0x00,  // zero bytes long
  };

  std::vector<net::x509_util::Extension> extensions = {
      {kBasicContraintsOID, /*critical=*/true, kBasicContraintsContents},
  };
  if (include_transports) {
    extensions.emplace_back(kTransportTypesOID, /*critical=*/false,
                            kTransportTypesContents);
  }

  // https://w3c.github.io/webauthn/#sctn-packed-attestation-cert-requirements
  // Make the certificate expire about 20 years from now.
  base::Time expiry_date = base::Time::Now() + base::Days(365 * 20);
  std::string attestation_cert;
  if (!net::x509_util::CreateSelfSignedCert(
          attestation_private_key->key(), net::x509_util::DIGEST_SHA256,
          "C=US, O=Chromium, OU=Authenticator Attestation, CN=" +
              (individual_attestation_requested
                   ? state_->individual_attestation_cert_common_name
                   : state_->attestation_cert_common_name),
          kAttestationCertSerialNumber, base::Time::FromTimeT(1500000000),
          expiry_date, extensions, &attestation_cert)) {
    DVLOG(2) << "Failed to create attestation certificate";
    return std::nullopt;
  }

  return std::vector<uint8_t>(attestation_cert.begin(), attestation_cert.end());
}

void VirtualFidoDevice::StoreNewKey(
    base::span<const uint8_t> key_handle,
    VirtualFidoDevice::RegistrationData registration_data) {
  // Skip storing the registration if this is a dummy request. This prevents
  // dummy credentials to be returned by the GetCredentials method of the
  // virtual authenticator API.
  if (registration_data.application_parameter == device::kBogusAppParam ||
      registration_data.application_parameter ==
          crypto::hash::Sha256(kDummyRpID)) {
    return;
  }

  // Store the registration. Because the key handle is the hashed public key we
  // just generated, no way this should already be registered.
  auto result = mutable_state()->registrations.emplace(
      fido_parsing_utils::Materialize(key_handle),
      std::move(registration_data));
  DCHECK(result.second);
  mutable_state()->NotifyCredentialCreated(
      std::make_pair(key_handle, &result.first->second));
}

VirtualFidoDevice::RegistrationData* VirtualFidoDevice::FindRegistrationData(
    base::span<const uint8_t> key_handle,
    base::span<const uint8_t, kRpIdHashLength> application_parameter) {
  // Check if this is our key_handle and it's for this appId.
  auto it = mutable_state()->registrations.find(key_handle);
  if (it == mutable_state()->registrations.end())
    return nullptr;

  if (!std::ranges::equal(application_parameter,
                          it->second.application_parameter)) {
    return nullptr;
  }

  return &it->second;
}

bool VirtualFidoDevice::SimulatePress() {
  if (!state_->simulate_press_callback)
    return true;

  auto weak_this = GetWeakPtr();
  bool result = state_->simulate_press_callback.Run(this);
  // |this| might have been destroyed at this point - accessing state from the
  // object without checking weak_this is dangerous.
  return weak_this && result;
}

void VirtualFidoDevice::TryWink(base::OnceClosure cb) {
  std::move(cb).Run();
}

FidoTransportProtocol VirtualFidoDevice::DeviceTransport() const {
  return state_->transport;
}

// static
std::string VirtualFidoDevice::MakeVirtualFidoDeviceId() {
  uint8_t rand_bytes[32];
  base::RandBytes(rand_bytes);
  return "VirtualFidoDevice-" + base::HexEncode(rand_bytes);
}

}  // namespace device
