// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_identifier_value.h"

#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/css_value_pool.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

CSSIdentifierValue* CSSIdentifierValue::Create(CSSValueID value_id) {
  CSSIdentifierValue* css_value = CssValuePool().IdentifierCacheValue(value_id);
  if (!css_value) {
    css_value = CssValuePool().SetIdentifierCacheValue(
        value_id, MakeGarbageCollected<CSSIdentifierValue>(value_id));
  }
  return css_value;
}

String CSSIdentifierValue::CustomCSSText() const {
  return AtomicString(getValueName(value_id_));
}

CSSIdentifierValue::CSSIdentifierValue(CSSValueID value_id)
    : CSSValue(kIdentifierClass), value_id_(value_id) {
  // TODO(sashab): Add a DCHECK_NE(valueID, CSSValueID::kInvalid) once no code
  // paths cause this to happen.
}

CSSIdentifierValue::CSSIdentifierValue(CSSValueID value_id, bool was_quirky)
    : CSSValue(kIdentifierClass), value_id_(value_id) {
  DCHECK_NE(value_id, CSSValueID::kInvalid);
  was_quirky_ = was_quirky;
}

CSSIdentifierValue::CSSIdentifierValue(const Length& length)
    : CSSValue(kIdentifierClass) {
  switch (length.GetType()) {
    case Length::kAuto:
      value_id_ = CSSValueID::kAuto;
      break;
    case Length::kMinContent:
      value_id_ = CSSValueID::kMinContent;
      break;
    case Length::kMaxContent:
      value_id_ = CSSValueID::kMaxContent;
      break;
    case Length::kStretch:
      value_id_ = CSSValueID::kWebkitFillAvailable;
      break;
    case Length::kFitContent:
      value_id_ = CSSValueID::kFitContent;
      break;
    case Length::kContent:
      value_id_ = CSSValueID::kContent;
      break;
    case Length::kExtendToZoom:
      value_id_ = CSSValueID::kInternalExtendToZoom;
      break;
    case Length::kPercent:
    case Length::kFixed:
    case Length::kCalculated:
    case Length::kFlex:
    case Length::kDeviceWidth:
    case Length::kDeviceHeight:
    case Length::kMinIntrinsic:
    case Length::kNone:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void CSSIdentifierValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace blink
