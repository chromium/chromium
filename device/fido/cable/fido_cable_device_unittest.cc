// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/fido_cable_device.h"

#include <array>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/optional.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "crypto/aead.h"
#include "device/bluetooth/test/bluetooth_test.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/fido/ble/mock_fido_ble_connection.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/test_callback_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::Test;
using TestDeviceCallbackReceiver =
    test::ValueCallbackReceiver<base::Optional<std::vector<uint8_t>>>;
using NiceMockBluetoothAdapter = ::testing::NiceMock<MockBluetoothAdapter>;

// Sufficiently large test control point length as we are not interested
// in testing fragmentations of BLE messages. All Cable messages are encrypted
// and decrypted per request frame, not fragment.
constexpr auto kControlPointLength = std::numeric_limits<uint16_t>::max();
// Counter value that is larger than FidoCableDevice::kMaxCounter.
constexpr uint32_t kInvalidCounter = 1 << 24;
constexpr char kTestSessionKey[] = "00000000000000000000000000000000";
constexpr std::array<uint8_t, 8> kTestEncryptionNonce = {
    {1, 1, 1, 1, 1, 1, 1, 1}};
constexpr uint8_t kTestData[] = {'T', 'E', 'S', 'T'};

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
        message, fido_parsing_utils::ConvertToStringPiece(encryption_nonce),
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
        fido_parsing_utils::ConvertToStringPiece(message),
        fido_parsing_utils::ConvertToStringPiece(encryption_nonce),
        std::string(1, base::strict_cast<uint8_t>(FidoBleDeviceCommand::kMsg)),
        &ciphertext);
    expected_client_counter_++;
    return ciphertext;
  }

  std::array<uint8_t, 8> nonce_ = kTestEncryptionNonce;
  std::string session_key_ = kTestSessionKey;
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
    device_->SetEncryptionData(kTestSessionKey, kTestEncryptionNonce);
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
  base::test::ScopedTaskEnvironment scoped_task_environment_;

 private:
  scoped_refptr<MockBluetoothAdapter> adapter_ =
      base::MakeRefCounted<NiceMockBluetoothAdapter>();
  FakeCableAuthenticator authenticator_;
  MockFidoBleConnection* connection_;
  std::unique_ptr<FidoCableDevice> device_;
};

TEST_F(FidoCableDeviceTest, TestCaBleDeviceSendData) {
  ConnectWithLength(kControlPointLength);

  EXPECT_CALL(*connection(), WriteControlPointPtr(_, _))
      .WillOnce(Invoke([this](const auto& data, auto* cb) {
        base::SequencedTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(std::move(*cb), true));

        const auto authenticator_reply = authenticator()->ReplyWithSameMessage(
            base::make_span(data).subspan(3));
        base::SequencedTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(connection()->read_callback(),
                                      ConstructSerializedOutgoingFragment(
                                          authenticator_reply)));
      }));

  TestDeviceCallbackReceiver callback_receiver;
  device()->DeviceTransact(fido_parsing_utils::Materialize(kTestData),
                           callback_receiver.callback());

  callback_receiver.WaitForCallback();
  const auto& value = callback_receiver.value();
  ASSERT_TRUE(value);
  EXPECT_THAT(*value, ::testing::ElementsAreArray(kTestData));
}

