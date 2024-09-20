// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/v8_inspector_string.h"

#include <utility>

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/core/inspector/protocol/protocol.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "third_party/inspector_protocol/crdtp/cbor.h"

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
                  base::checked_cast<wtf_size_t>(string.length()));
  }
  return String(reinterpret_cast<const UChar*>(string.characters16()),
                base::checked_cast<wtf_size_t>(string.length()));
}

String ToCoreString(std::unique_ptr<v8_inspector::StringBuffer> buffer) {
  if (!buffer)
    return String();
  return ToCoreString(buffer->string());
}

namespace protocol {

// static
String StringUtil::fromUTF16LE(const uint16_t* data, size_t length) {
  // Chromium doesn't support big endian architectures, so it's OK to cast here.
  return String(reinterpret_cast<const UChar*>(data), length);
}

namespace {

class BinaryBasedOnUint8Vector : public Binary::Impl {
 public:
  explicit BinaryBasedOnUint8Vector(Vector<uint8_t> values)
      : values_(std::move(values)) {}

  const uint8_t* data() const override { return values_.data(); }
  size_t size() const override { return values_.size(); }

 private:
  Vector<uint8_t> values_;
};

class BinaryBasedOnCharVector : public Binary::Impl {
 public:
  explicit BinaryBasedOnCharVector(Vector<char> values)
      : values_(std::move(values)) {}

  const uint8_t* data() const override {
    return reinterpret_cast<const uint8_t*>(values_.data());
  }
  size_t size() const override { return values_.size(); }

 private:
  Vector<char> values_;
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

// Implements Serializable.
void Binary::AppendSerialized(std::vector<uint8_t>* out) const {
  crdtp::cbor::EncodeBinary(crdtp::span<uint8_t>(data(), size()), out);
}

String Binary::toBase64() const {
  return impl_ ? Base64Encode(*impl_) : String();
}

// static
Binary Binary::fromBase64(const String& base64, bool* success) {
  Vector<char> out;
  *success = WTF::Base64Decode(base64, out);
  return Binary(base::AdoptRef(new BinaryBasedOnCharVector(std::move(out))));
}

// static
Binary Binary::fromVector(Vector<uint8_t> in) {
  return Binary(base::AdoptRef(new BinaryBasedOnUint8Vector(std::move(in))));
}

// static
Binary Binary::fromSpan(base::span<const uint8_t> data) {
  Vector<uint8_t> in;
  in.AppendSpan(data);
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

namespace crdtp {

using blink::protocol::Binary;
using blink::protocol::StringUtil;

// static
bool ProtocolTypeTraits<WTF::String>::Deserialize(DeserializerState* state,
                                                  String* value) {
  auto* tokenizer = state->tokenizer();
  if (tokenizer->TokenTag() == crdtp::cbor::CBORTokenTag::STRING8) {
    const auto str = tokenizer->GetString8();
    *value = StringUtil::fromUTF8(str.data(), str.size());
    return true;
  }
  if (tokenizer->TokenTag() == crdtp::cbor::CBORTokenTag::STRING16) {
    const auto str = tokenizer->GetString16WireRep();
    *value = StringUtil::fromUTF16LE(
        reinterpret_cast<const uint16_t*>(str.data()), str.size() / 2);
    return true;
  }
  state->RegisterError(Error::BINDINGS_STRING_VALUE_EXPECTED);
  return false;
}

// static
void ProtocolTypeTraits<WTF::String>::Serialize(const String& value,
                                                std::vector<uint8_t>* bytes) {
  if (value.length() == 0) {
    crdtp::cbor::EncodeString8(span<uint8_t>(nullptr, 0),
                               bytes);  // Empty string.
    return;
  }
  if (value.Is8Bit()) {
    crdtp::cbor::EncodeFromLatin1(
        span<uint8_t>(reinterpret_cast<const uint8_t*>(value.Characters8()),
                      value.length()),
        bytes);
    return;
  }
  crdtp::cbor::EncodeFromUTF16(
      span<uint16_t>(reinterpret_cast<const uint16_t*>(value.Characters16()),
                     value.length()),
      bytes);
}

// static
bool ProtocolTypeTraits<blink::protocol::Binary>::Deserialize(
    DeserializerState* state,
    blink::protocol::Binary* value) {
  auto* tokenizer = state->tokenizer();
  if (tokenizer->TokenTag() == crdtp::cbor::CBORTokenTag::BINARY) {
    *value = Binary::fromSpan(tokenizer->GetBinary());
    return true;
  }
  if (tokenizer->TokenTag() == crdtp::cbor::CBORTokenTag::STRING8) {
    const auto str_span = tokenizer->GetString8();
    String str = StringUtil::fromUTF8(str_span.data(), str_span.size());
    bool success = false;
    *value = Binary::fromBase64(str, &success);
    return success;
  }
  state->RegisterError(Error::BINDINGS_BINARY_VALUE_EXPECTED);
  return false;
}

// static
void ProtocolTypeTraits<blink::protocol::Binary>::Serialize(
    const blink::protocol::Binary& value,
    std::vector<uint8_t>* bytes) {
  value.AppendSerialized(bytes);
}

}  // namespace crdtp
