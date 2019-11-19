// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_unsupported_color_value.h"

#include "third_party/blink/renderer/core/css/css_color_value.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_inherited_value.h"
#include "third_party/blink/renderer/core/css/css_initial_value.h"
#include "third_party/blink/renderer/core/css/css_unset_value.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

CSSUnsupportedColorValue* CSSUnsupportedColorValue::Create(Color color) {
  return MakeGarbageCollected<CSSUnsupportedColorValue>(color);
}

CSSUnsupportedColorValue* CSSUnsupportedColorValue::Create(
    const CSSPropertyName& name,
    Color color) {
  return MakeGarbageCollected<CSSUnsupportedColorValue>(name, color);
}

CSSUnsupportedColorValue* CSSUnsupportedColorValue::FromCSSValue(
    const cssvalue::CSSColorValue& color_value) {
  return MakeGarbageCollected<CSSUnsupportedColorValue>(color_value.Value());
}

Color CSSUnsupportedColorValue::Value() const {
  return color_value_;
}

const CSSValue* CSSUnsupportedColorValue::ToCSSValue() const {
  return cssvalue::CSSColorValue::Create(
      MakeRGBA(color_value_.Red(), color_value_.Green(), color_value_.Blue(),
               color_value_.Alpha()));
}

}  // namespace blink
