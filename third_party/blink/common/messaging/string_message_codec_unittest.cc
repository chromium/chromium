// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/messaging/string_message_codec.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace blink {
namespace {

WebMessagePayload DecodeWithV8(const TransferableMessage& message) {
  base::test::TaskEnvironment task_environment;
  WebMessagePayload result;

  v8::Isolate::CreateParams params;
  params.array_buffer_allocator =
      v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  v8::Isolate* isolate = v8::Isolate::New(params);
  {
    v8::HandleScope scope(isolate);
    v8::TryCatch try_catch(isolate);

    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope lock(context);

    v8::ValueDeserializer deserializer(isolate, message.encoded_message.data(),
                                       message.encoded_message.size());
    deserializer.SetSupportsLegacyWireFormat(true);
    if (message.array_buffer_contents_array.size() == 1) {
      // Prepare to transfer ArrayBuffer first. This does not necessary mean the
      // result type is ArrayBuffer.
      mojo_base::BigBuffer& big_buffer =
          message.array_buffer_contents_array[0]->contents;
      v8::Local<v8::ArrayBuffer> message_as_array_buffer =
          v8::ArrayBuffer::New(isolate, big_buffer.size());
      memcpy(message_as_array_buffer->GetBackingStore()->Data(),
             big_buffer.data(), big_buffer.size());
      deserializer.TransferArrayBuffer(0, message_as_array_buffer);
    }
    EXPECT_TRUE(deserializer.ReadHeader(context).ToChecked());

    v8::Local<v8::Value> value =
        deserializer.ReadValue(context).ToLocalChecked();
    if (value->IsString()) {
      v8::Local<v8::String> js_str = value->ToString(context).ToLocalChecked();
      std::u16string str;
      str.resize(js_str->Length());
      js_str->Write(isolate, reinterpret_cast<uint16_t*>(&str[0]), 0,
                    str.size());
      result = str;
    }
    if (value->IsArrayBuffer()) {
      auto js_array_buffer = value.As<v8::ArrayBuffer>()->GetBackingStore();
      std::vector<uint8_t> array_buffer_contents;
      array_buffer_contents.resize(js_array_buffer->ByteLength());
      memcpy(array_buffer_contents.data(), js_array_buffer->Data(),
             js_array_buffer->ByteLength());
      result = array_buffer_contents;
    }
  }
  isolate->Dispose();
  delete params.array_buffer_allocator;

  return result;
}

TransferableMessage EncodeWithV8(const WebMessagePayload& message,
                                 const bool transferable = false) {
  TransferableMessage transferable_message;
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
    v8::Context::Scope lock(context);
    v8::ValueSerializer serializer(isolate);
    serializer.WriteHeader();

    if (const auto* str = absl::get_if<std::u16string>(&message)) {
      v8::Local<v8::String> message_as_value =
          v8::String::NewFromTwoByte(
              isolate, reinterpret_cast<const uint16_t*>(str->data()),
              v8::NewStringType::kNormal, str->size())
              .ToLocalChecked();
      EXPECT_TRUE(serializer.WriteValue(context, message_as_value).ToChecked());
    } else if (const auto* array_buffer =
                   absl::get_if<std::vector<uint8_t>>(&message)) {
      v8::Local<v8::ArrayBuffer> message_as_array_buffer =
          v8::ArrayBuffer::New(isolate, array_buffer->size());
      memcpy(message_as_array_buffer->GetBackingStore()->Data(),
             array_buffer->data(), array_buffer->size());
      if (transferable) {
        serializer.TransferArrayBuffer(0, message_as_array_buffer);
      }
      EXPECT_TRUE(
          serializer.WriteValue(context, message_as_array_buffer).ToChecked());

      mojo_base::BigBuffer big_buffer(*array_buffer);
      transferable_message.array_buffer_contents_array.push_back(
          mojom::SerializedArrayBufferContents::New(std::move(big_buffer)));
    } else {
      NOTREACHED();
    }

    std::pair<uint8_t*, size_t> buffer = serializer.Release();
    result = std::vector<uint8_t>(buffer.first, buffer.first + buffer.second);
    free(buffer.first);
  }
  isolate->Dispose();
  delete params.array_buffer_allocator;

  transferable_message.owned_encoded_message = std::move(result);
  transferable_message.encoded_message =
      transferable_message.owned_encoded_message;
  return transferable_message;
}

void CheckStringEQ(const absl::optional<WebMessagePayload>& optional_payload,
                   const std::u16string& str) {
  EXPECT_TRUE(optional_payload);
  auto& payload = optional_payload.value();
  EXPECT_TRUE(absl::holds_alternative<std::u16string>(payload));
  EXPECT_EQ(str, absl::get<std::u16string>(payload));
}

void CheckVectorEQ(const absl::optional<WebMessagePayload>& optional_payload,
                   const std::vector<uint8_t>& buffer) {
  EXPECT_TRUE(optional_payload);
  auto& payload = optional_payload.value();
  EXPECT_TRUE(absl::holds_alternative<std::vector<uint8_t>>(payload));
  const auto& vec = absl::get<std::vector<uint8_t>>(payload);
  EXPECT_EQ(buffer.size(), vec.size());
  for (size_t i = 0; i < buffer.size(); ++i)
    EXPECT_EQ(buffer[i], vec[i]);
}

