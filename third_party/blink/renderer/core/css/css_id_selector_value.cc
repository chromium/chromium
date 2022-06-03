// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_id_selector_value.h"

#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace cssvalue {

CSSIdSelectorValue::CSSIdSelectorValue(const String& id)
    : CSSValue(kIdSelectorClass), id_(id) {}

String CSSIdSelectorValue::CustomCSSText() const {
  StringBuilder builder;
  builder.Append('#');
  SerializeIdentifier(id_, builder);
  return builder.ReleaseString();
}

void CSSIdSelectorValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace cssvalue
}  // namespace blink
