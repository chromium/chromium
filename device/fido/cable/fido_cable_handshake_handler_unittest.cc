// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/fido_cable_handshake_handler.h"

#include <array>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/optional.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "crypto/hkdf.h"
#include "crypto/hmac.h"
#include "device/bluetooth/test/bluetooth_test.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/fido/cable/fido_ble_frames.h"
#include "device/fido/cable/fido_cable_device.h"
#include "device/fido/cable/mock_fido_ble_connection.h"
#include "device/fido/fido_constants.h"
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

constexpr std::array<uint8_t, 16> kAuthenticatorSessionRandom = {{
    0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04,
    0x01, 0x02, 0x03, 0x04,
}};

constexpr std::array<uint8_t, 32> kTestSessionPreKey = {{
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
}};

constexpr std::array<uint8_t, 32> kIncorrectSessionPreKey = {{
    0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee,
    0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee,
    0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee,
}};

constexpr std::array<uint8_t, 8> kTestNonce = {{
    0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x09, 0x08,
}};

constexpr std::array<uint8_t, 8> kIncorrectNonce = {{
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
}};

constexpr std::array<uint8_t, 50> kValidAuthenticatorHello = {{
    // Map(2)
    0xA2,
    // Key(0)
    0x00,
    // Text(28)
    0x78, 0x1C,
    // "caBLE v1 authenticator hello"
    0x63, 0x61, 0x42, 0x4C, 0x45, 0x20, 0x76, 0x31, 0x20, 0x61, 0x75, 0x74,
    0x68, 0x65, 0x6E, 0x74, 0x69, 0x63, 0x61, 0x74, 0x6F, 0x72, 0x20, 0x68,
    0x65, 0x6C, 0x6C, 0x6F,
    // Key(1)
    0x01,
    // Bytes(16)
    0x50,
    // Authenticator random session
    0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04,
    0x01, 0x02, 0x03, 0x04,
}};

constexpr std::array<uint8_t, 43> kInvalidAuthenticatorHello = {{
    // Map(2)
    0xA2,
    // Key(0)
    0x00,
    // Text(21)
    0x75,
    // "caBLE INVALID MESSAGE"
    0x63, 0x61, 0x42, 0x4C, 0x45, 0x20, 0x49, 0x4E, 0x56, 0x41, 0x4C, 0x49,
    0x44, 0x20, 0x4D, 0x45, 0x53, 0x53, 0x41, 0x47, 0x45,
    // Key(1)
    0x01,
    // Bytes(16)
    0x50,
    // Authenticator random session
    0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04,
    0x01, 0x02, 0x03, 0x04,
}};

constexpr char kIncorrectHandshakeKey[] = "INCORRECT_HANDSHAKE_KEY_12345678";

// Returns the expected encryption key that should be constructed given that
// the client random nonce is |client_random_nonce| and other determining
// factors (i.e. authenticator session random, session pre key, and nonce) are
// |kAuthenticatorSessionRandom|, |kTestSessionPreKey|, and |kTestNonce|,
// respectively.
std::vector<uint8_t> GetExpectedEncryptionKey(
    base::span<const uint8_t> client_random_nonce) {
  std::vector<uint8_t> nonce_message =
      fido_parsing_utils::Materialize(kTestNonce);
  fido_parsing_utils::Append(&nonce_message, client_random_nonce);
  fido_parsing_utils::Append(&nonce_message, kAuthenticatorSessionRandom);
  return crypto::HkdfSha256(kTestSessionPreKey,
                            crypto::SHA256Hash(nonce_message),
                            kCableDeviceEncryptionKeyInfo, 32);
}

// Given a hello message and handshake key from the authenticator, construct
// a handshake message by concatenating hello message and its mac message
// derived from |handshake_key|.
std::vector<uint8_t> ConstructAuthenticatorHelloReply(
    base::span<const uint8_t> hello_msg,
    base::StringPiece handshake_key) {
  auto reply = fido_parsing_utils::Materialize(hello_msg);
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  if (!hmac.Init(handshake_key))
    return std::vector<uint8_t>();

  std::array<uint8_t, 32> authenticator_hello_mac;
  if (!hmac.Sign(fido_parsing_utils::ConvertToStringPiece(hello_msg),
                 authenticator_hello_mac.data(),
                 authenticator_hello_mac.size())) {
    return std::vector<uint8_t>();
  }

  fido_parsing_utils::Append(
      &reply, base::make_span(authenticator_hello_mac).first(16));
  return reply;
}

