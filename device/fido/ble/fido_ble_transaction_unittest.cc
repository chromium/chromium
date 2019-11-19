// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ble/fido_ble_transaction.h"

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "device/bluetooth/test/bluetooth_test.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/fido/ble/fido_ble_connection.h"
#include "device/fido/ble/fido_ble_frames.h"
#include "device/fido/ble/mock_fido_ble_connection.h"
#include "device/fido/fido_constants.h"
#include "device/fido/test_callback_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

constexpr uint16_t kDefaultControlPointLength = 20;

using FrameCallbackReceiver =
    test::ValueCallbackReceiver<base::Optional<FidoBleFrame>>;

std::vector<std::vector<uint8_t>> ToByteFragments(const FidoBleFrame& frame) {
  std::vector<std::vector<uint8_t>> byte_fragments;
  auto fragments_pair = frame.ToFragments(kDefaultControlPointLength);

  byte_fragments.reserve(1 + fragments_pair.second.size());
  byte_fragments.emplace_back();
  byte_fragments.back().reserve(kDefaultControlPointLength);
  fragments_pair.first.Serialize(&byte_fragments.back());

  while (!fragments_pair.second.empty()) {
    byte_fragments.emplace_back();
    byte_fragments.back().reserve(kDefaultControlPointLength);
    fragments_pair.second.front().Serialize(&byte_fragments.back());
    fragments_pair.second.pop();
  }

  return byte_fragments;
}

}  // namespace

class FidoBleTransactionTest : public ::testing::Test {
 public:
  base::test::TaskEnvironment& task_environment() { return task_environment_; }
  MockFidoBleConnection& connection() { return connection_; }
  FidoBleTransaction& transaction() { return *transaction_; }

  void ResetTransaction(uint16_t control_point_length) {
    transaction_ = std::make_unique<FidoBleTransaction>(&connection_,
                                                        control_point_length);
  }

 private:
  base::test::TaskEnvironment task_environment_;

  scoped_refptr<BluetoothAdapter> adapter_ =
      base::MakeRefCounted<::testing::NiceMock<MockBluetoothAdapter>>();
  MockFidoBleConnection connection_{adapter_.get(),
                                    BluetoothTestBase::kTestDeviceAddress1};
  std::unique_ptr<FidoBleTransaction> transaction_ =
      std::make_unique<FidoBleTransaction>(&connection_,
                                           kDefaultControlPointLength);
};

// Tests a case where the control point write fails.
TEST_F(FidoBleTransactionTest, WriteRequestFrame_FailWrite) {
  EXPECT_CALL(connection(), WriteControlPointPtr)
      .WillOnce(::testing::Invoke(
          [](auto&&, auto* cb) { std::move(*cb).Run(false /* success */); }));

  FrameCallbackReceiver receiver;
  transaction().WriteRequestFrame(FidoBleFrame(), receiver.callback());
  receiver.WaitForCallback();
  EXPECT_EQ(base::nullopt, receiver.value());
}

// Tests a case where the control point write succeeds.
TEST_F(FidoBleTransactionTest, WriteRequestFrame_Success) {
  EXPECT_CALL(connection(), WriteControlPointPtr)
      .WillOnce(::testing::Invoke(
          [](auto&&, auto* cb) { std::move(*cb).Run(true /* success */); }));

  FidoBleFrame frame(FidoBleDeviceCommand::kPing, std::vector<uint8_t>(10));
  FrameCallbackReceiver receiver;
  transaction().WriteRequestFrame(frame, receiver.callback());

  for (auto&& byte_fragment : ToByteFragments(frame))
    transaction().OnResponseFragment(std::move(byte_fragment));

  receiver.WaitForCallback();
  EXPECT_EQ(frame, receiver.value());
}