TEST(StringMessageCodecTest, SelfTest_ASCII) {
  std::u16string message = u"hello";
  CheckStringEQ(DecodeToWebMessagePayload(
                    EncodeWebMessagePayload(WebMessagePayload(message))),
                message);
}

TEST(StringMessageCodecTest, SelfTest_Latin1) {
  std::u16string message = u"hello \u00E7";
  CheckStringEQ(DecodeToWebMessagePayload(
                    EncodeWebMessagePayload(WebMessagePayload(message))),
                message);
}

TEST(StringMessageCodecTest, SelfTest_TwoByte) {
  std::u16string message = u"hello \u263A";
  CheckStringEQ(DecodeToWebMessagePayload(
                    EncodeWebMessagePayload(WebMessagePayload(message))),
                message);
}

TEST(StringMessageCodecTest, SelfTest_TwoByteLongEnoughToForcePadding) {
  std::u16string message(200, 0x263A);
  CheckStringEQ(DecodeToWebMessagePayload(
                    EncodeWebMessagePayload(WebMessagePayload(message))),
                message);
}

TEST(StringMessageCodecTest, SelfTest_ArrayBuffer) {
  std::vector<uint8_t> message(200, 0xFF);
  CheckVectorEQ(DecodeToWebMessagePayload(
                    EncodeWebMessagePayload(WebMessagePayload(message))),
                message);
}

TEST(StringMessageCodecTest, SelfToV8Test_ASCII) {
  std::u16string message = u"hello";
  CheckStringEQ(
      DecodeWithV8(EncodeWebMessagePayload(WebMessagePayload(message))),
      message);
}

TEST(StringMessageCodecTest, SelfToV8Test_Latin1) {
  std::u16string message = u"hello \u00E7";
  CheckStringEQ(
      DecodeWithV8(EncodeWebMessagePayload(WebMessagePayload(message))),
      message);
}

TEST(StringMessageCodecTest, SelfToV8Test_TwoByte) {
  std::u16string message = u"hello \u263A";
  CheckStringEQ(
      DecodeWithV8(EncodeWebMessagePayload(WebMessagePayload(message))),
      message);
}

TEST(StringMessageCodecTest, SelfToV8Test_TwoByteLongEnoughToForcePadding) {
  std::u16string message(200, 0x263A);
  CheckStringEQ(
      DecodeWithV8(EncodeWebMessagePayload(WebMessagePayload(message))),
      message);
}

TEST(StringMessageCodecTest, SelfToV8Test_ArrayBuffer) {
  std::vector<uint8_t> message(200, 0xFF);
  CheckVectorEQ(
      DecodeWithV8(EncodeWebMessagePayload(WebMessagePayload(message))),
      message);
}

TEST(StringMessageCodecTest, V8ToSelfTest_ASCII) {
  std::u16string message = u"hello";
  CheckStringEQ(DecodeToWebMessagePayload(EncodeWithV8(message)), message);
}

TEST(StringMessageCodecTest, V8ToSelfTest_Latin1) {
  std::u16string message = u"hello \u00E7";
  CheckStringEQ(DecodeToWebMessagePayload(EncodeWithV8(message)), message);
}

TEST(StringMessageCodecTest, V8ToSelfTest_TwoByte) {
  std::u16string message = u"hello \u263A";
  CheckStringEQ(DecodeToWebMessagePayload(EncodeWithV8(message)), message);
}

TEST(StringMessageCodecTest, V8ToSelfTest_TwoByteLongEnoughToForcePadding) {
  std::u16string message(200, 0x263A);
  CheckStringEQ(DecodeToWebMessagePayload(EncodeWithV8(message)), message);
}

TEST(StringMessageCodecTest, V8ToSelfTest_ArrayBuffer) {
  std::vector<uint8_t> message(200, 0xFF);
  CheckVectorEQ(DecodeToWebMessagePayload(EncodeWithV8(message)), message);
}

TEST(StringMessageCodecTest, V8ToSelfTest_ArrayBuffer_transferrable) {
  std::vector<uint8_t> message(200, 0xFF);
  CheckVectorEQ(DecodeToWebMessagePayload(EncodeWithV8(message, true)),
                message);
}

TEST(StringMessageCodecTest, Overflow) {
  const std::vector<uint8_t> kOverflowOneByteData{'"', 0xff, 0xff, 0xff, 0x7f};
  const std::vector<uint8_t> kOverflowTwoByteData{'c', 0xff, 0xff, 0xff, 0x7f};

  TransferableMessage one_byte_message;
  one_byte_message.owned_encoded_message = kOverflowOneByteData;
  one_byte_message.encoded_message = one_byte_message.owned_encoded_message;

  TransferableMessage two_byte_message;
  two_byte_message.owned_encoded_message = kOverflowTwoByteData;
  two_byte_message.encoded_message = two_byte_message.owned_encoded_message;

  EXPECT_FALSE(DecodeToWebMessagePayload(one_byte_message));
  EXPECT_FALSE(DecodeToWebMessagePayload(two_byte_message));
}

}  // namespace
}  // namespace blink
