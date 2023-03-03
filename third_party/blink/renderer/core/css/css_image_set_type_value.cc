// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_image_set_type_value.h"

#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSImageSetTypeValue::CSSImageSetTypeValue(const String& type)
    : CSSValue(kImageSetTypeClass), type_(type) {}

CSSImageSetTypeValue::~CSSImageSetTypeValue() = default;

String CSSImageSetTypeValue::CustomCSSText() const {
  StringBuilder result;

  result.Append("type(\"");
  result.Append(type_);
  result.Append("\")");

  return result.ReleaseString();
}

bool CSSImageSetTypeValue::IsSupported() const {
  return IsSupportedImageMimeType(type_.Ascii());
}

bool CSSImageSetTypeValue::Equals(const CSSImageSetTypeValue& other) const {
  return type_ == other.type_;
}

void CSSImageSetTypeValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace blink
