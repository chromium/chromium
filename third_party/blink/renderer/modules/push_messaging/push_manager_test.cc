// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/push_messaging/push_manager.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview_string.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_push_subscription_options_init.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/push_messaging/push_subscription_options.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace {

const unsigned kMaxKeyLength = 255;

// NIST P-256 public key made available to tests. Must be an uncompressed
// point in accordance with SEC1 2.3.3.
const unsigned kApplicationServerKeyLength = 65;
const std::array<uint8_t, kApplicationServerKeyLength> kApplicationServerKey = {
    0x04, 0x55, 0x52, 0x6A, 0xA5, 0x6E, 0x8E, 0xAA, 0x47, 0x97, 0x36,
    0x10, 0xC1, 0x66, 0x3C, 0x1E, 0x65, 0xBF, 0xA1, 0x7B, 0xEE, 0x48,
    0xC9, 0xC6, 0xBB, 0xBF, 0x02, 0x18, 0x53, 0x72, 0x1D, 0x0C, 0x7B,
    0xA9, 0xE3, 0x11, 0xB7, 0x03, 0x52, 0x21, 0xD3, 0x71, 0x90, 0x13,
    0xA8, 0xC1, 0xCF, 0xED, 0x20, 0xF7, 0x1F, 0xD1, 0x7F, 0xF2, 0x76,
    0xB6, 0x01, 0x20, 0xD8, 0x35, 0xA5, 0xD9, 0x3C, 0x43, 0xDF};

void IsApplicationServerKeyValid(PushSubscriptionOptions* output) {
  // Copy the key into a size+1 buffer so that it can be treated as a null
  // terminated string for the purposes of EXPECT_EQ.
  std::array<uint8_t, kApplicationServerKeyLength + 1> sender_key;
  for (unsigned i = 0; i < kApplicationServerKeyLength; i++)
    sender_key[i] = kApplicationServerKey[i];
  sender_key[kApplicationServerKeyLength] = 0x0;

  ASSERT_EQ(output->applicationServerKey()->ByteLength(),
            kApplicationServerKeyLength);

  String application_server_key(output->applicationServerKey()->ByteSpan());
  ASSERT_EQ(reinterpret_cast<const char*>(sender_key.data()),
            application_server_key.Latin1());
}

TEST(PushManagerTest, ValidSenderKey) {
  test::TaskEnvironment task_environment;
  PushSubscriptionOptionsInit* options = PushSubscriptionOptionsInit::Create();
  options->setApplicationServerKey(
      MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferViewOrString>(
          DOMArrayBuffer::Create(kApplicationServerKey)));

  DummyExceptionStateForTesting exception_state;
  PushSubscriptionOptions* output =
      PushSubscriptionOptions::FromOptionsInit(options, exception_state);
  ASSERT_TRUE(output);
  ASSERT_FALSE(exception_state.HadException());
  ASSERT_NO_FATAL_FAILURE(IsApplicationServerKeyValid(output));
}

// applicationServerKey should be Unpadded 'base64url'
// https://tools.ietf.org/html/rfc7515#appendix-C
inline bool RemovePad(UChar character) {
  return character == '=';
}

TEST(PushManagerTest, ValidBase64URLWithoutPaddingSenderKey) {
  test::TaskEnvironment task_environment;
  PushSubscriptionOptionsInit* options =
      MakeGarbageCollected<PushSubscriptionOptionsInit>();
  String base64_url = WTF::Base64URLEncode(kApplicationServerKey);
  base64_url = base64_url.RemoveCharacters(RemovePad);
  options->setApplicationServerKey(
      MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferViewOrString>(
          base64_url));

  DummyExceptionStateForTesting exception_state;
  PushSubscriptionOptions* output =
      PushSubscriptionOptions::FromOptionsInit(options, exception_state);
  ASSERT_TRUE(output);
  ASSERT_FALSE(exception_state.HadException());
  ASSERT_NO_FATAL_FAILURE(IsApplicationServerKeyValid(output));
}

TEST(PushManagerTest, InvalidSenderKeyLength) {
  test::TaskEnvironment task_environment;
  uint8_t sender_key[kMaxKeyLength + 1] = {};
  PushSubscriptionOptionsInit* options = PushSubscriptionOptionsInit::Create();
  options->setApplicationServerKey(
      MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferViewOrString>(
          DOMArrayBuffer::Create(sender_key)));

  DummyExceptionStateForTesting exception_state;
  PushSubscriptionOptions* output =
      PushSubscriptionOptions::FromOptionsInit(options, exception_state);
  ASSERT_TRUE(output);
  ASSERT_TRUE(exception_state.HadException());
  ASSERT_EQ(exception_state.Message(),
            "The provided applicationServerKey is not valid.");
}

TEST(PushManagerTest, InvalidBase64SenderKey) {
  test::TaskEnvironment task_environment;
  PushSubscriptionOptionsInit* options =
      MakeGarbageCollected<PushSubscriptionOptionsInit>();
  options->setApplicationServerKey(
      MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferViewOrString>(
          Base64Encode(kApplicationServerKey)));

  DummyExceptionStateForTesting exception_state;
  PushSubscriptionOptions* output =
      PushSubscriptionOptions::FromOptionsInit(options, exception_state);
  ASSERT_TRUE(output);
  ASSERT_TRUE(exception_state.HadException());
  ASSERT_EQ(exception_state.Message(),
            "The provided applicationServerKey is not encoded as base64url "
            "without padding.");
}

TEST(PushManagerTest, InvalidBase64URLWithPaddingSenderKey) {
  test::TaskEnvironment task_environment;
  PushSubscriptionOptionsInit* options =
      MakeGarbageCollected<PushSubscriptionOptionsInit>();
  options->setApplicationServerKey(
      MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferViewOrString>(
          WTF::Base64URLEncode(kApplicationServerKey)));

  DummyExceptionStateForTesting exception_state;
  PushSubscriptionOptions* output =
      PushSubscriptionOptions::FromOptionsInit(options, exception_state);
  ASSERT_TRUE(output);
  ASSERT_TRUE(exception_state.HadException());
  ASSERT_EQ(exception_state.Message(),
            "The provided applicationServerKey is not encoded as base64url "
            "without padding.");
}

}  // namespace
}  // namespace blink
