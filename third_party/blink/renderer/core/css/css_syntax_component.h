// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_SYNTAX_COMPONENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_SYNTAX_COMPONENT_H_

#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

enum class CSSSyntaxType {
  kTokenStream,
  kIdent,
  kLength,
  kNumber,
  kPercentage,
  kLengthPercentage,
  kColor,
  kImage,
  kUrl,
  kInteger,
  kAngle,
  kTime,
  kResolution,
  kTransformFunction,
  kTransformList,
  kCustomIdent,
  kString,
};

enum class CSSSyntaxRepeat { kNone, kSpaceSeparated, kCommaSeparated };

class CSSSyntaxComponent {
  DISALLOW_NEW();

 public:
  CSSSyntaxComponent(CSSSyntaxType type,
                     const String& string,
                     CSSSyntaxRepeat repeat)
      : type_(type), string_(string), repeat_(repeat) {}

  bool operator==(const CSSSyntaxComponent& a) const {
    return type_ == a.type_ && string_ == a.string_ && repeat_ == a.repeat_;
  }

  CSSSyntaxType GetType() const { return type_; }
  const String& GetString() const { return string_; }
  CSSSyntaxRepeat GetRepeat() const { return repeat_; }
  bool IsRepeatable() const { return repeat_ != CSSSyntaxRepeat::kNone; }
  bool IsInteger() const { return type_ == CSSSyntaxType::kInteger; }
  char Separator() const {
    DCHECK(IsRepeatable());
    return repeat_ == CSSSyntaxRepeat::kSpaceSeparated ? ' ' : ',';
  }

 private:
  CSSSyntaxType type_;
  String string_;  // Only used when type_ is CSSSyntaxType::kIdent
  CSSSyntaxRepeat repeat_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_SYNTAX_COMPONENT_H_
