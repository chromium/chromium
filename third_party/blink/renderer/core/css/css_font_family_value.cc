// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_font_family_value.h"

#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/css_value_pool.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

CSSFontFamilyValue* CSSFontFamilyValue::Create(
    const AtomicString& family_name) {
  if (family_name.IsNull()) {
    return MakeGarbageCollected<CSSFontFamilyValue>(family_name);
  }
  CSSValuePool::FontFamilyValueCache::AddResult entry =
      CssValuePool().GetFontFamilyCacheEntry(family_name);
  if (!entry.stored_value->value) {
    entry.stored_value->value =
        MakeGarbageCollected<CSSFontFamilyValue>(family_name);
  }
  return entry.stored_value->value.Get();
}

CSSFontFamilyValue::CSSFontFamilyValue(const AtomicString& str)
    : CSSValue(kFontFamilyClass), string_(str) {}

String CSSFontFamilyValue::CustomCSSText() const {
  return SerializeFontFamily(string_);
}

void CSSFontFamilyValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace blink
