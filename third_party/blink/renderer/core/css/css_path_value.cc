// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_path_value.h"

#include <memory>

#include "third_party/blink/renderer/core/style/style_path.h"
#include "third_party/blink/renderer/core/svg/svg_path_utilities.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace cssvalue {

CSSPathValue::CSSPathValue(scoped_refptr<StylePath> style_path,
                           PathSerializationFormat serialization_format)
    : CSSValue(kPathClass),
      serialization_format_(serialization_format),
      style_path_(std::move(style_path)) {
  DCHECK(style_path_);
}

CSSPathValue::CSSPathValue(SVGPathByteStream path_byte_stream,
                           WindRule wind_rule,
                           PathSerializationFormat serialization_format)
    : CSSPathValue(StylePath::Create(std::move(path_byte_stream), wind_rule),
                   serialization_format) {}

namespace {

CSSPathValue* CreatePathValue() {
  return MakeGarbageCollected<CSSPathValue>(SVGPathByteStream());
}

}  // namespace

const CSSPathValue& CSSPathValue::EmptyPathValue() {
  DEFINE_STATIC_LOCAL(Persistent<CSSPathValue>, empty, (CreatePathValue()));
  return *empty;
}

String CSSPathValue::CustomCSSText() const {
  StringBuilder result;
  result.Append("path(");
  if (style_path_->GetWindRule() == RULE_EVENODD) {
    result.Append("evenodd, ");
  }
  result.Append("\"");
  result.Append(BuildStringFromByteStream(ByteStream(), serialization_format_));
  result.Append("\")");
  return result.ReleaseString();
}

bool CSSPathValue::Equals(const CSSPathValue& other) const {
  return ByteStream() == other.ByteStream();
}

void CSSPathValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace cssvalue
}  // namespace blink
