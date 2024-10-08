// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/digital_credentials/cross_device_request_dispatcher.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "content/browser/webid/digital_credentials/cross_device_request_dispatcher.h"
#include "content/public/browser/digital_credentials_cross_device.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/fido/cable/fido_cable_discovery.h"
#include "device/fido/cable/v2_authenticator.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/cable/v2_test_util.h"
#include "device/fido/fido_constants.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/nid.h"
#include "url/gurl.h"
#include "url/origin.h"

using testing::NiceMock;

namespace content::digital_credentials::cross_device {
namespace {

class DigitalCredentialsCrossDeviceRequestDispatcherTest
    : public ::testing::Test {
 public:
  void SetUp() override {
    network_context_ = device::cablev2::NewMockTunnelServer(std::nullopt);
    std::tie(ble_advert_callback_, ble_advert_events_) =
        device::cablev2::Discovery::AdvertEventStream::New();

    bssl::UniquePtr<EC_GROUP> p256(
        EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
    bssl::UniquePtr<EC_KEY> peer_identity(EC_KEY_derive_from_secret(
        p256.get(), zero_seed_.data(), zero_seed_.size()));
    CHECK_EQ(sizeof(peer_identity_x962_),
             EC_POINT_point2oct(
                 p256.get(), EC_KEY_get0_public_key(peer_identity.get()),
                 POINT_CONVERSION_UNCOMPRESSED, peer_identity_x962_,
                 sizeof(peer_identity_x962_), /*ctx=*/nullptr));

    mock_adapter_ =
        base::MakeRefCounted<NiceMock<device::MockBluetoothAdapter>>();
    device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);
  }

 protected:
  base::expected<Response, RequestDispatcher::Error> Transact(
      device::cablev2::PayloadType response_payload_type,
      const std::string& response) {
    {
      auto callback_and_event_stream = device::cablev2::Discovery::EventStream<
          std::unique_ptr<device::cablev2::Pairing>>::New();
      auto discovery = std::make_unique<device::cablev2::Discovery>(
          // This value isn't used since it's a QR-based transaction.
          device::FidoRequestType::kGetAssertion,
          base::BindLambdaForTesting([&]() { return network_context_.get(); }),
          qr_generator_key_, std::move(ble_advert_events_),
          std::move(callback_and_event_stream.second),
          /*extension_contents=*/std::vector<device::CableDiscoveryData>(),
          GetPairingCallback(), GetInvalidatedPairingCallback(),
          GetEventCallback(),
          /*must_support_ctap=*/false);
      const GURL url("https://example.com");
      url::Origin origin(url::Origin::Create(url));
      base::Value::Dict request_value;
      request_value.Set("foo", "bar");
      base::test::TestFuture<base::expected<Response, RequestDispatcher::Error>>
          callback;
      auto request_handler = std::make_unique<RequestDispatcher>(
          std::make_unique<device::FidoCableDiscovery>(
              std::vector<device::CableDiscoveryData>()),
          std::move(discovery), std::move(origin),
          base::Value(std::move(request_value)), callback.GetCallback());
      std::unique_ptr<device::cablev2::authenticator::Transaction> transaction =
          device::cablev2::authenticator::
              TransactDigitalIdentityFromQRCodeForTesting(
                  device::cablev2::authenticator::NewMockPlatform(
                      std::move(ble_advert_callback_),
                      /*ctap2_device=*/nullptr,
                      /*observer=*/nullptr),
                  base::BindLambdaForTesting(
                      [&]() { return network_context_.get(); }),
                  zero_qr_secret_, peer_identity_x962_, response_payload_type,
                  std::vector<uint8_t>(response.begin(), response.end()));
      return callback.Take();
    }
  }

  base::RepeatingCallback<void(std::unique_ptr<device::cablev2::Pairing>)>
  GetPairingCallback() {
    return base::DoNothing();
  }

  base::RepeatingCallback<void(std::unique_ptr<device::cablev2::Pairing>)>
  GetInvalidatedPairingCallback() {
    return base::DoNothing();
  }

  base::RepeatingCallback<void(device::cablev2::Event)> GetEventCallback() {
    return base::DoNothing();
  }

  std::unique_ptr<network::mojom::NetworkContext> network_context_;
  const std::array<uint8_t, device::cablev2::kQRKeySize> qr_generator_key_ = {
      0};
  std::unique_ptr<device::cablev2::Discovery::AdvertEventStream>
      ble_advert_events_;
  device::cablev2::Discovery::AdvertEventStream::Callback ble_advert_callback_;
  uint8_t peer_identity_x962_[device::kP256X962Length] = {0};
  const std::array<uint8_t, device::cablev2::kQRSecretSize> zero_qr_secret_ = {
      0};
  const std::array<uint8_t, device::cablev2::kRootSecretSize> root_secret_ = {
      0};
  const std::array<uint8_t, device::cablev2::kQRSeedSize> zero_seed_ = {0};

  scoped_refptr<NiceMock<device::MockBluetoothAdapter>> mock_adapter_;

  base::test::TaskEnvironment task_environment;
};

TEST_F(DigitalCredentialsCrossDeviceRequestDispatcherTest, Valid) {
  base::expected<Response, RequestDispatcher::Error> result =
      Transact(device::cablev2::PayloadType::kJSON,
               R"({"response": {"digital": {"data": "ok"}}})");
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value()->is_string());
  ASSERT_EQ(result.value()->GetString(), "ok");
}

TEST_F(DigitalCredentialsCrossDeviceRequestDispatcherTest, InvalidJson) {
  base::expected<Response, RequestDispatcher::Error> result =
      Transact(device::cablev2::PayloadType::kJSON, "!");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            RequestDispatcher::Error(ProtocolError::kInvalidResponse));
}

TEST_F(DigitalCredentialsCrossDeviceRequestDispatcherTest, ErrorResponse) {
  base::expected<Response, RequestDispatcher::Error> result =
      Transact(device::cablev2::PayloadType::kJSON,
               R"({"response": {"digital": {"error": "NO_CREDENTIAL"}}})");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            RequestDispatcher::Error(RemoteError::kNoCredential));
}

TEST_F(DigitalCredentialsCrossDeviceRequestDispatcherTest, OtherError) {
  base::expected<Response, RequestDispatcher::Error> result =
      Transact(device::cablev2::PayloadType::kJSON,
               R"({"response": {"digital": {"error": "RANDOM_STUFF"}}})");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), RequestDispatcher::Error(RemoteError::kOther));
}

TEST_F(DigitalCredentialsCrossDeviceRequestDispatcherTest, ErrorIsNotAString) {
  base::expected<Response, RequestDispatcher::Error> result =
      Transact(device::cablev2::PayloadType::kJSON,
               R"({"response": {"digital": {"error": 1}}})");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            RequestDispatcher::Error(ProtocolError::kInvalidResponse));
}

TEST_F(DigitalCredentialsCrossDeviceRequestDispatcherTest, InvalidStructure) {
  base::expected<Response, RequestDispatcher::Error> result =
      Transact(device::cablev2::PayloadType::kJSON, R"({"result": 1})");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            RequestDispatcher::Error(ProtocolError::kInvalidResponse));
}

TEST_F(DigitalCredentialsCrossDeviceRequestDispatcherTest, CTAPResponse) {
  base::expected<Response, RequestDispatcher::Error> result =
      Transact(device::cablev2::PayloadType::kCTAP, "");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            RequestDispatcher::Error(ProtocolError::kTransportError));
}

}  // namespace
}  // namespace content::digital_credentials::cross_device
