// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/virtual_fido_device.h"

#include <algorithm>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "crypto/ec_private_key.h"
#include "crypto/ec_signature_creator.h"
#include "crypto/openssl_util.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/p256_public_key.h"
#include "device/fido/public_key.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
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

// CBBFunctionToVector converts a BoringSSL function that writes to a CBB to one
// that returns a std::vector. Invoke for a function, f, with:
//   CBBFunctionToVector<decltype(&f), f>(args, to, f);
template <typename F, F function, typename... Args>
std::vector<uint8_t> CBBFunctionToVector(Args&&... args) {
  uint8_t* der = nullptr;
  size_t der_len = 0;
  bssl::ScopedCBB cbb;
  CHECK(CBB_init(cbb.get(), 0) &&
        function(cbb.get(), std::forward<Args>(args)...) &&
        CBB_finish(cbb.get(), &der, &der_len));
  std::vector<uint8_t> ret(der, der + der_len);
  OPENSSL_free(der);
  return ret;
}

// EVPBackedPrivateKey is an abstract class that implements some of the
// |PrivateKey| interface using BoringSSL's EVP layer.
class EVPBackedPrivateKey : public VirtualFidoDevice::PrivateKey {
 protected:
  EVPBackedPrivateKey(int type, int (*const config_key_gen)(EVP_PKEY_CTX*)) {
    crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

    bssl::UniquePtr<EVP_PKEY_CTX> gen_ctx(
        EVP_PKEY_CTX_new_id(type, /*engine=*/nullptr));
    EVP_PKEY* pkey_ptr = nullptr;
    CHECK(EVP_PKEY_keygen_init(gen_ctx.get()) &&
          config_key_gen(gen_ctx.get()) &&
          EVP_PKEY_keygen(gen_ctx.get(), &pkey_ptr));
    pkey_.reset(pkey_ptr);
  }

  explicit EVPBackedPrivateKey(bssl::UniquePtr<EVP_PKEY> pkey)
      : pkey_(std::move(pkey)) {}

 public:
  std::vector<uint8_t> Sign(base::span<const uint8_t> msg) override {
    crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
    bssl::ScopedEVP_MD_CTX md_ctx;
    std::vector<uint8_t> ret;
    ret.resize(EVP_PKEY_size(pkey_.get()));

    size_t sig_len = ret.size();
    // Ed25519 does not separate out the hash function as an independent
    // variable so it must be nullptr in that case.
    const EVP_MD* digest =
        EVP_PKEY_id(pkey_.get()) == EVP_PKEY_ED25519 ? nullptr : EVP_sha256();
    CHECK(EVP_DigestSignInit(md_ctx.get(), /*pctx=*/nullptr, digest,
                             /*engine=*/nullptr, pkey_.get()) &&
          EVP_DigestSign(md_ctx.get(), ret.data(), &sig_len, msg.data(),
                         msg.size()) &&
          sig_len <= ret.size());
    ret.resize(sig_len);
    return ret;
  }

  std::vector<uint8_t> GetPKCS8PrivateKey() const override {
    return CBBFunctionToVector<decltype(&EVP_marshal_private_key),
                               EVP_marshal_private_key>(pkey_.get());
  }

 protected:
  bssl::UniquePtr<EVP_PKEY> pkey_;
};

class P256PrivateKey : public EVPBackedPrivateKey {
 public:
  P256PrivateKey() : EVPBackedPrivateKey(EVP_PKEY_EC, ConfigureKeyGen) {}

  explicit P256PrivateKey(bssl::UniquePtr<EVP_PKEY> pkey)
      : EVPBackedPrivateKey(std::move(pkey)) {
    CHECK_EQ(NID_X9_62_prime256v1, EC_GROUP_get_curve_name(EC_KEY_get0_group(
                                       EVP_PKEY_get0_EC_KEY(pkey_.get()))));
  }

  std::vector<uint8_t> GetX962PublicKey() const override {
    const EC_KEY* ec_key = EVP_PKEY_get0_EC_KEY(pkey_.get());
    return CBBFunctionToVector<decltype(&EC_POINT_point2cbb),
                               EC_POINT_point2cbb>(
        EC_KEY_get0_group(ec_key), EC_KEY_get0_public_key(ec_key),
        POINT_CONVERSION_UNCOMPRESSED, /*ctx=*/nullptr);
  }

  std::unique_ptr<PublicKey> GetPublicKey() const override {
    return P256PublicKey::ParseX962Uncompressed(
        static_cast<int32_t>(CoseAlgorithmIdentifier::kEs256),
        GetX962PublicKey());
  }

