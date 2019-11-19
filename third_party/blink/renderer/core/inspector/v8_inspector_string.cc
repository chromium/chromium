// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/v8_inspector_string.h"

#include <utility>
#include "third_party/blink/renderer/core/inspector/protocol/Protocol.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"

namespace blink {

v8_inspector::StringView ToV8InspectorStringView(const StringView& string) {
  if (string.IsNull())
    return v8_inspector::StringView();
  if (string.Is8Bit())
    return v8_inspector::StringView(
        reinterpret_cast<const uint8_t*>(string.Characters8()),
        string.length());
  return v8_inspector::StringView(
      reinterpret_cast<const uint16_t*>(string.Characters16()),
      string.length());
}

std::unique_ptr<v8_inspector::StringBuffer> ToV8InspectorStringBuffer(
    const StringView& string) {
  return v8_inspector::StringBuffer::create(ToV8InspectorStringView(string));
}

String ToCoreString(const v8_inspector::StringView& string) {
  if (string.is8Bit()) {
    return String(reinterpret_cast<const LChar*>(string.characters8()),
                  SafeCast<wtf_size_t>(string.length()));
  }
  return String(reinterpret_cast<const UChar*>(string.characters16()),
                SafeCast<wtf_size_t>(string.length()));
}

String ToCoreString(std::unique_ptr<v8_inspector::StringBuffer> buffer) {
  if (!buffer)
    return String();
  return ToCoreString(buffer->string());
}

namespace protocol {

// static
std::unique_ptr<protocol::Value> StringUtil::parseJSON(const String& string) {
  if (string.IsNull())
    return nullptr;
  if (string.Is8Bit()) {
    return parseJSONCharacters(
        reinterpret_cast<const uint8_t*>(string.Characters8()),
        string.length());
  }
  return parseJSONCharacters(
      reinterpret_cast<const uint16_t*>(string.Characters16()),
      string.length());
}

// static
void StringUtil::builderAppendQuotedString(StringBuilder& builder,
                                           const String& str) {
  builder.Append('"');
  if (!str.IsEmpty()) {
    if (str.Is8Bit()) {
      escapeLatinStringForJSON(
          reinterpret_cast<const uint8_t*>(str.Characters8()), str.length(),
          &builder);
    } else {
      escapeWideStringForJSON(
          reinterpret_cast<const uint16_t*>(str.Characters16()), str.length(),
          &builder);
    }
  }
  builder.Append('"');
}

// static
String StringUtil::fromUTF16LE(const uint16_t* data, size_t length) {
  // Chromium doesn't support big endian architectures, so it's OK to cast here.
  return String(reinterpret_cast<const UChar*>(data), length);
}

namespace {
class BinaryBasedOnSharedBuffer : public Binary::Impl {
 public:
  explicit BinaryBasedOnSharedBuffer(scoped_refptr<SharedBuffer> buffer)
      : buffer_(std::move(buffer)) {}

  const uint8_t* data() const override {
    return reinterpret_cast<const uint8_t*>(buffer_->Data());
  }
  size_t size() const override { return buffer_->size(); }

 private:
  // buffer_ is mutable so we can call SharedBuffer::Data(),
  // which flattens the segments of the buffer.
  mutable scoped_refptr<SharedBuffer> buffer_;
};

class BinaryBasedOnVector : public Binary::Impl {
 public:
  explicit BinaryBasedOnVector(Vector<uint8_t> values)
      : values_(std::move(values)) {}

  const uint8_t* data() const override { return values_.data(); }
  size_t size() const override { return values_.size(); }

 private:
  Vector<uint8_t> values_;
};

class BinaryBasedOnCachedData : public Binary::Impl {
 public:
  explicit BinaryBasedOnCachedData(
      std::unique_ptr<v8::ScriptCompiler::CachedData> data)
      : data_(std::move(data)) {}

  const uint8_t* data() const override { return data_->data; }
  size_t size() const override { return data_->length; }

 private:
  std::unique_ptr<v8::ScriptCompiler::CachedData> data_;
};
}  // namespace

String Binary::toBase64() const {
  return impl_ ? Base64Encode(*impl_) : String();
}

// static
Binary Binary::fromBase64(const String& base64, bool* success) {
  Vector<char> out;
  *success = WTF::Base64Decode(base64, out);
  return Binary(base::AdoptRef(
      new BinaryBasedOnSharedBuffer(SharedBuffer::AdoptVector(out))));
}

// static
Binary Binary::fromSharedBuffer(scoped_refptr<SharedBuffer> buffer) {
  return Binary(
      base::AdoptRef(new BinaryBasedOnSharedBuffer(std::move(buffer))));
}

// static
Binary Binary::fromVector(Vector<uint8_t> in) {
  return Binary(base::AdoptRef(new BinaryBasedOnVector(std::move(in))));
}

// static
Binary Binary::fromSpan(const uint8_t* data, size_t size) {
  Vector<uint8_t> in;
  in.Append(data, size);
  return Binary::fromVector(std::move(in));
}

// static
Binary Binary::fromCachedData(
    std::unique_ptr<v8::ScriptCompiler::CachedData> data) {
  CHECK_EQ(data->buffer_policy, v8::ScriptCompiler::CachedData::BufferOwned);
  return Binary(base::AdoptRef(new BinaryBasedOnCachedData(std::move(data))));
}

}  // namespace protocol
}  // namespace blink
