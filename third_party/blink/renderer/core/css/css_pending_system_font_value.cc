// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_pending_system_font_value.h"

#include "third_party/blink/renderer/core/css/parser/css_parser_fast_paths.h"
#include "third_party/blink/renderer/core/layout/layout_theme_font_provider.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace cssvalue {

CSSPendingSystemFontValue::CSSPendingSystemFontValue(CSSValueID system_font_id)
    : CSSValue(kPendingSystemFontValueClass), system_font_id_(system_font_id) {
  DCHECK(CSSParserFastPaths::IsValidSystemFont(system_font_id));
}

// static
CSSPendingSystemFontValue* CSSPendingSystemFontValue::Create(
    CSSValueID system_font_id) {
  return MakeGarbageCollected<CSSPendingSystemFontValue>(system_font_id);
}

const AtomicString& CSSPendingSystemFontValue::ResolveFontFamily() const {
  return LayoutThemeFontProvider::SystemFontFamily(system_font_id_);
}

float CSSPendingSystemFontValue::ResolveFontSize(
    const Document* document) const {
  return LayoutThemeFontProvider::SystemFontSize(system_font_id_, document);
}

String CSSPendingSystemFontValue::CustomCSSText() const {
  return "";
}

void CSSPendingSystemFontValue::TraceAfterDispatch(
    blink::Visitor* visitor) const {
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace cssvalue
}  // namespace blink
