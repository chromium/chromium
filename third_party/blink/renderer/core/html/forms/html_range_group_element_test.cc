// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/html_range_group_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/html_unknown_element.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class HTMLRangeGroupElementTest : public PageTestBase {};

TEST_F(HTMLRangeGroupElementTest, CreatesRangeGroupElementWhenFeatureEnabled) {
  ScopedRangeGroupForTest scoped_feature(/*enabled=*/true);
  SetBodyInnerHTML(
      "<form id=owner></form>"
      "<rangegroup id=group form=owner name=volume>"
      "<input id=child type=range>"
      "</rangegroup>");

  auto* const group = To<HTMLRangeGroupElement>(
      GetDocument().getElementById(AtomicString("group")));
  auto* const form =
      To<HTMLFormElement>(GetDocument().getElementById(AtomicString("owner")));
  auto* const control = static_cast<HTMLFormControlElement*>(group);
  Element* const child = GetDocument().getElementById(AtomicString("child"));

  ASSERT_TRUE(child);
  EXPECT_EQ(group->tagName(), "RANGEGROUP");
  EXPECT_FALSE(group->UserAgentShadowRoot());
  EXPECT_EQ(control->formOwner(), form);
  EXPECT_EQ(control->FormControlType(),
            mojom::blink::FormControlType::kRangeGroup);
  EXPECT_EQ(control->type(), "rangegroup");
  EXPECT_FALSE(control->willValidate());
  EXPECT_FALSE(group->IsDisabledFormControl());

  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_TRUE(child->GetLayoutObject());

  GetDocument().GetSettings()->SetScriptEnabled(true);
  ClassicScript::CreateUnspecifiedScript(R"JS(
    const form = document.getElementById('owner');
    document.body.dataset.formElementsIncludesGroup =
        form.elements.namedItem('volume') ===
        document.getElementById('group');
  )JS")
      ->RunScript(GetDocument().domWindow());
  EXPECT_EQ(GetDocument().body()->getAttribute(
                AtomicString("data-form-elements-includes-group")),
            "true");
}

TEST_F(HTMLRangeGroupElementTest, ExposesReflectedIDLAttributes) {
  ScopedRangeGroupForTest scoped_feature(/*enabled=*/true);
  SetBodyInnerHTML(
      "<form id=owner></form><rangegroup id=group form=owner></rangegroup>");
  GetDocument().GetSettings()->SetScriptEnabled(true);

  ClassicScript::CreateUnspecifiedScript(R"JS(
    const group = document.getElementById('group');
    group.disabled = true;
    group.name = 'volume';
    group.min = '1';
    group.max = '9';
    group.stepBetween = '2';
    group.setAttribute('data-surface', [
      group.constructor.name,
      group.form.id,
      group.type,
      group.willValidate,
    ].join('|'));
    group.setAttribute('data-idl-to-content', [
      group.hasAttribute('disabled'),
      group.getAttribute('name'),
      group.getAttribute('min'),
      group.getAttribute('max'),
      group.getAttribute('stepbetween'),
    ].join('|'));
    group.setAttribute('name', 'balance');
    group.setAttribute('min', '3');
    group.setAttribute('max', '7');
    group.setAttribute('stepbetween', '4');
    group.setAttribute('data-content-to-idl', [
      group.disabled,
      group.name,
      group.min,
      group.max,
      group.stepBetween,
    ].join('|'));
  )JS")
      ->RunScript(GetDocument().domWindow());

  Element* const group = GetDocument().getElementById(AtomicString("group"));
  ASSERT_TRUE(group);
  EXPECT_EQ(group->getAttribute(AtomicString("data-surface")),
            "HTMLRangeGroupElement|owner|rangegroup|false");
  EXPECT_EQ(group->getAttribute(AtomicString("data-idl-to-content")),
            "true|volume|1|9|2");
  EXPECT_EQ(group->getAttribute(AtomicString("data-content-to-idl")),
            "true|balance|3|7|4");
}

TEST_F(HTMLRangeGroupElementTest, FormReturnsReferenceTargetHost) {
  ScopedRangeGroupForTest scoped_feature(/*enabled=*/true);
  SetBodyInnerHTML("<div id=host></div>");
  GetDocument().GetSettings()->SetScriptEnabled(true);

  ClassicScript::CreateUnspecifiedScript(R"JS(
    const host = document.getElementById('host');
    const shadow = host.attachShadow({
      mode: 'closed',
      referenceTarget: 'form',
    });
    const form = document.createElement('form');
    form.id = 'form';
    shadow.append(form);

    const group = document.createElement('rangegroup');
    group.name = 'volume';
    group.setAttribute('form', 'host');
    document.body.append(group);
    document.body.dataset.referenceTargetResult = [
      group.form === host,
      form.elements.namedItem('volume') === group,
    ].join('|');
  )JS")
      ->RunScript(GetDocument().domWindow());

  EXPECT_EQ(GetDocument().body()->getAttribute(
                AtomicString("data-reference-target-result")),
            "true|true");
}

TEST_F(HTMLRangeGroupElementTest, CreatesUnknownElementWhenFeatureDisabled) {
  ScopedRangeGroupForTest scoped_feature(/*enabled=*/false);
  SetBodyInnerHTML("<rangegroup id=group></rangegroup>");
  GetDocument().GetSettings()->SetScriptEnabled(true);

  Element* const group = GetDocument().getElementById(AtomicString("group"));
  ASSERT_TRUE(group);
  EXPECT_TRUE(IsA<HTMLUnknownElement>(group));

  ClassicScript::CreateUnspecifiedScript(
      "document.body.dataset.constructorType = "
      "typeof HTMLRangeGroupElement;")
      ->RunScript(GetDocument().domWindow());
  EXPECT_EQ(
      GetDocument().body()->getAttribute(AtomicString("data-constructor-type")),
      "undefined");
}

}  // namespace blink
