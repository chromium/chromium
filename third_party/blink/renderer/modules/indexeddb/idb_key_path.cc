/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/indexeddb/idb_key_path.h"

#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_stringsequence.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/wtf/dtoa.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"

namespace blink {

namespace {

// The following correspond to grammar in ECMA-262.
const uint32_t kUnicodeLetter =
    WTF::unicode::kLetter_Uppercase | WTF::unicode::kLetter_Lowercase |
    WTF::unicode::kLetter_Titlecase | WTF::unicode::kLetter_Modifier |
    WTF::unicode::kLetter_Other | WTF::unicode::kNumber_Letter;
const uint32_t kUnicodeCombiningMark =
    WTF::unicode::kMark_NonSpacing | WTF::unicode::kMark_SpacingCombining;
const uint32_t kUnicodeDigit = WTF::unicode::kNumber_DecimalDigit;
const uint32_t kUnicodeConnectorPunctuation =
    WTF::unicode::kPunctuation_Connector;

static inline bool IsIdentifierStartCharacter(UChar c) {
  return (WTF::unicode::Category(c) & kUnicodeLetter) || (c == '$') ||
         (c == '_');
}

static inline bool IsIdentifierCharacter(UChar c) {
  return (WTF::unicode::Category(c) &
          (kUnicodeLetter | kUnicodeCombiningMark | kUnicodeDigit |
           kUnicodeConnectorPunctuation)) ||
         (c == '$') || (c == '_') || (c == kZeroWidthNonJoinerCharacter) ||
         (c == kZeroWidthJoinerCharacter);
}

bool IsIdentifier(const String& s) {
  wtf_size_t length = s.length();
  if (!length)
    return false;
  if (!IsIdentifierStartCharacter(s[0]))
    return false;
  for (wtf_size_t i = 1; i < length; ++i) {
    if (!IsIdentifierCharacter(s[i]))
      return false;
  }
  return true;
}

}  // namespace

bool IDBIsValidKeyPath(const String& key_path) {
  IDBKeyPathParseError error;
  Vector<String> key_path_elements;
  IDBParseKeyPath(key_path, key_path_elements, error);
  return error == kIDBKeyPathParseErrorNone;
}

void IDBParseKeyPath(const String& key_path,
                     Vector<String>& elements,
                     IDBKeyPathParseError& error) {
  // IDBKeyPath ::= EMPTY_STRING | identifier ('.' identifier)*

  if (key_path.empty()) {
    error = kIDBKeyPathParseErrorNone;
    return;
  }

  key_path.Split('.', /*allow_empty_entries=*/true, elements);
  for (const auto& element : elements) {
    if (!IsIdentifier(element)) {
      error = kIDBKeyPathParseErrorIdentifier;
      return;
    }
  }
  error = kIDBKeyPathParseErrorNone;
}

IDBKeyPath::IDBKeyPath(const class String& string)
    : type_(mojom::IDBKeyPathType::String), string_(string) {
  DCHECK(!string_.IsNull());
}

IDBKeyPath::IDBKeyPath(const Vector<class String>& array)
    : type_(mojom::IDBKeyPathType::Array), array_(array) {
#if DCHECK_IS_ON()
  for (const auto& element : array_)
    DCHECK(!element.IsNull());
#endif
}

IDBKeyPath::IDBKeyPath(const V8UnionStringOrStringSequence* key_path) {
  if (!key_path) {
    type_ = mojom::IDBKeyPathType::Null;
    return;
  }

  switch (key_path->GetContentType()) {
    case V8UnionStringOrStringSequence::ContentType::kString:
      type_ = mojom::IDBKeyPathType::String;
      string_ = key_path->GetAsString();
      DCHECK(!string_.IsNull());
      break;
    case V8UnionStringOrStringSequence::ContentType::kStringSequence:
      type_ = mojom::IDBKeyPathType::Array;
      array_ = key_path->GetAsStringSequence();
#if DCHECK_IS_ON()
      for (const auto& element : array_)
        DCHECK(!element.IsNull());
#endif
      break;
  }
}

bool IDBKeyPath::IsValid() const {
  switch (type_) {
    case mojom::IDBKeyPathType::Null:
      return false;

    case mojom::IDBKeyPathType::String:
      return IDBIsValidKeyPath(string_);

    case mojom::IDBKeyPathType::Array:
      if (array_.empty())
        return false;
      for (const auto& element : array_) {
        if (!IDBIsValidKeyPath(element))
          return false;
      }
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

v8::Local<v8::Value> IDBKeyPath::ToV8(ScriptState* script_state) const {
  v8::Isolate* isolate = script_state->GetIsolate();
  switch (type_) {
    case mojom::IDBKeyPathType::Null:
      return v8::Null(isolate);
    case mojom::IDBKeyPathType::String:
      return V8String(isolate, GetString());
    case mojom::IDBKeyPathType::Array:
      return ToV8Traits<IDLSequence<IDLString>>::ToV8(script_state, Array());
  }
  NOTREACHED_IN_MIGRATION();
  return v8::Undefined(isolate);
}

bool IDBKeyPath::operator==(const IDBKeyPath& other) const {
  if (type_ != other.type_)
    return false;

  switch (type_) {
    case mojom::IDBKeyPathType::Null:
      return true;
    case mojom::IDBKeyPathType::String:
      return string_ == other.string_;
    case mojom::IDBKeyPathType::Array:
      return array_ == other.array_;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

}  // namespace blink
