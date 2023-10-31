// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_repeat_style_value.h"

#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSRepeatStyleValue::CSSRepeatStyleValue(const CSSIdentifierValue* id)
    : CSSValue(kRepeatStyleClass) {
  switch (id->GetValueID()) {
    case CSSValueID::kRepeatX:
      x_ = CSSIdentifierValue::Create(CSSValueID::kRepeat);
      y_ = CSSIdentifierValue::Create(CSSValueID::kNoRepeat);
      break;

    case CSSValueID::kRepeatY:
      x_ = CSSIdentifierValue::Create(CSSValueID::kNoRepeat);
      y_ = CSSIdentifierValue::Create(CSSValueID::kRepeat);
      break;

    default:
      x_ = y_ = id;
      break;
  }
}

CSSRepeatStyleValue::CSSRepeatStyleValue(const CSSIdentifierValue* x,
                                         const CSSIdentifierValue* y)
    : CSSValue(kRepeatStyleClass), x_(x), y_(y) {}

CSSRepeatStyleValue::~CSSRepeatStyleValue() = default;

String CSSRepeatStyleValue::CustomCSSText() const {
  StringBuilder result;

  if (base::ValuesEquivalent(x_, y_)) {
    result.Append(x_->CssText());
  } else if (x_->GetValueID() == CSSValueID::kRepeat &&
             y_->GetValueID() == CSSValueID::kNoRepeat) {
    result.Append(getValueName(CSSValueID::kRepeatX));
  } else if (x_->GetValueID() == CSSValueID::kNoRepeat &&
             y_->GetValueID() == CSSValueID::kRepeat) {
    result.Append(getValueName(CSSValueID::kRepeatY));
  } else {
    result.Append(x_->CssText());
    result.Append(' ');
    result.Append(y_->CssText());
  }

  return result.ReleaseString();
}

bool CSSRepeatStyleValue::Equals(const CSSRepeatStyleValue& other) const {
  return base::ValuesEquivalent(x_, other.x_) &&
         base::ValuesEquivalent(y_, other.y_);
}

bool CSSRepeatStyleValue::IsRepeat() const {
  return x_->GetValueID() == CSSValueID::kRepeat &&
         y_->GetValueID() == CSSValueID::kRepeat;
}

void CSSRepeatStyleValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(x_);
  visitor->Trace(y_);

  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace blink