 private:
  static int ConfigureKeyGen(EVP_PKEY_CTX* ctx) {
    return EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx, NID_X9_62_prime256v1);
  }
};

class RSAPrivateKey : public EVPBackedPrivateKey {
 public:
  RSAPrivateKey() : EVPBackedPrivateKey(EVP_PKEY_RSA, ConfigureKeyGen) {}

  explicit RSAPrivateKey(bssl::UniquePtr<EVP_PKEY> pkey)
      : EVPBackedPrivateKey(std::move(pkey)) {}

  std::unique_ptr<PublicKey> GetPublicKey() const override {
    const RSA* rsa = EVP_PKEY_get0_RSA(pkey_.get());
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

    base::Optional<std::vector<uint8_t>> cbor_bytes(
        cbor::Writer::Write(cbor::Value(std::move(map))));

    std::vector<uint8_t> der_bytes(
        CBBFunctionToVector<decltype(&EVP_marshal_public_key),
                            EVP_marshal_public_key>(pkey_.get()));

    return std::make_unique<PublicKey>(
        static_cast<int32_t>(CoseAlgorithmIdentifier::kRs256), *cbor_bytes,
        std::move(der_bytes));
  }

 private:
  static int ConfigureKeyGen(EVP_PKEY_CTX* ctx) {
    return EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048);
  }
};

class Ed25519PrivateKey : public EVPBackedPrivateKey {
 public:
  Ed25519PrivateKey()
      : EVPBackedPrivateKey(EVP_PKEY_ED25519, ConfigureKeyGen) {}

  explicit Ed25519PrivateKey(bssl::UniquePtr<EVP_PKEY> pkey)
      : EVPBackedPrivateKey(std::move(pkey)) {}

  std::unique_ptr<PublicKey> GetPublicKey() const override {
    uint8_t public_key[32];
    size_t public_key_len = sizeof(public_key);
    CHECK(
        EVP_PKEY_get_raw_public_key(pkey_.get(), public_key, &public_key_len) &&
        public_key_len == sizeof(public_key));

    cbor::Value::MapValue map;
    map.emplace(static_cast<int64_t>(CoseKeyKey::kAlg),
                static_cast<int64_t>(CoseAlgorithmIdentifier::kEdDSA));
    map.emplace(static_cast<int64_t>(CoseKeyKey::kKty),
                static_cast<int64_t>(CoseKeyTypes::kOKP));
    map.emplace(static_cast<int64_t>(CoseKeyKey::kEllipticCurve),
                static_cast<int64_t>(CoseCurves::kEd25519));
    map.emplace(static_cast<int64_t>(CoseKeyKey::kEllipticX),
                base::span<const uint8_t>(public_key, sizeof(public_key)));

    base::Optional<std::vector<uint8_t>> cbor_bytes(
        cbor::Writer::Write(cbor::Value(std::move(map))));

    std::vector<uint8_t> der_bytes(
        CBBFunctionToVector<decltype(&EVP_marshal_public_key),
                            EVP_marshal_public_key>(pkey_.get()));

    return std::make_unique<PublicKey>(
        static_cast<int32_t>(CoseAlgorithmIdentifier::kRs256), *cbor_bytes,
        std::move(der_bytes));
  }

 private:
  static int ConfigureKeyGen(EVP_PKEY_CTX* ctx) { return 1; }
};

class InvalidForTestingPrivateKey : public VirtualFidoDevice::PrivateKey {
 public:
  InvalidForTestingPrivateKey() = default;

  std::vector<uint8_t> Sign(base::span<const uint8_t> message) override {
    return {'s', 'i', 'g'};
  }

  std::vector<uint8_t> GetPKCS8PrivateKey() const override {
    CHECK(false);
    return {};
  }

  std::unique_ptr<PublicKey> GetPublicKey() const override {
    cbor::Value::MapValue map;
    map.emplace(
        static_cast<int64_t>(CoseKeyKey::kAlg),
        static_cast<int64_t>(CoseAlgorithmIdentifier::kInvalidForTesting));
    map.emplace(static_cast<int64_t>(CoseKeyKey::kKty),
                static_cast<int64_t>(CoseKeyTypes::kInvalidForTesting));

    base::Optional<std::vector<uint8_t>> cbor_bytes(
        cbor::Writer::Write(cbor::Value(std::move(map))));

    return std::make_unique<PublicKey>(
        static_cast<int32_t>(CoseAlgorithmIdentifier::kInvalidForTesting),
        *cbor_bytes, base::nullopt);
  }
};

}  // namespace

// VirtualFidoDevice::PrivateKey ----------------------------------------------

VirtualFidoDevice::PrivateKey::~PrivateKey() = default;