// Constructs incoming handshake message from the authenticator into a BLE
// control fragment.
std::vector<uint8_t> ConstructSerializedOutgoingFragment(
    base::span<const uint8_t> data) {
  if (data.empty())
    return std::vector<uint8_t>();

  FidoBleFrame response_frame(FidoBleDeviceCommand::kControl,
                              fido_parsing_utils::Materialize(data));
  const auto response_fragment =
      std::get<0>(response_frame.ToFragments(kControlPointLength));

  std::vector<uint8_t> outgoing_message;
  response_fragment.Serialize(&outgoing_message);
  return outgoing_message;
}

// Authenticator abstraction that handles logic related to validating handshake
// messages from the client and sending rely handshake message back to the
// client. Session key and nonce are assumed to be |kTestSessionPreKey| and
// |kTestNonce| respectively.
class FakeCableAuthenticator {
 public:
  FakeCableAuthenticator() {
    handshake_key_ = crypto::HkdfSha256(
        fido_parsing_utils::ConvertToStringPiece(kTestSessionPreKey),
        fido_parsing_utils::ConvertToStringPiece(kTestNonce),
        kCableHandshakeKeyInfo, 32);
  }

  // Receives handshake message from the client, check its validity and if the
  // handshake message is valid, store |client_session_random| embedded in the
  // handshake message.
  bool ConfirmClientHandshakeMessage(
      base::span<const uint8_t> handshake_message) {
    if (handshake_message.size() <= 16)
      return false;

    crypto::HMAC hmac(crypto::HMAC::SHA256);
    if (!hmac.Init(handshake_key_))
      return false;

    // Handshake message from client should be concatenation of client hello
    // message (42 bytes) with message authentication code (16 bytes).
    if (handshake_message.size() != 58)
      return false;

    const auto client_hello = handshake_message.first(42);
    if (!hmac.VerifyTruncated(
            fido_parsing_utils::ConvertToStringPiece(client_hello),
            fido_parsing_utils::ConvertToStringPiece(
                handshake_message.subspan(42)))) {
      return false;
    }

    const auto& client_hello_cbor = cbor::Reader::Read(client_hello);
    if (!client_hello_cbor)
      return false;

    const auto& message_map = client_hello_cbor->GetMap();
    auto hello_message_it = message_map.find(cbor::Value(0));
    auto client_random_nonce_it = message_map.find(cbor::Value(1));
    if (hello_message_it == message_map.end() ||
        client_random_nonce_it == message_map.end())
      return false;

    if (!hello_message_it->second.is_string() ||
        hello_message_it->second.GetString() != kCableClientHelloMessage) {
      return false;
    }

    if (!client_random_nonce_it->second.is_bytestring() ||
        client_random_nonce_it->second.GetBytestring().size() != 16) {
      return false;
    }

    client_session_random_ =
        std::move(client_random_nonce_it->second.GetBytestring());
    return true;
  }

  std::vector<uint8_t> RelyWithAuthenticatorHandShakeMessage(
      base::span<const uint8_t> handshake_message) {
    if (!ConfirmClientHandshakeMessage(handshake_message))
      return std::vector<uint8_t>();

    return ConstructAuthenticatorHelloReply(kValidAuthenticatorHello,
                                            handshake_key_);
  }

 private:
  std::string handshake_key_;
  std::vector<uint8_t> client_session_random_;
  std::vector<uint8_t> authenticator_session_random_ =
      fido_parsing_utils::Materialize(kAuthenticatorSessionRandom);
};

}  // namespace

class FidoCableHandshakeHandlerTest : public Test {
 public:
  FidoCableHandshakeHandlerTest() {
    auto connection = std::make_unique<MockFidoBleConnection>(
        adapter_.get(), BluetoothTestBase::kTestDeviceAddress1);
    connection_ = connection.get();
    device_ = std::make_unique<FidoCableDevice>(std::move(connection));

    connection_->read_callback() = device_->GetReadCallbackForTesting();
  }

  std::unique_ptr<FidoCableV1HandshakeHandler> CreateHandshakeHandler(
      std::array<uint8_t, 8> nonce,
      std::array<uint8_t, 32> session_pre_key) {
    return std::make_unique<FidoCableV1HandshakeHandler>(device_.get(), nonce,
                                                         session_pre_key);
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
  TestDeviceCallbackReceiver& callback_receiver() { return callback_receiver_; }

 protected:
  base::test::TaskEnvironment task_environment_;

 private:
  scoped_refptr<MockBluetoothAdapter> adapter_ =
      base::MakeRefCounted<NiceMockBluetoothAdapter>();
  FakeCableAuthenticator authenticator_;
  MockFidoBleConnection* connection_;
  std::unique_ptr<FidoCableDevice> device_;
  TestDeviceCallbackReceiver callback_receiver_;
};

// Checks that outgoing handshake message from the client is a BLE frame with
// Control command type.
MATCHER(IsControlFrame, "") {
  return !arg.empty() &&
         arg[0] == base::strict_cast<uint8_t>(FidoBleDeviceCommand::kControl);
}

TEST_F(FidoCableHandshakeHandlerTest, HandShakeSuccess) {
  ConnectWithLength(kControlPointLength);

  EXPECT_CALL(*connection(), WriteControlPointPtr(IsControlFrame(), _))
      .WillOnce(Invoke([this](const auto& data, auto* cb) {
        base::SequencedTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(std::move(*cb), true));

        const auto client_ble_handshake_message =
            base::make_span(data).subspan(3);
        base::SequencedTaskRunnerHandle::Get()->PostTask(
            FROM_HERE,
            base::BindOnce(
                connection()->read_callback(),
                ConstructSerializedOutgoingFragment(
                    authenticator()->RelyWithAuthenticatorHandShakeMessage(
                        client_ble_handshake_message))));
      }));

