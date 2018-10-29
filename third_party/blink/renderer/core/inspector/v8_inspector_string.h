// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_V8_INSPECTOR_STRING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_V8_INSPECTOR_STRING_H_

#include <memory>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/decimal.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/string_to_number.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8-inspector.h"

namespace blink {

// Note that passed string must outlive the resulting StringView. This implies
// it must not be a temporary object.
CORE_EXPORT v8_inspector::StringView ToV8InspectorStringView(const StringView&);
CORE_EXPORT std::unique_ptr<v8_inspector::StringBuffer>
ToV8InspectorStringBuffer(const StringView&);
CORE_EXPORT String ToCoreString(const v8_inspector::StringView&);
CORE_EXPORT String ToCoreString(std::unique_ptr<v8_inspector::StringBuffer>);

namespace protocol {

class Value;

using String = WTF::String;
using StringBuilder = WTF::StringBuilder;

class CORE_EXPORT StringUtil {
  STATIC_ONLY(StringUtil);

 public:
  static String substring(const String& s, size_t pos, size_t len) {
    return s.Substring(static_cast<wtf_size_t>(pos),
                       static_cast<wtf_size_t>(len));
  }
  static String fromInteger(int64_t number) { return String::Number(number); }
  static String fromDouble(double number) {
    return Decimal::FromDouble(number).ToString();
  }
  static double toDouble(const char* s, size_t len, bool* ok) {
    return WTF::CharactersToDouble(reinterpret_cast<const LChar*>(s), len, ok);
  }
  static size_t find(const String& s, const char* needle) {
    return s.Find(needle);
  }
  static size_t find(const String& s, const String& needle) {
    return s.Find(needle);
  }
  static const size_t kNotFound = WTF::kNotFound;
  static void builderAppend(StringBuilder& builder, const String& s) {
    builder.Append(s);
  }
  static void builderAppend(StringBuilder& builder, UChar c) {
    builder.Append(c);
  }
  static void builderAppend(StringBuilder& builder, const char* s, size_t len) {
    builder.Append(s, static_cast<wtf_size_t>(len));
  }
  static void builderAppendQuotedString(StringBuilder&, const String&);
  static void builderReserve(StringBuilder& builder, uint64_t capacity) {
    builder.ReserveCapacity(static_cast<wtf_size_t>(capacity));
  }
  static String builderToString(StringBuilder& builder) {
    return builder.ToString();
  }
  static std::unique_ptr<protocol::Value> parseJSON(const String&);
};

// A read-only sequence of uninterpreted bytes with reference-counted storage.
class CORE_EXPORT Binary {
 public:
  class Impl : public RefCounted<Impl> {
   public:
    Impl() = default;
    virtual ~Impl() = default;
    virtual const uint8_t* data() const = 0;
    virtual size_t size() const = 0;
  };

  Binary() = default;

  const uint8_t* data() const { return impl_->data(); }
  size_t size() const { return impl_->size(); }

  String toBase64() const;
  static Binary fromBase64(const String& base64, bool* success);
  static Binary fromSharedBuffer(scoped_refptr<SharedBuffer> buffer);
  static Binary fromVector(Vector<uint8_t> in);

  // Note: |data.buffer_policy| must be
  // ScriptCompiler::ScriptCompiler::CachedData::BufferOwned.
  static Binary fromCachedData(
      std::unique_ptr<v8::ScriptCompiler::CachedData> data);

 private:
  explicit Binary(scoped_refptr<Impl> impl) : impl_(impl) {}
  scoped_refptr<Impl> impl_;
};
}  // namespace protocol

}  // namespace blink

// TODO(dgozman): migrate core/inspector/protocol to wtf::HashMap.
namespace std {
template <>
struct hash<WTF::String> {
  std::size_t operator()(const WTF::String& string) const {
    return StringHash::GetHash(string);
  }
};
}  // namespace std

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_V8_INSPECTOR_STRING_H_
