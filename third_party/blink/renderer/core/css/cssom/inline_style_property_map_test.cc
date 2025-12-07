// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/inline_style_property_map.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(InlineStylePropertyMapTest, PendingSubstitutionValueCrash) {
  test::TaskEnvironment task_environment;
  // Test that trying to reify any longhands with a CSSPendingSubstitutionValue
  // does not cause a crash.

  ScopedNullExecutionContext execution_context;
  Document* document =
      Document::CreateForTest(execution_context.GetExecutionContext());
  Element* div = document->CreateRawElement(html_names::kDivTag);
  InlineStylePropertyMap* map =
      MakeGarbageCollected<InlineStylePropertyMap>(div);

  // For each shorthand, create a declaration with a var() reference and try
  // reifying all longhands.
  for (CSSPropertyID property_id : CSSPropertyIDList()) {
    const CSSProperty& shorthand = CSSProperty::Get(property_id);
    if (!shorthand.IsShorthand()) {
      continue;
    }
    if (shorthand.Exposure() == CSSExposure::kNone) {
      continue;
    }
    div->SetInlineStyleProperty(property_id, "var(--dummy)");
    const StylePropertyShorthand& longhands = shorthandForProperty(property_id);
    for (const CSSProperty* longhand : longhands.properties()) {
      map->get(document->GetExecutionContext(),
               longhand->GetCSSPropertyName().ToAtomicString(),
               ASSERT_NO_EXCEPTION);
    }
  }
}

}  // namespace blink