// Tests a scenario where the full response frame is obtained before the control
// point write was acknowledged. The response callback should only be run once
// the ACK is received.
TEST_F(FidoBleTransactionTest, WriteRequestFrame_DelayedWriteAck) {
  FidoBleConnection::WriteCallback delayed_write_callback;

  EXPECT_CALL(connection(), WriteControlPointPtr)
      .WillOnce(::testing::Invoke(
          [&](auto&&, auto* cb) { delayed_write_callback = std::move(*cb); }));

  FidoBleFrame frame(FidoBleDeviceCommand::kPing, std::vector<uint8_t>(10));
  FrameCallbackReceiver receiver;
  transaction().WriteRequestFrame(frame, receiver.callback());

  for (auto&& byte_fragment : ToByteFragments(frame))
    transaction().OnResponseFragment(std::move(byte_fragment));

  task_environment().RunUntilIdle();
  EXPECT_FALSE(receiver.was_called());

  std::move(delayed_write_callback).Run(true);
  receiver.WaitForCallback();
  EXPECT_EQ(frame, receiver.value());
}

// Tests a scenario where keep alive frames are obtained before the control
// point write was acknowledged. The keep alive should be processed.
TEST_F(FidoBleTransactionTest, WriteRequestFrame_DelayedWriteAck_KeepAlive) {
  FidoBleConnection::WriteCallback delayed_write_callback;

  EXPECT_CALL(connection(), WriteControlPointPtr)
      .WillOnce(::testing::Invoke(
          [&](auto&&, auto* cb) { delayed_write_callback = std::move(*cb); }));

  FidoBleFrame frame(FidoBleDeviceCommand::kPing, std::vector<uint8_t>(10));
  FidoBleFrame tup_needed_frame(
      FidoBleDeviceCommand::kKeepAlive,
      {base::strict_cast<uint8_t>(FidoBleFrame::KeepaliveCode::TUP_NEEDED)});
  FrameCallbackReceiver receiver;

  // Send two keep alives then the actual response.
  transaction().WriteRequestFrame(frame, receiver.callback());
  for (auto&& byte_fragment : ToByteFragments(tup_needed_frame))
    transaction().OnResponseFragment(std::move(byte_fragment));
  for (auto&& byte_fragment : ToByteFragments(tup_needed_frame))
    transaction().OnResponseFragment(std::move(byte_fragment));
  for (auto&& byte_fragment : ToByteFragments(frame))
    transaction().OnResponseFragment(std::move(byte_fragment));

  task_environment().RunUntilIdle();
  EXPECT_FALSE(receiver.was_called());

  std::move(delayed_write_callback).Run(true);
  receiver.WaitForCallback();
  EXPECT_EQ(frame, receiver.value());
}

// Tests a case where the control point length is too small.
TEST_F(FidoBleTransactionTest, WriteRequestFrame_ControlPointLength_TooSmall) {
  static constexpr uint16_t kTooSmallControlPointLength = 2u;
  ResetTransaction(kTooSmallControlPointLength);

  EXPECT_CALL(connection(), WriteControlPointPtr).Times(0);
  FidoBleFrame frame(FidoBleDeviceCommand::kPing, std::vector<uint8_t>(10));
  FrameCallbackReceiver receiver;
  transaction().WriteRequestFrame(frame, receiver.callback());

  receiver.WaitForCallback();
  EXPECT_EQ(base::nullopt, receiver.value());
}

// Tests that valid KeepaliveCodes are ignored, and only a valid
// response frame completes the request.
TEST_F(FidoBleTransactionTest, WriteRequestFrame_IgnoreValidKeepAlives) {
  EXPECT_CALL(connection(), WriteControlPointPtr)
      .WillOnce(::testing::Invoke(
          [](auto&&, auto* cb) { std::move(*cb).Run(true /* success */); }));

  FidoBleFrame frame(FidoBleDeviceCommand::kPing, std::vector<uint8_t>(10));
  FrameCallbackReceiver receiver;
  transaction().WriteRequestFrame(frame, receiver.callback());

  FidoBleFrame tup_needed_frame(
      FidoBleDeviceCommand::kKeepAlive,
      {base::strict_cast<uint8_t>(FidoBleFrame::KeepaliveCode::TUP_NEEDED)});
  for (auto&& byte_fragment : ToByteFragments(tup_needed_frame))
    transaction().OnResponseFragment(std::move(byte_fragment));

  task_environment().RunUntilIdle();
  EXPECT_FALSE(receiver.was_called());

  FidoBleFrame processing_frame(
      FidoBleDeviceCommand::kKeepAlive,
      {base::strict_cast<uint8_t>(FidoBleFrame::KeepaliveCode::PROCESSING)});
  for (auto&& byte_fragment : ToByteFragments(processing_frame))
    transaction().OnResponseFragment(std::move(byte_fragment));

  task_environment().RunUntilIdle();
  EXPECT_FALSE(receiver.was_called());

  for (auto&& byte_fragment : ToByteFragments(frame))
    transaction().OnResponseFragment(std::move(byte_fragment));

  receiver.WaitForCallback();
  EXPECT_EQ(frame, receiver.value());
}

