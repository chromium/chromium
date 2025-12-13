// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TEXT_OVERFLOW_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TEXT_OVERFLOW_DATA_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// Represent CSS text-overflow: [ clip | ellipsis | <string> ]
class CORE_EXPORT TextOverflowData final {
  DISALLOW_NEW();

 public:
  enum class Type : uint8_t { kClip, kEllipsis, kString };

  explicit TextOverflowData(Type type)
      : type_(type), string_value_(g_empty_string) {
    DCHECK_NE(type, Type::kString);
  }
  explicit TextOverflowData(String string_value)
      : type_(Type::kString), string_value_(string_value) {}

  bool operator==(const TextOverflowData& other) const {
    return type_ == other.type_ && string_value_ == other.string_value_;
  }

  bool IsClip() const { return type_ == Type::kClip; }
  bool IsEllipsis() const { return type_ == Type::kEllipsis; }
  bool IsString() const { return type_ == Type::kString; }

  Type GetType() const { return type_; }
  const String& StringValue() const { return string_value_; }

 private:
  Type type_;
  String string_value_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TEXT_OVERFLOW_DATA_H_
