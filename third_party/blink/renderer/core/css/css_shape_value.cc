// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_shape_value.h"

#include <memory>

#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink::cssvalue {

String CSSShapeCommand::CSSText() const {
  StringBuilder builder;
  switch (type_) {
    case CSSValueID::kMove:
      builder.Append("move");
      builder.Append(end_point_origin_ == CSSValueID::kTo ? " to " : " by ");
      builder.Append(end_point_->CssText());
      break;
    case CSSValueID::kLine:
      builder.Append("line");
      builder.Append(end_point_origin_ == CSSValueID::kTo ? " to " : " by ");
      builder.Append(end_point_->CssText());
      break;
    case CSSValueID::kClose:
      builder.Append("close");
      break;
    default:
      NOTREACHED();
  }

  return builder.ReleaseString();
}

bool CSSShapeCommand::operator==(const CSSShapeCommand& other) const {
  return type_ == other.type_ && end_point_origin_ == other.end_point_origin_ &&
         base::ValuesEquivalent(end_point_, other.end_point_);
}

String CSSShapeValue::CustomCSSText() const {
  StringBuilder result;
  result.Append("shape(");
  if (wind_rule_ == RULE_EVENODD) {
    result.Append("evenodd ");
  }
  result.Append("from ");
  result.Append(origin_->CssText());
  result.Append(", ");
  bool first = true;
  for (const CSSShapeCommand* command : commands_) {
    if (!first) {
      result.Append(", ");
    }
    first = false;
    result.Append(command->CSSText());
  }
  result.Append(")");
  return result.ReleaseString();
}

void CSSShapeValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(origin_);
  visitor->Trace(commands_);
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace blink::cssvalue
