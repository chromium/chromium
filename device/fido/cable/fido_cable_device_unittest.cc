// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/fido_cable_device.h"

#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "crypto/aead.h"
#include "device/bluetooth/test/bluetooth_test.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/fido/cable/mock_fido_ble_connection.h"
#include "device/fido/fido_parsing_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::Test;
using TestDeviceFuture =
    base::test::TestFuture<std::optional<std::vector<uint8_t>>>;
using NiceMockBluetoothAdapter = ::testing::NiceMock<MockBluetoothAdapter>;

// Sufficiently large test control point length as we are not interested
// in testing fragmentations of BLE messages. All Cable messages are encrypted
// and decrypted per request frame, not fragment.
constexpr auto kControlPointLength = std::numeric_limits<uint16_t>::max();
// Counter value that is larger than FidoCableDevice::kMaxCounter.
constexpr uint32_t kInvalidCounter = 1 << 24;
constexpr std::array<uint8_t, 32> kTestSessionKey = {0};
constexpr std::array<uint8_t, 8> kTestEncryptionNonce = {
    {1, 1, 1, 1, 1, 1, 1, 1}};
constexpr uint8_t kTestData[] = {'T', 'E', 'S', 'T'};
// kCTAPFramingLength is the number of bytes of framing data at the beginning
// of transmitted BLE messages. See
// https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html#ble-client-to-authenticator
constexpr size_t kCTAPFramingLength = 3;

std::vector<uint8_t> ConstructSerializedOutgoingFragment(
    base::span<const uint8_t> data) {
  FidoBleFrame response_frame(FidoBleDeviceCommand::kMsg,
                              fido_parsing_utils::Materialize(data));
  const auto response_fragment =
      std::get<0>(response_frame.ToFragments(kControlPointLength));

  std::vector<uint8_t> outgoing_message;
  response_fragment.Serialize(&outgoing_message);
  return outgoing_message;
}

class FakeCableAuthenticator {
 public:
  // Returns encrypted message of the ciphertext received from the client.
  std::vector<uint8_t> ReplyWithSameMessage(base::span<const uint8_t> message) {
    auto decrypted_message = DecryptMessage(message);
    auto message_to_send = EncryptMessage(std::move(decrypted_message));
    return std::vector<uint8_t>(message_to_send.begin(), message_to_send.end());
  }

  void SetSessionKey(const std::string& session_key) {
    session_key_ = session_key;
  }

  void SetAuthenticatorCounter(uint32_t authenticator_counter) {
    authenticator_counter_ = authenticator_counter;
  }

 private:
  std::string EncryptMessage(std::string message) {
    crypto::Aead aead(crypto::Aead::AES_256_GCM);
    DCHECK_EQ(session_key_.size(), aead.KeyLength());
    aead.Init(&session_key_);

    auto encryption_nonce = fido_parsing_utils::Materialize(nonce_);
    encryption_nonce.push_back(0x01);
    encryption_nonce.push_back(authenticator_counter_ >> 16 & 0xFF);
    encryption_nonce.push_back(authenticator_counter_ >> 8 & 0xFF);
    encryption_nonce.push_back(authenticator_counter_ & 0xFF);
    DCHECK(encryption_nonce.size() == aead.NonceLength());

    std::string ciphertext;
    aead.Seal(
        message, fido_parsing_utils::ConvertToStringView(encryption_nonce),
        std::string(1, base::strict_cast<uint8_t>(FidoBleDeviceCommand::kMsg)),
        &ciphertext);
    authenticator_counter_++;
    return ciphertext;
  }

