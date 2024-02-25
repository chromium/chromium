// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/mojom/authenticator_mojom_traits.h"

#include <vector>

#include "device/fido/authenticator_selection_criteria.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_params.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"

namespace mojo {

using device::AuthenticatorAttachment;
using device::AuthenticatorSelectionCriteria;
using device::CableDiscoveryData;
using device::CableEidArray;
using device::CableSessionPreKeyArray;
using device::CoseAlgorithmIdentifier;
using device::CredentialType;
using device::FidoTransportProtocol;
using device::PublicKeyCredentialDescriptor;
using device::PublicKeyCredentialParams;
using device::PublicKeyCredentialRpEntity;
using device::PublicKeyCredentialUserEntity;
using device::ResidentKeyRequirement;
using device::UserVerificationRequirement;

const std::vector<uint8_t> kDescriptorId = {'d', 'e', 's', 'c'};
constexpr char kRpId[] = "google.com";
constexpr char kRpName[] = "Google";
constexpr CableEidArray kClientEid = {{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                                       0x07, 0x08, 0x09, 0x10, 0x11, 0x12, 0x13,
                                       0x14, 0x15}};
constexpr CableEidArray kAuthenticatorEid = {
    {0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
     0x01, 0x01, 0x01, 0x01}};
constexpr CableSessionPreKeyArray kSessionPreKey = {
    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
     0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
     0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}};

namespace {

template <typename MojomType, typename UserType>
void AssertSerializeAndDeserializeSucceeds(std::vector<UserType> test_cases) {
  for (auto original : test_cases) {
    UserType copied;
    EXPECT_TRUE(
        mojo::test::SerializeAndDeserialize<MojomType>(original, copied));
    EXPECT_EQ(original, copied);
  }
}

}  // namespace

// Verify serialization and deserialization of PublicKeyCredentialParams.
TEST(AuthenticatorMojomTraitsTest, SerializeCredentialParams) {
  std::vector<PublicKeyCredentialParams::CredentialInfo> success_cases = {
      {CredentialType::kPublicKey,
       base::strict_cast<int>(CoseAlgorithmIdentifier::kEs256)}};

  AssertSerializeAndDeserializeSucceeds<
      blink::mojom::PublicKeyCredentialParameters,
      PublicKeyCredentialParams::CredentialInfo>(success_cases);
}

// Verify serialization and deserialization of PublicKeyCredentialDescriptor.
TEST(AuthenticatorMojomTraitsTest, SerializeCredentialDescriptors) {
  std::vector<PublicKeyCredentialDescriptor> success_cases = {
      PublicKeyCredentialDescriptor(CredentialType::kPublicKey, kDescriptorId),
      PublicKeyCredentialDescriptor(CredentialType::kPublicKey, kDescriptorId),
      PublicKeyCredentialDescriptor(CredentialType::kPublicKey, kDescriptorId)};
  success_cases[1].transports.emplace(FidoTransportProtocol::kInternal);
  success_cases[2].transports.emplace(FidoTransportProtocol::kInternal);
  success_cases[2].transports.emplace(
      FidoTransportProtocol::kUsbHumanInterfaceDevice);
  success_cases[2].transports.emplace(
      FidoTransportProtocol::kNearFieldCommunication);
  success_cases[2].transports.emplace(FidoTransportProtocol::kHybrid);
  success_cases[2].transports.emplace(
      FidoTransportProtocol::kBluetoothLowEnergy);

  AssertSerializeAndDeserializeSucceeds<
      blink::mojom::PublicKeyCredentialDescriptor,
      PublicKeyCredentialDescriptor>(success_cases);
}

// Verify serialization and deserialization of AuthenticatorSelectionCriteria.
TEST(AuthenticatorMojomTraitsTest, SerializeAuthenticatorSelectionCriteria) {
  std::vector<AuthenticatorSelectionCriteria> success_cases = {
      AuthenticatorSelectionCriteria(AuthenticatorAttachment::kAny,
                                     ResidentKeyRequirement::kRequired,
                                     UserVerificationRequirement::kRequired),
      AuthenticatorSelectionCriteria(AuthenticatorAttachment::kPlatform,
                                     ResidentKeyRequirement::kPreferred,
                                     UserVerificationRequirement::kPreferred),
      AuthenticatorSelectionCriteria(AuthenticatorAttachment::kPlatform,
                                     ResidentKeyRequirement::kDiscouraged,
                                     UserVerificationRequirement::kPreferred),
      AuthenticatorSelectionCriteria(
          AuthenticatorAttachment::kCrossPlatform,
          ResidentKeyRequirement::kRequired,
          UserVerificationRequirement::kDiscouraged)};

  AssertSerializeAndDeserializeSucceeds<
      blink::mojom::AuthenticatorSelectionCriteria,
      AuthenticatorSelectionCriteria>(success_cases);
}

// Verify serialization and deserialization of PublicKeyCredentialRpEntity.
TEST(AuthenticatorMojomTraitsTest, SerializePublicKeyCredentialRpEntity) {
  std::vector<PublicKeyCredentialRpEntity> success_cases = {
      PublicKeyCredentialRpEntity(std::string(kRpId)),
      PublicKeyCredentialRpEntity(std::string(kRpId))};
  // TODO(kenrb): There is a mismatch between the types, where
  // device::PublicKeyCredentialRpEntity can have std::nullopt for
  // the name but the mapped mojom type is not optional. This should
  // be corrected at some point. We can't currently test std::nullopt
  // because it won't serialize.
  success_cases[0].name = std::string(kRpName);
  success_cases[1].name = std::string(kRpName);

  AssertSerializeAndDeserializeSucceeds<
      blink::mojom::PublicKeyCredentialRpEntity, PublicKeyCredentialRpEntity>(
      success_cases);
}

// Verify serialization and deserialization of PublicKeyCredentialUserEntity.
TEST(AuthenticatorMojomTraitsTest, SerializePublicKeyCredentialUserEntity) {
  std::vector<PublicKeyCredentialUserEntity> success_cases = {
      PublicKeyCredentialUserEntity(kDescriptorId),
      PublicKeyCredentialUserEntity(kDescriptorId)};
  // TODO(kenrb): |name| and |display_name| have the same issue as
  // PublicKeyCredentialRpEntity::name above.
  success_cases[0].name = std::string(kRpName);
  success_cases[0].display_name = std::string(kRpName);
  success_cases[1].name = std::string(kRpName);
  success_cases[1].display_name = std::string(kRpName);

  AssertSerializeAndDeserializeSucceeds<
      blink::mojom::PublicKeyCredentialUserEntity,
      PublicKeyCredentialUserEntity>(success_cases);
}

// Verify serialization and deserialization of CableDiscoveryData.
TEST(AuthenticatorMojomTraitsTest, SerializeCableDiscoveryData) {
  std::vector<CableDiscoveryData> success_cases = {
      CableDiscoveryData(CableDiscoveryData::Version::V1, kClientEid,
                         kAuthenticatorEid, kSessionPreKey)};

  AssertSerializeAndDeserializeSucceeds<blink::mojom::CableAuthentication,
                                        CableDiscoveryData>(success_cases);
}

}  // namespace mojo
