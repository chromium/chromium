// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cbor/reader.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/ctap_request_common.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/mock_fido_device.h"
#include "device/fido/public/fido_constants.h"
#include "device/fido/public/fido_transport_protocol.h"
#include "device/fido/public/public_key_credential_descriptor.h"
#include "device/fido/virtual_ctap2_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/nid.h"

namespace device {

// Leveraging example 4 of section 6.1 of the spec
// https://fidoalliance.org/specs/fido-v2.0-rd-20170927/fido-client-to-authenticator-protocol-v2.0-rd-20170927.html
TEST(CTAPRequestTest, TestConstructMakeCredentialRequestParam) {
  PublicKeyCredentialRpEntity rp("acme.com");
  rp.name = "Acme";

  PublicKeyCredentialUserEntity user(
      fido_parsing_utils::Materialize(test_data::kUserId));
  user.name = "johnpsmith@example.com";
  user.display_name = "John P. Smith";

  CtapMakeCredentialRequest make_credential_param(
      test_data::kClientDataJson, std::move(rp), std::move(user),
      PublicKeyCredentialParams({{CredentialType::kPublicKey, -7},
                                 {CredentialType::kPublicKey, 257}}));
  make_credential_param.resident_key_required = true;
  make_credential_param.user_verification =
      UserVerificationRequirement::kRequired;
  auto serialized_data = MockFidoDevice::EncodeCBORRequest(
      AsCTAPRequestValuePair(make_credential_param));
  EXPECT_THAT(serialized_data, ::testing::ElementsAreArray(
                                   test_data::kCtapMakeCredentialRequest));
}

TEST(CTAPRequestTest, TestConstructGetAssertionRequest) {
  CtapGetAssertionRequest get_assertion_req("acme.com",
                                            test_data::kClientDataJson);

  std::vector<PublicKeyCredentialDescriptor> allowed_list;
  allowed_list.push_back(PublicKeyCredentialDescriptor(
      CredentialType::kPublicKey,
      {0xf2, 0x20, 0x06, 0xde, 0x4f, 0x90, 0x5a, 0xf6, 0x8a, 0x43, 0x94,
       0x2f, 0x02, 0x4f, 0x2a, 0x5e, 0xce, 0x60, 0x3d, 0x9c, 0x6d, 0x4b,
       0x3d, 0xf8, 0xbe, 0x08, 0xed, 0x01, 0xfc, 0x44, 0x26, 0x46, 0xd0,
       0x34, 0x85, 0x8a, 0xc7, 0x5b, 0xed, 0x3f, 0xd5, 0x80, 0xbf, 0x98,
       0x08, 0xd9, 0x4f, 0xcb, 0xee, 0x82, 0xb9, 0xb2, 0xef, 0x66, 0x77,
       0xaf, 0x0a, 0xdc, 0xc3, 0x58, 0x52, 0xea, 0x6b, 0x9e}));
  allowed_list.push_back(PublicKeyCredentialDescriptor(
      CredentialType::kPublicKey,
      {0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
       0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
       0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
       0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
       0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03}));

  get_assertion_req.allow_list = std::move(allowed_list);
  get_assertion_req.user_presence_required = false;
  get_assertion_req.user_verification = UserVerificationRequirement::kRequired;

  auto serialized_data = MockFidoDevice::EncodeCBORRequest(
      AsCTAPRequestValuePair(get_assertion_req));
  EXPECT_THAT(serialized_data,
              ::testing::ElementsAreArray(
                  test_data::kTestComplexCtapGetAssertionRequest));
}

// Regression test for https://crbug.com/1270757.
TEST(CTAPRequestTest, PublicKeyCredentialDescriptorAsCBOR_1270757) {
  PublicKeyCredentialDescriptor descriptor(
      CredentialType::kPublicKey, {{1, 2, 3}},
      {FidoTransportProtocol::kUsbHumanInterfaceDevice});
  cbor::Value value = AsCBOR(descriptor);
  const cbor::Value::MapValue& map = value.GetMap();
  EXPECT_FALSE(map.contains(cbor::Value("transports")));
}

// Also for https://crbug.com/1270757: check that
// |PublicKeyCredentialDescriptor| notices when extra keys are present. The
// |VirtualCtap2Device| will reject such requests.
TEST(CTAPRequestTest, PublicKeyCredentialDescriptorNoticesExtraKeys) {
  for (const bool extra_key : {false, true}) {
    SCOPED_TRACE(extra_key);
    cbor::Value::MapValue map;
    map.emplace("type", "public-key");
    map.emplace("id", std::vector<uint8_t>({1, 2, 3, 4}));
    if (extra_key) {
      map.emplace("unexpected", "value");
    }
    const std::optional<PublicKeyCredentialDescriptor> descriptor(
        PublicKeyCredentialDescriptor::CreateFromCBORValue(
            cbor::Value(std::move(map))));
    ASSERT_TRUE(descriptor);
    EXPECT_EQ(extra_key, descriptor->had_other_keys);
  }
}

TEST(CTAPRequestTest, ParseHMACSecret) {
  using X962Array = std::array<uint8_t, kP256X962Length>;
  X962Array x962 = {0, 1, 2, 3};
  bssl::UniquePtr<EC_KEY> key(EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  CHECK(EC_KEY_generate_key(key.get()));
  CHECK_EQ(x962.size(),
           EC_POINT_point2oct(EC_KEY_get0_group(key.get()),
                              EC_KEY_get0_public_key(key.get()),
                              POINT_CONVERSION_UNCOMPRESSED, x962.data(),
                              x962.size(), nullptr /* BN_CTX */));

  HMACSecret secret(std::move(x962), base::span<const uint8_t>({4, 5, 6, 7}),
                    base::span<const uint8_t>({8, 9, 10, 11}), std::nullopt);

  {
    std::optional<HMACSecret> new_secret =
        HMACSecret::Parse(secret.AsCBORMapValue(PINUVAuthProtocol::kV1));

    ASSERT_TRUE(new_secret);
    EXPECT_EQ(secret.public_key_x962, new_secret->public_key_x962);
    EXPECT_EQ(secret.encrypted_salts, new_secret->encrypted_salts);
    EXPECT_EQ(secret.salts_auth, new_secret->salts_auth);
    EXPECT_EQ(std::nullopt, new_secret->pin_protocol);
  }

  {
    std::optional<HMACSecret> new_secret =
        HMACSecret::Parse(secret.AsCBORMapValue(PINUVAuthProtocol::kV2));

    ASSERT_TRUE(new_secret);
    EXPECT_EQ(secret.public_key_x962, new_secret->public_key_x962);
    EXPECT_EQ(secret.encrypted_salts, new_secret->encrypted_salts);
    EXPECT_EQ(secret.salts_auth, new_secret->salts_auth);
    EXPECT_EQ(PINUVAuthProtocol::kV2, new_secret->pin_protocol);
  }

  {
    cbor::Value::MapValue hmac_extension;
    hmac_extension.emplace(1, 0);
    hmac_extension.emplace(2, secret.encrypted_salts);
    hmac_extension.emplace(3, secret.salts_auth);
    hmac_extension.emplace(4, static_cast<int64_t>(PINUVAuthProtocol::kV2));
    ASSERT_EQ(std::nullopt, HMACSecret::Parse(hmac_extension));
  }

  {
    cbor::Value::MapValue hmac_extension;
    hmac_extension.emplace(1, pin::EncodeCOSEPublicKey(X962Array()));
    hmac_extension.emplace(2, secret.encrypted_salts);
    hmac_extension.emplace(3, secret.salts_auth);
    hmac_extension.emplace(4, static_cast<int64_t>(PINUVAuthProtocol::kV2));
    ASSERT_EQ(std::nullopt, HMACSecret::Parse(hmac_extension));
  }

  {
    cbor::Value::MapValue hmac_extension;
    hmac_extension.emplace(1, pin::EncodeCOSEPublicKey(secret.public_key_x962));
    hmac_extension.emplace(2, 0);
    hmac_extension.emplace(3, secret.salts_auth);
    hmac_extension.emplace(4, static_cast<int64_t>(PINUVAuthProtocol::kV2));
    ASSERT_EQ(std::nullopt, HMACSecret::Parse(hmac_extension));
  }

  {
    cbor::Value::MapValue hmac_extension;
    hmac_extension.emplace(1, pin::EncodeCOSEPublicKey(secret.public_key_x962));
    hmac_extension.emplace(2, secret.encrypted_salts);
    hmac_extension.emplace(3, 0);
    hmac_extension.emplace(4, static_cast<int64_t>(PINUVAuthProtocol::kV2));
    ASSERT_EQ(std::nullopt, HMACSecret::Parse(hmac_extension));
  }

  {
    cbor::Value::MapValue hmac_extension;
    hmac_extension.emplace(1, pin::EncodeCOSEPublicKey(secret.public_key_x962));
    hmac_extension.emplace(2, secret.encrypted_salts);
    hmac_extension.emplace(3, secret.salts_auth);
    hmac_extension.emplace(4, -1);
    ASSERT_EQ(std::nullopt, HMACSecret::Parse(hmac_extension));
  }

  {
    cbor::Value::MapValue hmac_extension;
    hmac_extension.emplace(1, pin::EncodeCOSEPublicKey(secret.public_key_x962));
    hmac_extension.emplace(2, secret.encrypted_salts);
    hmac_extension.emplace(3, secret.salts_auth);
    hmac_extension.emplace(4, 0);
    ASSERT_EQ(std::nullopt, HMACSecret::Parse(hmac_extension));
  }

  for (bool expect_hmac_parsing_error : {true, false}) {
    SCOPED_TRACE(testing::Message()
                 << "expect_hmac_parsing_error: " << expect_hmac_parsing_error);

    PublicKeyCredentialRpEntity rp("acme.com");
    rp.name = "Acme";
    PublicKeyCredentialUserEntity user(
        fido_parsing_utils::Materialize(test_data::kUserId));
    user.name = "johnpsmith@example.com";
    user.display_name = "John P. Smith";
    CtapMakeCredentialRequest make_credential_request(
        test_data::kClientDataJson, std::move(rp), std::move(user),
        PublicKeyCredentialParams({{CredentialType::kPublicKey, -7},
                                   {CredentialType::kPublicKey, 257}}));
    make_credential_request.pin_protocol = PINUVAuthProtocol::kV2;
    make_credential_request.hmac_secret_mc.emplace(
        expect_hmac_parsing_error ? X962Array() : secret.public_key_x962,
        secret.encrypted_salts, secret.salts_auth, std::nullopt);

    auto value_pair = AsCTAPRequestValuePair(make_credential_request);
    ASSERT_TRUE(value_pair.second);
    ASSERT_TRUE(value_pair.second->is_map());
    const cbor::Value::MapValue& request_map = value_pair.second->GetMap();
    const auto extensions_it = request_map.find(cbor::Value(6));
    ASSERT_NE(request_map.end(), extensions_it);
    ASSERT_TRUE(extensions_it->second.is_map());
    const cbor::Value::MapValue& extensions = extensions_it->second.GetMap();
    const auto hmac_secret_mc_it =
        extensions.find(cbor::Value(kExtensionHmacSecretMc));
    ASSERT_NE(extensions.end(), hmac_secret_mc_it);
    ASSERT_TRUE(hmac_secret_mc_it->second.is_map());

    auto new_request = CtapMakeCredentialRequest::Parse(request_map);

    if (expect_hmac_parsing_error) {
      ASSERT_FALSE(new_request);
    } else {
      ASSERT_TRUE(new_request);
      EXPECT_EQ(make_credential_request.user, new_request->user);
      EXPECT_EQ(make_credential_request.rp, new_request->rp);
      ASSERT_TRUE(new_request->hmac_secret_mc);
      HMACSecret& new_secret = *new_request->hmac_secret_mc;
      EXPECT_EQ(secret.public_key_x962, new_secret.public_key_x962);
      EXPECT_EQ(secret.encrypted_salts, new_secret.encrypted_salts);
      EXPECT_EQ(secret.salts_auth, new_secret.salts_auth);
      EXPECT_EQ(PINUVAuthProtocol::kV2, new_secret.pin_protocol);
    }
  }
}

}  // namespace device