  std::string DecryptMessage(base::span<const uint8_t> message) {
    crypto::Aead aead(crypto::Aead::AES_256_GCM);
    DCHECK_EQ(session_key_.size(), aead.KeyLength());
    aead.Init(&session_key_);

    auto encryption_nonce = fido_parsing_utils::Materialize(nonce_);
    encryption_nonce.push_back(0x00);
    encryption_nonce.push_back(expected_client_counter_ >> 16 & 0xFF);
    encryption_nonce.push_back(expected_client_counter_ >> 8 & 0xFF);
    encryption_nonce.push_back(expected_client_counter_ & 0xFF);
    DCHECK(encryption_nonce.size() == aead.NonceLength());

    std::string ciphertext;
    aead.Open(
        fido_parsing_utils::ConvertToStringView(message),
        fido_parsing_utils::ConvertToStringView(encryption_nonce),
        std::string(1, base::strict_cast<uint8_t>(FidoBleDeviceCommand::kMsg)),
        &ciphertext);
    expected_client_counter_++;
    return ciphertext;
  }

  std::array<uint8_t, 8> nonce_ = kTestEncryptionNonce;
  std::string session_key_{
      reinterpret_cast<const char*>(kTestSessionKey.data()),
      kTestSessionKey.size()};
  uint32_t expected_client_counter_ = 0;
  uint32_t authenticator_counter_ = 0;
};

}  // namespace

class FidoCableDeviceTest : public Test {
 public:
  FidoCableDeviceTest() {
    auto connection = std::make_unique<MockFidoBleConnection>(
        adapter_.get(), BluetoothTestBase::kTestDeviceAddress1);
    connection_ = connection.get();
    device_ = std::make_unique<FidoCableDevice>(std::move(connection));
    device_->SetV1EncryptionData(kTestSessionKey, kTestEncryptionNonce);
    connection_->read_callback() = device_->GetReadCallbackForTesting();
  }

  void ConnectWithLength(uint16_t length) {
    EXPECT_CALL(*connection(), ConnectPtr).WillOnce(Invoke([](auto* callback) {
      std::move(*callback).Run(true);
    }));

    EXPECT_CALL(*connection(), ReadControlPointLengthPtr(_))
        .WillOnce(Invoke([length](auto* cb) { std::move(*cb).Run(length); }));

    device()->Connect();
  }

  FidoCableDevice* device() { return device_.get(); }
  MockFidoBleConnection* connection() { return connection_; }
  FakeCableAuthenticator* authenticator() { return &authenticator_; }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  scoped_refptr<MockBluetoothAdapter> adapter_ =
      base::MakeRefCounted<NiceMockBluetoothAdapter>();
  FakeCableAuthenticator authenticator_;
  raw_ptr<MockFidoBleConnection, DanglingUntriaged> connection_;
  std::unique_ptr<FidoCableDevice> device_;
};

TEST_F(FidoCableDeviceTest, ConnectionFailureTest) {
  EXPECT_CALL(*connection(), ConnectPtr).WillOnce(Invoke([](auto* callback) {
    std::move(*callback).Run(false);
  }));

  device()->Connect();
}

TEST_F(FidoCableDeviceTest, StaticGetIdTest) {
  std::string address = BluetoothTestBase::kTestDeviceAddress1;
  EXPECT_EQ("ble-" + address, FidoCableDevice::GetIdForAddress(address));
}

TEST_F(FidoCableDeviceTest, GetIdTest) {
  EXPECT_EQ(std::string("ble-") + BluetoothTestBase::kTestDeviceAddress1,
            device()->GetId());
}

TEST_F(FidoCableDeviceTest, Timeout) {
  EXPECT_CALL(*connection(), ConnectPtr);
  TestDeviceFuture future;
  device()->SendPing(std::vector<uint8_t>(), future.GetCallback());

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(FidoDevice::State::kDeviceError, device()->state_for_testing());
  EXPECT_TRUE(future.IsReady());
  EXPECT_FALSE(future.Get());
}