// Tests that an invalid KeepaliveCode is treated as an error.
TEST_F(FidoBleTransactionTest, WriteRequestFrame_InvalidKeepAlive_Fail) {
  EXPECT_CALL(connection(), WriteControlPointPtr)
      .WillOnce(::testing::Invoke(
          [](auto&&, auto* cb) { std::move(*cb).Run(true /* success */); }));

  FidoBleFrame frame(FidoBleDeviceCommand::kPing, std::vector<uint8_t>(10));
  FrameCallbackReceiver receiver;
  transaction().WriteRequestFrame(frame, receiver.callback());

  // This frame is invalid, as it does not contain data.
  FidoBleFrame keep_alive_frame(FidoBleDeviceCommand::kKeepAlive, {});
  for (auto&& byte_fragment : ToByteFragments(keep_alive_frame))
    transaction().OnResponseFragment(std::move(byte_fragment));

  receiver.WaitForCallback();
  EXPECT_EQ(base::nullopt, receiver.value());
}

// Tests a scenario where the response frame contains a valid error command.
TEST_F(FidoBleTransactionTest, WriteRequestFrame_ValidErrorCommand) {
  EXPECT_CALL(connection(), WriteControlPointPtr)
      .WillOnce(::testing::Invoke(
          [](auto&&, auto* cb) { std::move(*cb).Run(true /* success */); }));

  FidoBleFrame ping_frame(FidoBleDeviceCommand::kPing,
                          std::vector<uint8_t>(10));
  FrameCallbackReceiver receiver;
  transaction().WriteRequestFrame(ping_frame, receiver.callback());

  FidoBleFrame error_frame(
      FidoBleDeviceCommand::kError,
      {base::strict_cast<uint8_t>(FidoBleFrame::ErrorCode::INVALID_CMD)});

  for (auto&& byte_fragment : ToByteFragments(error_frame))
    transaction().OnResponseFragment(std::move(byte_fragment));

  receiver.WaitForCallback();
  EXPECT_EQ(error_frame, receiver.value());
}

// Tests a scenario where the response frame contains an invalid error command.
TEST_F(FidoBleTransactionTest, WriteRequestFrame_InvalidErrorCommand) {
  EXPECT_CALL(connection(), WriteControlPointPtr)
      .WillOnce(::testing::Invoke(
          [](auto&&, auto* cb) { std::move(*cb).Run(true /* success */); }));

  FidoBleFrame ping_frame(FidoBleDeviceCommand::kPing,
                          std::vector<uint8_t>(10));
  FrameCallbackReceiver receiver;
  transaction().WriteRequestFrame(ping_frame, receiver.callback());

  // This frame is invalid, as it does not contain data.
  FidoBleFrame error_frame(FidoBleDeviceCommand::kError, {});

  for (auto&& byte_fragment : ToByteFragments(error_frame))
    transaction().OnResponseFragment(std::move(byte_fragment));

  receiver.WaitForCallback();
  EXPECT_EQ(base::nullopt, receiver.value());
}

