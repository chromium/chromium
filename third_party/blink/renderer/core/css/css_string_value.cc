// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_string_value.h"

#include "third_party/blink/renderer/core/css/css_attr_value_tainting.h"
#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

CSSStringValue::CSSStringValue(const String& str)
    : CSSValue(kStringClass), string_(str) {}

String CSSStringValue::CustomCSSText() const {
  StringBuilder builder;
  SerializeString(string_, builder);
  if (attr_tainted_) {
    builder.Append(GetCSSAttrTaintToken());
  }
  return builder.ReleaseString();
}

const CSSValue* CSSStringValue::TaintedCopy() const {
  if (attr_tainted_) {
    return this;
  }
  CSSStringValue* new_value = MakeGarbageCollected<CSSStringValue>(*this);
  new_value->attr_tainted_ = true;
  return new_value;
}

const CSSValue* CSSStringValue::UntaintedCopy() const {
  if (!attr_tainted_) {
    return this;
  }
  CSSStringValue* new_value = MakeGarbageCollected<CSSStringValue>(*this);
  new_value->attr_tainted_ = false;
  return new_value;
}

void CSSStringValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace blink