std::vector<uint8_t> VirtualFidoDevice::PrivateKey::GetX962PublicKey() const {
  // Not generally possible to encode in X9.62 format. Elliptic-specific
  // subclasses can override.
  CHECK(false);
  return std::vector<uint8_t>();
}

// static
base::Optional<std::unique_ptr<VirtualFidoDevice::PrivateKey>>
VirtualFidoDevice::PrivateKey::FromPKCS8(
    base::span<const uint8_t> pkcs8_private_key) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  CBS cbs;
  CBS_init(&cbs, pkcs8_private_key.data(), pkcs8_private_key.size());
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_parse_private_key(&cbs));
  if (!pkey || CBS_len(&cbs) != 0) {
    return base::nullopt;
  }

  switch (EVP_PKEY_id(pkey.get())) {
    case EVP_PKEY_EC:
      if (EC_GROUP_get_curve_name(EC_KEY_get0_group(
              EVP_PKEY_get0_EC_KEY(pkey.get()))) != NID_X9_62_prime256v1) {
        return base::nullopt;
      }
      return std::unique_ptr<PrivateKey>(new P256PrivateKey(std::move(pkey)));

    case EVP_PKEY_RSA:
      return std::unique_ptr<PrivateKey>(new RSAPrivateKey(std::move(pkey)));

    case EVP_PKEY_ED25519:
      return std::unique_ptr<PrivateKey>(
          new Ed25519PrivateKey(std::move(pkey)));

    default:
      return base::nullopt;
  }
}

// static
std::unique_ptr<VirtualFidoDevice::PrivateKey>
VirtualFidoDevice::PrivateKey::FreshP256Key() {
  return std::make_unique<P256PrivateKey>();
}

// static
std::unique_ptr<VirtualFidoDevice::PrivateKey>
VirtualFidoDevice::PrivateKey::FreshRSAKey() {
  return std::make_unique<RSAPrivateKey>();
}

// static
std::unique_ptr<VirtualFidoDevice::PrivateKey>
VirtualFidoDevice::PrivateKey::FreshEd25519Key() {
  return std::make_unique<Ed25519PrivateKey>();
}

// static
std::unique_ptr<VirtualFidoDevice::PrivateKey>
VirtualFidoDevice::PrivateKey::FreshInvalidForTestingKey() {
  return std::make_unique<InvalidForTestingPrivateKey>();
}

// VirtualFidoDevice::RegistrationData ----------------------------------------

VirtualFidoDevice::RegistrationData::RegistrationData() = default;
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
      individual_attestation_cert_common_name("Individual Certificate") {}
VirtualFidoDevice::State::~State() = default;

bool VirtualFidoDevice::State::InjectRegistration(
    base::span<const uint8_t> credential_id,
    const std::string& relying_party_id) {
  auto application_parameter =
      fido_parsing_utils::CreateSHA256Hash(relying_party_id);

  RegistrationData registration(PrivateKey::FreshP256Key(),
                                std::move(application_parameter),
                                0 /* signature counter */);

  bool was_inserted;
  std::tie(std::ignore, was_inserted) = registrations.emplace(
      fido_parsing_utils::Materialize(credential_id), std::move(registration));
  return was_inserted;
}

bool VirtualFidoDevice::State::InjectResidentKey(
    base::span<const uint8_t> credential_id,
    device::PublicKeyCredentialRpEntity rp,
    device::PublicKeyCredentialUserEntity user,
    int32_t signature_counter,
    std::unique_ptr<PrivateKey> private_key) {
  auto application_parameter = fido_parsing_utils::CreateSHA256Hash(rp.id);

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

  bool was_inserted;
  std::tie(std::ignore, was_inserted) = registrations.emplace(
      fido_parsing_utils::Materialize(credential_id), std::move(registration));
  return was_inserted;
}

bool VirtualFidoDevice::State::InjectResidentKey(
    base::span<const uint8_t> credential_id,
    device::PublicKeyCredentialRpEntity rp,
    device::PublicKeyCredentialUserEntity user) {
  return InjectResidentKey(std::move(credential_id), std::move(rp),
                           std::move(user), /*signature_counter=*/0,
                           PrivateKey::FreshP256Key());
}

bool VirtualFidoDevice::State::InjectResidentKey(
    base::span<const uint8_t> credential_id,
    const std::string& relying_party_id,
    base::span<const uint8_t> user_id,
    base::Optional<std::string> user_name,
    base::Optional<std::string> user_display_name) {
  return InjectResidentKey(
      credential_id, PublicKeyCredentialRpEntity(std::move(relying_party_id)),
      PublicKeyCredentialUserEntity(fido_parsing_utils::Materialize(user_id),
                                    std::move(user_name),
                                    std::move(user_display_name),
                                    /*icon_url=*/base::nullopt));
}

