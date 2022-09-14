// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/messaging/string_message_codec.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace blink {
namespace {

std::u16string DecodeWithV8(const std::vector<uint8_t>& encoded) {
  base::test::TaskEnvironment task_environment;
  std::u16string result;

  v8::Isolate::CreateParams params;
  params.array_buffer_allocator =
      v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  v8::Isolate* isolate = v8::Isolate::New(params);
  {
    v8::HandleScope scope(isolate);
    v8::TryCatch try_catch(isolate);

    v8::Local<v8::Context> context = v8::Context::New(isolate);

    v8::ValueDeserializer deserializer(isolate, encoded.data(), encoded.size());
    deserializer.SetSupportsLegacyWireFormat(true);

    EXPECT_TRUE(deserializer.ReadHeader(context).ToChecked());

    v8::Local<v8::Value> value =
        deserializer.ReadValue(context).ToLocalChecked();
    v8::Local<v8::String> str = value->ToString(context).ToLocalChecked();

    result.resize(str->Length());
    str->Write(isolate, reinterpret_cast<uint16_t*>(&result[0]), 0,
               result.size());
  }
  isolate->Dispose();
  delete params.array_buffer_allocator;

  return result;
}

std::vector<uint8_t> EncodeWithV8(const std::u16string& message) {
  base::test::TaskEnvironment task_environment;
  std::vector<uint8_t> result;

  v8::Isolate::CreateParams params;
  params.array_buffer_allocator =
      v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  v8::Isolate* isolate = v8::Isolate::New(params);
  {
    v8::HandleScope scope(isolate);
    v8::TryCatch try_catch(isolate);

    v8::Local<v8::Context> context = v8::Context::New(isolate);

    v8::Local<v8::String> message_as_value =
        v8::String::NewFromTwoByte(
            isolate, reinterpret_cast<const uint16_t*>(message.data()),
            v8::NewStringType::kNormal, message.size())
            .ToLocalChecked();

    v8::ValueSerializer serializer(isolate);
    serializer.WriteHeader();
    EXPECT_TRUE(serializer.WriteValue(context, message_as_value).ToChecked());

    std::pair<uint8_t*, size_t> buffer = serializer.Release();
    result = std::vector<uint8_t>(buffer.first, buffer.first + buffer.second);
    free(buffer.first);
  }
  isolate->Dispose();
  delete params.array_buffer_allocator;

  return result;
}

TEST(StringMessageCodecTest, SelfTest_ASCII) {
  std::u16string message = u"hello";
  std::u16string decoded;
  EXPECT_TRUE(DecodeStringMessage(EncodeStringMessage(message), &decoded));
  EXPECT_EQ(message, decoded);
}

TEST(StringMessageCodecTest, SelfTest_Latin1) {
  std::u16string message = u"hello \u00E7";
  std::u16string decoded;
  EXPECT_TRUE(DecodeStringMessage(EncodeStringMessage(message), &decoded));
  EXPECT_EQ(message, decoded);
}

TEST(StringMessageCodecTest, SelfTest_TwoByte) {
  std::u16string message = u"hello \u263A";
  std::u16string decoded;
  EXPECT_TRUE(DecodeStringMessage(EncodeStringMessage(message), &decoded));
  EXPECT_EQ(message, decoded);
}

TEST(StringMessageCodecTest, SelfTest_TwoByteLongEnoughToForcePadding) {
  std::u16string message(200, 0x263A);
  std::u16string decoded;
  EXPECT_TRUE(DecodeStringMessage(EncodeStringMessage(message), &decoded));
  EXPECT_EQ(message, decoded);
}

TEST(StringMessageCodecTest, SelfToV8Test_ASCII) {
  std::u16string message = u"hello";
  EXPECT_EQ(message, DecodeWithV8(EncodeStringMessage(message)));
}

TEST(StringMessageCodecTest, SelfToV8Test_Latin1) {
  std::u16string message = u"hello \u00E7";
  EXPECT_EQ(message, DecodeWithV8(EncodeStringMessage(message)));
}

TEST(StringMessageCodecTest, SelfToV8Test_TwoByte) {
  std::u16string message = u"hello \u263A";
  EXPECT_EQ(message, DecodeWithV8(EncodeStringMessage(message)));
}

TEST(StringMessageCodecTest, SelfToV8Test_TwoByteLongEnoughToForcePadding) {
  std::u16string message(200, 0x263A);
  EXPECT_EQ(message, DecodeWithV8(EncodeStringMessage(message)));
}

TEST(StringMessageCodecTest, V8ToSelfTest_ASCII) {
  std::u16string message = u"hello";
  std::u16string decoded;
  EXPECT_TRUE(DecodeStringMessage(EncodeWithV8(message), &decoded));
  EXPECT_EQ(message, decoded);
}

TEST(StringMessageCodecTest, V8ToSelfTest_Latin1) {
  std::u16string message = u"hello \u00E7";
  std::u16string decoded;
  EXPECT_TRUE(DecodeStringMessage(EncodeWithV8(message), &decoded));
  EXPECT_EQ(message, decoded);
}

TEST(StringMessageCodecTest, V8ToSelfTest_TwoByte) {
  std::u16string message = u"hello \u263A";
  std::u16string decoded;
  EXPECT_TRUE(DecodeStringMessage(EncodeWithV8(message), &decoded));
  EXPECT_EQ(message, decoded);
}

TEST(StringMessageCodecTest, V8ToSelfTest_TwoByteLongEnoughToForcePadding) {
  std::u16string message(200, 0x263A);
  std::u16string decoded;
  EXPECT_TRUE(DecodeStringMessage(EncodeWithV8(message), &decoded));
  EXPECT_EQ(message, decoded);
}

TEST(StringMessageCodecTest, Overflow) {
  const uint8_t kOverflowOneByteData[] = {'"', 0xff, 0xff, 0xff, 0x7f};
  const uint8_t kOverflowTwoByteData[] = {'c', 0xff, 0xff, 0xff, 0x7f};
  std::u16string result;
  EXPECT_FALSE(DecodeStringMessage(kOverflowOneByteData, &result));
  EXPECT_FALSE(DecodeStringMessage(kOverflowTwoByteData, &result));
}

}  // namespace
}  // namespace blink
