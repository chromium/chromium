// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/inline_style_property_map.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"

namespace blink {

TEST(InlineStylePropertyMapTest, PendingSubstitutionValueCrash) {
  // Test that trying to reify any longhands with a CSSPendingSubstitutionValue
  // does not cause a crash.

  Document* document = MakeGarbageCollected<Document>();
  Element* div = document->CreateRawElement(html_names::kDivTag);
  InlineStylePropertyMap map(div);

  // For each shorthand, create a declaration with a var() reference and try
  // reifying all longhands.
  for (CSSPropertyID property_id : CSSPropertyIDList()) {
    const CSSProperty& shorthand = CSSProperty::Get(property_id);
    if (!shorthand.IsShorthand())
      continue;
    div->SetInlineStyleProperty(property_id, "var(--dummy)");
    const StylePropertyShorthand& longhands = shorthandForProperty(property_id);
    for (unsigned i = 0; i < longhands.length(); i++) {
      map.get(document,
              longhands.properties()[i]->GetCSSPropertyName().ToAtomicString(),
              ASSERT_NO_EXCEPTION);
    }
  }
}

}  // namespace blink
