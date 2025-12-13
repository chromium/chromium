// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_transport/rtc_received_packet.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

class RtcReceivedPacketTest : public PageTestBase {};

TEST_F(RtcReceivedPacketTest, PayloadByteLength) {
  Vector<uint8_t> data;
  data.Append("packet", 6);
  auto* packet = MakeGarbageCollected<RtcReceivedPacket>(std::move(data), 0);
  EXPECT_EQ(packet->payloadByteLength(), 6u);
}

TEST_F(RtcReceivedPacketTest, CopyPayloadTo) {
  Vector<uint8_t> data;
  data.Append("packet", 6);
  auto* packet = MakeGarbageCollected<RtcReceivedPacket>(std::move(data), 0);

  auto* destination =
      DOMArrayBuffer::Create(/*num_elements=*/6, /*element_byte_size=*/1);
  DummyExceptionStateForTesting exception_state;
  packet->copyPayloadTo(
      MakeGarbageCollected<
          V8UnionArrayBufferAllowSharedOrArrayBufferViewAllowShared>(
          destination),
      exception_state);
  ASSERT_FALSE(exception_state.HadException()) << exception_state.Message();

  EXPECT_EQ(destination->ByteSpan(), String("packet").RawByteSpan());
}

TEST_F(RtcReceivedPacketTest, CopyPayloadToDestinationTooSmall) {
  V8TestingScope scope;
  Vector<uint8_t> data;
  data.Append("packet", 6);
  auto* packet = MakeGarbageCollected<RtcReceivedPacket>(std::move(data), 0);

  auto* destination =
      DOMArrayBuffer::Create(/*num_elements=*/5, /*element_byte_size=*/1);
  DummyExceptionStateForTesting exception_state;
  packet->copyPayloadTo(
      MakeGarbageCollected<
          V8UnionArrayBufferAllowSharedOrArrayBufferViewAllowShared>(
          destination),
      exception_state);
  ASSERT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.Code(), ToExceptionCode(ESErrorType::kTypeError));
}

TEST_F(RtcReceivedPacketTest, CopyPayloadToDetachedDestination) {
  V8TestingScope scope;
  Vector<uint8_t> data;
  data.Append("packet", 6);
  auto* packet = MakeGarbageCollected<RtcReceivedPacket>(std::move(data), 0);

  auto* destination =
      DOMArrayBuffer::Create(/*num_elements=*/6, /*element_byte_size=*/1);
  destination->Detach();
  DummyExceptionStateForTesting exception_state;
  packet->copyPayloadTo(
      MakeGarbageCollected<
          V8UnionArrayBufferAllowSharedOrArrayBufferViewAllowShared>(
          destination),
      exception_state);
  ASSERT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.Code(), ToExceptionCode(ESErrorType::kTypeError));
}

}  // namespace blink