// Test that FidoCableDevice properly updates counters when sending/receiving
// multiple requests.
TEST_F(FidoCableDeviceTest, TestCableDeviceSendMultipleRequests) {
  ConnectWithLength(kControlPointLength);
  EXPECT_CALL(*connection(), WriteControlPointPtr(_, _))
      .Times(2)
      .WillRepeatedly(Invoke([this](const auto& data, auto* cb) {
        base::SequencedTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(std::move(*cb), true));

        const auto authenticator_reply = authenticator()->ReplyWithSameMessage(
            base::make_span(data).subspan(3));
        base::SequencedTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(connection()->read_callback(),
                                      ConstructSerializedOutgoingFragment(
                                          authenticator_reply)));
      }));

  ASSERT_TRUE(device()->encryption_data_);
  EXPECT_EQ(0u, device()->encryption_data_->write_sequence_num);
  EXPECT_EQ(0u, device()->encryption_data_->read_sequence_num);

  TestDeviceCallbackReceiver callback_receiver1;
  device()->DeviceTransact(fido_parsing_utils::Materialize(kTestData),
                           callback_receiver1.callback());
  callback_receiver1.WaitForCallback();
  const auto& value1 = callback_receiver1.value();
  ASSERT_TRUE(value1);
  EXPECT_THAT(*value1, ::testing::ElementsAreArray(kTestData));
  EXPECT_EQ(1u, device()->encryption_data_->write_sequence_num);
  EXPECT_EQ(1u, device()->encryption_data_->read_sequence_num);

  constexpr uint8_t kTestData2[] = {'T', 'E', 'S', 'T', '2'};
  TestDeviceCallbackReceiver callback_receiver2;
  device()->DeviceTransact(fido_parsing_utils::Materialize(kTestData2),
                           callback_receiver2.callback());
  callback_receiver2.WaitForCallback();
  const auto& value2 = callback_receiver2.value();
  ASSERT_TRUE(value2);
  EXPECT_THAT(*value2, ::testing::ElementsAreArray(kTestData2));
  EXPECT_EQ(2u, device()->encryption_data_->write_sequence_num);
  EXPECT_EQ(2u, device()->encryption_data_->read_sequence_num);
}

TEST_F(FidoCableDeviceTest, TestCableDeviceFailOnIncorrectSessionKey) {
  constexpr char kIncorrectSessionKey[] = "11111111111111111111111111111111";
  ConnectWithLength(kControlPointLength);

  EXPECT_CALL(*connection(), WriteControlPointPtr(_, _))
      .WillOnce(Invoke([this, &kIncorrectSessionKey](const auto& data,
                                                     auto* cb) {
        base::SequencedTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(std::move(*cb), true));

        authenticator()->SetSessionKey(kIncorrectSessionKey);
        const auto authenticator_reply = authenticator()->ReplyWithSameMessage(
            base::make_span(data).subspan(3));

        base::SequencedTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(connection()->read_callback(),
                                      ConstructSerializedOutgoingFragment(
                                          authenticator_reply)));
      }));

  TestDeviceCallbackReceiver callback_receiver;
  device()->DeviceTransact(fido_parsing_utils::Materialize(kTestData),
                           callback_receiver.callback());

  callback_receiver.WaitForCallback();
  const auto& value = callback_receiver.value();
  EXPECT_FALSE(value);
}

TEST_F(FidoCableDeviceTest, TestCableDeviceFailOnUnexpectedCounter) {
  constexpr uint32_t kIncorrectAuthenticatorCounter = 1;
  ConnectWithLength(kControlPointLength);

  EXPECT_CALL(*connection(), WriteControlPointPtr(_, _))
      .WillOnce(Invoke([this](const auto& data, auto* cb) {
        base::SequencedTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(std::move(*cb), true));

        authenticator()->SetAuthenticatorCounter(
            kIncorrectAuthenticatorCounter);
        const auto authenticator_reply = authenticator()->ReplyWithSameMessage(
            base::make_span(data).subspan(3));

        base::SequencedTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(connection()->read_callback(),
                                      ConstructSerializedOutgoingFragment(
                                          authenticator_reply)));
      }));

  TestDeviceCallbackReceiver callback_receiver;
  device()->DeviceTransact(fido_parsing_utils::Materialize(kTestData),
                           callback_receiver.callback());

  callback_receiver.WaitForCallback();
  const auto& value = callback_receiver.value();
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
        base::SequencedTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(std::move(*cb), true));

        authenticator()->SetAuthenticatorCounter(kInvalidCounter);
        const auto authenticator_reply = authenticator()->ReplyWithSameMessage(
            base::make_span(data).subspan(3));

        base::SequencedTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(connection()->read_callback(),
                                      ConstructSerializedOutgoingFragment(
                                          authenticator_reply)));
      }));

  TestDeviceCallbackReceiver callback_receiver;
  ASSERT_TRUE(device()->encryption_data_);
  device()->encryption_data_->read_sequence_num = kInvalidCounter;
  device()->DeviceTransact(fido_parsing_utils::Materialize(kTestData),
                           callback_receiver.callback());

  callback_receiver.WaitForCallback();
  const auto& value = callback_receiver.value();
  EXPECT_FALSE(value);
}

}  // namespace device