// Tests a scenario where the command of the response frame does not match the
// command of the request frame.
TEST_F(FidoBleTransactionTest, WriteRequestFrame_InvalidResponseFrameCommand) {
  EXPECT_CALL(connection(), WriteControlPointPtr)
      .WillOnce(::testing::Invoke(
          [](auto&&, auto* cb) { std::move(*cb).Run(true /* success */); }));

  FidoBleFrame ping_frame(FidoBleDeviceCommand::kPing,
                          std::vector<uint8_t>(10));
  FrameCallbackReceiver receiver;
  transaction().WriteRequestFrame(ping_frame, receiver.callback());

  FidoBleFrame message_frame(FidoBleDeviceCommand::kMsg,
                             std::vector<uint8_t>(kDefaultControlPointLength));

  for (auto&& byte_fragment : ToByteFragments(message_frame))
    transaction().OnResponseFragment(std::move(byte_fragment));

  receiver.WaitForCallback();
  EXPECT_EQ(base::nullopt, receiver.value());
}

// Tests a scenario where the response initialization fragment is invalid.
TEST_F(FidoBleTransactionTest,
       WriteRequestFrame_InvalidResponseInitializationFragment) {
  EXPECT_CALL(connection(), WriteControlPointPtr)
      .WillRepeatedly(::testing::Invoke(
          [](auto&&, auto* cb) { std::move(*cb).Run(true /* success */); }));

  FidoBleFrame frame(FidoBleDeviceCommand::kPing,
                     std::vector<uint8_t>(kDefaultControlPointLength));
  FrameCallbackReceiver receiver;
  transaction().WriteRequestFrame(frame, receiver.callback());

  auto byte_fragments = ToByteFragments(frame);
  ASSERT_EQ(2u, byte_fragments.size());
  transaction().OnResponseFragment(std::move(byte_fragments.back()));
  transaction().OnResponseFragment(std::move(byte_fragments.front()));

  receiver.WaitForCallback();
  EXPECT_EQ(base::nullopt, receiver.value());
}

// Tests a scenario where a response continuation fragment is invalid.
TEST_F(FidoBleTransactionTest,
       WriteRequestFrame_InvalidResponseContinuationFragment) {
  EXPECT_CALL(connection(), WriteControlPointPtr)
      .WillRepeatedly(::testing::Invoke(
          [](auto&&, auto* cb) { std::move(*cb).Run(true /* success */); }));

  FidoBleFrame frame(FidoBleDeviceCommand::kPing,
                     std::vector<uint8_t>(kDefaultControlPointLength));
  FrameCallbackReceiver receiver;
  transaction().WriteRequestFrame(frame, receiver.callback());

  // Provide the initialization fragment twice. The second time should be an
  // error, as it's not a valid continuation fragment.
  auto byte_fragments = ToByteFragments(frame);
  ASSERT_EQ(2u, byte_fragments.size());
  transaction().OnResponseFragment(byte_fragments.front());
  transaction().OnResponseFragment(byte_fragments.front());

  receiver.WaitForCallback();
  EXPECT_EQ(base::nullopt, receiver.value());
}

// Tests a scenario where the order of response continuation fragments is
// invalid.
TEST_F(FidoBleTransactionTest,
       WriteRequestFrame_InvalidOrderResponseContinuationFragments) {
  EXPECT_CALL(connection(), WriteControlPointPtr)
      .WillRepeatedly(::testing::Invoke(
          [](auto&&, auto* cb) { std::move(*cb).Run(true /* success */); }));

  FidoBleFrame frame(FidoBleDeviceCommand::kPing,
                     std::vector<uint8_t>(kDefaultControlPointLength * 2));
  FrameCallbackReceiver receiver;
  transaction().WriteRequestFrame(frame, receiver.callback());

  // Provide the continuation fragments in the wrong order.
  auto byte_fragments = ToByteFragments(frame);
  ASSERT_EQ(3u, byte_fragments.size());
  transaction().OnResponseFragment(byte_fragments[0]);
  transaction().OnResponseFragment(byte_fragments[2]);
  transaction().OnResponseFragment(byte_fragments[1]);

  receiver.WaitForCallback();
  EXPECT_EQ(base::nullopt, receiver.value());
}

}  // namespace device