// VirtualFidoDevice ----------------------------------------------------------

VirtualFidoDevice::VirtualFidoDevice() = default;

VirtualFidoDevice::VirtualFidoDevice(scoped_refptr<State> state)
    : state_(std::move(state)) {}

VirtualFidoDevice::~VirtualFidoDevice() = default;

// static
std::vector<uint8_t> VirtualFidoDevice::GetAttestationKey() {
  return fido_parsing_utils::Materialize(kAttestationKey);
}

bool VirtualFidoDevice::Sign(crypto::ECPrivateKey* private_key,
                             base::span<const uint8_t> sign_buffer,
                             std::vector<uint8_t>* signature) {
  auto signer = crypto::ECSignatureCreator::Create(private_key);
  return signer->Sign(sign_buffer.data(), sign_buffer.size(), signature);
}

base::Optional<std::vector<uint8_t>>
VirtualFidoDevice::GenerateAttestationCertificate(
    bool individual_attestation_requested) const {
  std::unique_ptr<crypto::ECPrivateKey> attestation_private_key =
      crypto::ECPrivateKey::CreateFromPrivateKeyInfo(GetAttestationKey());
  constexpr uint32_t kAttestationCertSerialNumber = 1;

  // https://fidoalliance.org/specs/fido-u2f-v1.2-ps-20170411/fido-u2f-authenticator-transports-extension-v1.2-ps-20170411.html#fido-u2f-certificate-transports-extension
  static constexpr uint8_t kTransportTypesOID[] = {
      0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0xe5, 0x1c, 0x02, 0x01, 0x01};
  uint8_t transport_bit;
  switch (DeviceTransport()) {
    case FidoTransportProtocol::kBluetoothLowEnergy:
    case FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy:
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
    case FidoTransportProtocol::kAndroidAccessory:
      transport_bit = 1;
      break;
  }
  const uint8_t kTransportTypesContents[] = {
      3,                            // BIT STRING
      2,                            // two bytes long
      8 - transport_bit - 1,        // trailing bits unused
      0b10000000 >> transport_bit,  // transport
  };

  // https://www.w3.org/TR/webauthn/#packed-attestation-cert-requirements
  // The Basic Constraints extension MUST have the CA component set to false.
  static constexpr uint8_t kBasicContraintsOID[] = {0x55, 0x1d, 0x13};
  static constexpr uint8_t kBasicContraintsContents[] = {
      0x30,  // SEQUENCE
      0x03,  // three bytes long
      0x01,  // BOOLEAN
      0x01,  // one byte long
      0x00,  // false
  };

  const std::vector<net::x509_util::Extension> extensions = {
      {kTransportTypesOID, /*critical=*/false, kTransportTypesContents},
      {kBasicContraintsOID, /*critical=*/true, kBasicContraintsContents},
  };

  // https://w3c.github.io/webauthn/#sctn-packed-attestation-cert-requirements
  // Make the certificate expire about 20 years from now.
  base::Time expiry_date =
      base::Time::Now() + base::TimeDelta::FromDays(365 * 20);
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
    return base::nullopt;
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
          fido_parsing_utils::CreateSHA256Hash(kDummyRpID)) {
    return;
  }

  // Store the registration. Because the key handle is the hashed public key we
  // just generated, no way this should already be registered.
  bool did_insert = false;
  std::tie(std::ignore, did_insert) = mutable_state()->registrations.emplace(
      fido_parsing_utils::Materialize(key_handle),
      std::move(registration_data));
  DCHECK(did_insert);
}

VirtualFidoDevice::RegistrationData* VirtualFidoDevice::FindRegistrationData(
    base::span<const uint8_t> key_handle,
    base::span<const uint8_t, kRpIdHashLength> application_parameter) {
  // Check if this is our key_handle and it's for this appId.
  auto it = mutable_state()->registrations.find(key_handle);
  if (it == mutable_state()->registrations.end())
    return nullptr;

  if (!std::equal(application_parameter.begin(), application_parameter.end(),
                  it->second.application_parameter.begin(),
                  it->second.application_parameter.end())) {
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

std::string VirtualFidoDevice::GetId() const {
  return id_;
}

FidoTransportProtocol VirtualFidoDevice::DeviceTransport() const {
  return state_->transport;
}

// static
std::string VirtualFidoDevice::MakeVirtualFidoDeviceId() {
  uint8_t rand_bytes[32];
  base::RandBytes(rand_bytes, sizeof(rand_bytes));
  return "VirtualFidoDevice-" + base::HexEncode(rand_bytes);
}

}  // namespace device