  auto handshake_handler =
      CreateHandshakeHandler(kTestNonce, kTestSessionPreKey);
  handshake_handler->InitiateCableHandshake(callback_receiver().callback());

  callback_receiver().WaitForCallback();
  const auto& value = callback_receiver().value();
  ASSERT_TRUE(value);
  EXPECT_TRUE(handshake_handler->ValidateAuthenticatorHandshakeMessage(*value));
  EXPECT_EQ(GetExpectedEncryptionKey(handshake_handler->client_session_random_),
            handshake_handler->GetEncryptionKeyAfterSuccessfulHandshake(
                kAuthenticatorSessionRandom));
}

TEST_F(FidoCableHandshakeHandlerTest, HandShakeWithIncorrectSessionPreKey) {
  ConnectWithLength(kControlPointLength);

  EXPECT_CALL(*connection(), WriteControlPointPtr(IsControlFrame(), _))
      .WillOnce(Invoke([this](const auto& data, auto* cb) {
        base::SequencedTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(std::move(*cb), true));

        const auto client_ble_handshake_message =
            base::make_span(data).subspan(3);
        base::SequencedTaskRunnerHandle::Get()->PostTask(
            FROM_HERE,
            base::BindOnce(
                connection()->read_callback(),
                ConstructSerializedOutgoingFragment(
                    authenticator()->RelyWithAuthenticatorHandShakeMessage(
                        client_ble_handshake_message))));
      }));

  auto handshake_handler =
      CreateHandshakeHandler(kTestNonce, kIncorrectSessionPreKey);
  handshake_handler->InitiateCableHandshake(callback_receiver().callback());

  callback_receiver().WaitForCallback();
  EXPECT_FALSE(callback_receiver().value());
}

TEST_F(FidoCableHandshakeHandlerTest, HandshakeFailWithIncorrectNonce) {
  ConnectWithLength(kControlPointLength);

  EXPECT_CALL(*connection(), WriteControlPointPtr(IsControlFrame(), _))
      .WillOnce(Invoke([this](const auto& data, auto* cb) {
        base::SequencedTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(std::move(*cb), true));

        const auto client_ble_handshake_message =
            base::make_span(data).subspan(3);
        base::SequencedTaskRunnerHandle::Get()->PostTask(
            FROM_HERE,
            base::BindOnce(
                connection()->read_callback(),
                ConstructSerializedOutgoingFragment(
                    authenticator()->RelyWithAuthenticatorHandShakeMessage(
                        client_ble_handshake_message))));
      }));

  auto handshake_handler =
      CreateHandshakeHandler(kIncorrectNonce, kTestSessionPreKey);
  handshake_handler->InitiateCableHandshake(callback_receiver().callback());

  callback_receiver().WaitForCallback();
  EXPECT_FALSE(callback_receiver().value());
}

TEST_F(FidoCableHandshakeHandlerTest,
       HandshakeFailWithIncorrectAuthenticatorResponse) {
  auto handshake_handler =
      CreateHandshakeHandler(kTestNonce, kTestSessionPreKey);

  EXPECT_NE(kIncorrectHandshakeKey, handshake_handler->handshake_key_);
  const auto authenticator_reply_with_invalid_key =
      ConstructAuthenticatorHelloReply(kValidAuthenticatorHello,
                                       kIncorrectHandshakeKey);
  EXPECT_FALSE(handshake_handler->ValidateAuthenticatorHandshakeMessage(
      authenticator_reply_with_invalid_key));

  const auto authenticator_reply_with_invalid_hello_msg =
      ConstructAuthenticatorHelloReply(kInvalidAuthenticatorHello,
                                       handshake_handler->handshake_key_);
  EXPECT_FALSE(handshake_handler->ValidateAuthenticatorHandshakeMessage(
      authenticator_reply_with_invalid_hello_msg));
}

}  // namespace device