TEST_F(FidoCableDeviceTest, TestCaBleDeviceSendData) {
  ConnectWithLength(kControlPointLength);

  EXPECT_CALL(*connection(), WriteControlPointPtr(_, _))
      .WillRepeatedly(Invoke([this](const auto& data, auto* cb) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(*cb), true));

        const auto authenticator_reply = authenticator()->ReplyWithSameMessage(
            base::make_span(data).subspan(kCTAPFramingLength));
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(connection()->read_callback(),
                                      ConstructSerializedOutgoingFragment(
                                          authenticator_reply)));
      }));

  for (size_t i = 0; i < 3; i++) {
    SCOPED_TRACE(i);

    TestDeviceFuture future;
    device()->DeviceTransact(fido_parsing_utils::Materialize(kTestData),
                             future.GetCallback());

    EXPECT_TRUE(future.Wait());
    const auto& value = future.Get();
    ASSERT_TRUE(value);
    EXPECT_THAT(*value, ::testing::ElementsAreArray(kTestData));
  }
}

TEST_F(FidoCableDeviceTest, TestCableDeviceFailOnIncorrectSessionKey) {
  constexpr char kIncorrectSessionKey[] = "11111111111111111111111111111111";
  ConnectWithLength(kControlPointLength);

  EXPECT_CALL(*connection(), WriteControlPointPtr(_, _))
      .WillOnce(Invoke([this, &kIncorrectSessionKey](const auto& data,
                                                     auto* cb) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(*cb), true));

        authenticator()->SetSessionKey(kIncorrectSessionKey);
        const auto authenticator_reply = authenticator()->ReplyWithSameMessage(
            base::make_span(data).subspan(kCTAPFramingLength));

        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(connection()->read_callback(),
                                      ConstructSerializedOutgoingFragment(
                                          authenticator_reply)));
      }));

  TestDeviceFuture future;
  device()->DeviceTransact(fido_parsing_utils::Materialize(kTestData),
                           future.GetCallback());

  EXPECT_TRUE(future.Wait());
  const auto& value = future.Get();
  EXPECT_FALSE(value);
}

TEST_F(FidoCableDeviceTest, TestCableDeviceFailOnUnexpectedCounter) {
  constexpr uint32_t kIncorrectAuthenticatorCounter = 1;
  ConnectWithLength(kControlPointLength);

  EXPECT_CALL(*connection(), WriteControlPointPtr(_, _))
      .WillOnce(Invoke([this](const auto& data, auto* cb) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(*cb), true));

        authenticator()->SetAuthenticatorCounter(
            kIncorrectAuthenticatorCounter);
        const auto authenticator_reply = authenticator()->ReplyWithSameMessage(
            base::make_span(data).subspan(kCTAPFramingLength));

        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(connection()->read_callback(),
                                      ConstructSerializedOutgoingFragment(
                                          authenticator_reply)));
      }));

  TestDeviceFuture future;
  device()->DeviceTransact(fido_parsing_utils::Materialize(kTestData),
                           future.GetCallback());

  EXPECT_TRUE(future.Wait());
  const auto& value = future.Get();
  EXPECT_FALSE(value);
}

// Test the unlikely event that the authenticator and client has sent/received
// requests more than FidoCableDevice::kMaxCounter amount of times. As we are
// only using 3 bytes to encapsulate counter during encryption, any counter
// value that is above FidoCableDevice::kMaxCounter -- even though it may be
// the expected counter value -- should return an error.
TEST_F(FidoCableDeviceTest, TestCableDeviceErrorOnMaxCounter) {
  ConnectWithLength(kControlPointLength);

  EXPECT_CALL(*connection(), WriteControlPointPtr(_, _))
      .WillOnce(Invoke([this](const auto& data, auto* cb) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(*cb), true));

        authenticator()->SetAuthenticatorCounter(kInvalidCounter);
        const auto authenticator_reply = authenticator()->ReplyWithSameMessage(
            base::make_span(data).subspan(kCTAPFramingLength));

        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(connection()->read_callback(),
                                      ConstructSerializedOutgoingFragment(
                                          authenticator_reply)));
      }));

  TestDeviceFuture future;
  device()->SetSequenceNumbersForTesting(kInvalidCounter, 0);
  device()->DeviceTransact(fido_parsing_utils::Materialize(kTestData),
                           future.GetCallback());

  EXPECT_TRUE(future.Wait());
  const auto& value = future.Get();
  EXPECT_FALSE(value);
}

}  // namespace device
