// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/public/common/messaging/string_message_codec.h"

#include <string>

#include "base/containers/span.h"
#include "base/functional/overloaded.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
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
      result = WebMessageArrayBufferPayload::CreateForTesting(
          std::move(array_buffer_contents));
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

    absl::visit(
        base::Overloaded{
            [&](const std::u16string& str) {
              v8::Local<v8::String> message_as_value =
                  v8::String::NewFromTwoByte(
                      isolate, reinterpret_cast<const uint16_t*>(str.data()),
                      v8::NewStringType::kNormal, str.size())
                      .ToLocalChecked();
              EXPECT_TRUE(
                  serializer.WriteValue(context, message_as_value).ToChecked());
            },
            [&](const std::unique_ptr<WebMessageArrayBufferPayload>&
                    array_buffer) {
              // Create a new JS ArrayBuffer, then transfer into serializer.
              v8::Local<v8::ArrayBuffer> message_as_array_buffer =
                  v8::ArrayBuffer::New(isolate, array_buffer->GetLength());
              array_buffer->CopyInto(base::make_span(
                  reinterpret_cast<uint8_t*>(message_as_array_buffer->Data()),
                  message_as_array_buffer->ByteLength()));
              if (transferable) {
                serializer.TransferArrayBuffer(0, message_as_array_buffer);
                // Copy data into a new array_buffer_contents_array slot.
                mojo_base::BigBuffer big_buffer(array_buffer->GetLength());
                array_buffer->CopyInto(big_buffer);
                constexpr bool is_resizable_by_user_js = false;
                constexpr size_t max_byte_length = 0;
                transferable_message.array_buffer_contents_array.push_back(
                    mojom::SerializedArrayBufferContents::New(
                        std::move(big_buffer), is_resizable_by_user_js,
                        max_byte_length));
              }
              EXPECT_TRUE(
                  serializer.WriteValue(context, message_as_array_buffer)
                      .ToChecked());
            }},
        message);

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

void CheckStringEQ(const std::optional<WebMessagePayload>& optional_payload,
                   const std::u16string& str) {
  EXPECT_TRUE(optional_payload);
  auto& payload = optional_payload.value();
  EXPECT_TRUE(absl::holds_alternative<std::u16string>(payload));
  EXPECT_EQ(str, absl::get<std::u16string>(payload));
}

void CheckVectorEQ(const std::optional<WebMessagePayload>& optional_payload,
                   const std::vector<uint8_t>& buffer) {
  EXPECT_TRUE(optional_payload);
  auto& payload = optional_payload.value();
  EXPECT_TRUE(
      absl::holds_alternative<std::unique_ptr<WebMessageArrayBufferPayload>>(
          payload));
  auto& array_buffer =
      absl::get<std::unique_ptr<WebMessageArrayBufferPayload>>(payload);
  EXPECT_EQ(buffer.size(), array_buffer->GetLength());

  auto span = array_buffer->GetAsSpanIfPossible();
  if (span) {
    // GetAsSpan is supported, check it is the same as the original buffer.
    EXPECT_EQ(std::vector<uint8_t>(span->begin(), span->end()), buffer);
  }

  std::vector<uint8_t> temp(array_buffer->GetLength());
  array_buffer->CopyInto(base::make_span(temp));
  EXPECT_EQ(temp, buffer);
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
  CheckVectorEQ(DecodeToWebMessagePayload(EncodeWebMessagePayload(
                    WebMessageArrayBufferPayload::CreateForTesting(message))),
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
  CheckVectorEQ(DecodeWithV8(EncodeWebMessagePayload(
                    WebMessageArrayBufferPayload::CreateForTesting(message))),
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
  CheckVectorEQ(DecodeToWebMessagePayload(EncodeWithV8(
                    WebMessageArrayBufferPayload::CreateForTesting(message))),
                message);
}

TEST(StringMessageCodecTest, V8ToSelfTest_ArrayBuffer_transferrable) {
  std::vector<uint8_t> message(200, 0xFF);
  CheckVectorEQ(
      DecodeToWebMessagePayload(EncodeWithV8(
          WebMessageArrayBufferPayload::CreateForTesting(message), true)),
      message);
}

TransferableMessage TransferableMessageFromRawData(std::vector<uint8_t> data) {
  TransferableMessage message;
  message.owned_encoded_message = std::move(data);
  message.encoded_message = message.owned_encoded_message;
  return message;
}

TEST(StringMessageCodecTest, Overflow) {
  const std::vector<uint8_t> kOverflowOneByteData{'"', 0xff, 0xff, 0xff, 0x7f};
  EXPECT_FALSE(DecodeToWebMessagePayload(
      TransferableMessageFromRawData(kOverflowOneByteData)));

  const std::vector<uint8_t> kOverflowTwoByteData{'c', 0xff, 0xff, 0xff, 0x7f};
  EXPECT_FALSE(DecodeToWebMessagePayload(
      TransferableMessageFromRawData(kOverflowTwoByteData)));
}

TEST(StringMessageCodecTest, InvalidDecode) {
  auto decode_from_raw = [](std::vector<uint8_t> data) {
    return DecodeToWebMessagePayload(
        TransferableMessageFromRawData(std::move(data)));
  };

  EXPECT_FALSE(decode_from_raw({})) << "no data";
  EXPECT_FALSE(decode_from_raw({0xff, 0x01})) << "only one version";
  EXPECT_FALSE(decode_from_raw({0xff, 0x80}))
      << "end of buffer during first version";
  EXPECT_FALSE(decode_from_raw({0xff, 0x01, 0xff, 0x01}))
      << "only two versions";
  EXPECT_FALSE(decode_from_raw({0xff, 0x10, 0xff, 0x80}))
      << "end of buffer during second version";
  EXPECT_FALSE(decode_from_raw({0xff, 0x15, 0xfe, 0xff, 0x01, '"', 0x01, 'a'}))
      << "end of buffer during trailer offset";
  EXPECT_FALSE(decode_from_raw({0xff, 0x15, 0x7f, 0x00, 0x00, 0x00, 0x00,
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                0x00, 0xff, 0x10, '"',  0x01, 'a'}))
      << "unrecognized trailer offset tag";

  // Confirm that aside from the specific errors above, this encoding is
  // generally correct.
  auto valid_payload = decode_from_raw(
      {0xff, 0x15, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x10, '"',  0x01, 'a'});
  ASSERT_TRUE(valid_payload.has_value());
  ASSERT_TRUE(absl::holds_alternative<std::u16string>(*valid_payload));
  EXPECT_EQ(absl::get<std::u16string>(*valid_payload), u"a");
}

}  // namespace
}  // namespace blink
