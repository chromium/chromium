// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_form_control_element.h"

#include <vector>

#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/web/web_autofill_state.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

using mojom::blink::FormControlType;

namespace {

using ::testing::ElementsAre;
using ::testing::Values;

// A fake event listener that logs keys and codes of observed keyboard events.
class FakeEventListener final : public NativeEventListener {
 public:
  void Invoke(ExecutionContext*, Event* event) override {
    KeyboardEvent* keyboard_event = DynamicTo<KeyboardEvent>(event);
    if (!event) {
      return;
    }
    codes_.push_back(keyboard_event->code());
    keys_.push_back(keyboard_event->key());
  }

  const std::vector<String>& codes() const { return codes_; }
  const std::vector<String>& keys() const { return keys_; }

 private:
  std::vector<String> codes_;
  std::vector<String> keys_;
};

}  // namespace

class WebFormControlElementTest : public PageTestBase {
 public:
  WebFormControlElementTest() {
    feature_list_.InitAndEnableFeature(
        blink::features::kAutofillSendUnidentifiedKeyAfterFill);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that resetting a form clears the `user_has_edited_the_field_` state.
TEST_F(WebFormControlElementTest, ResetDocumentClearsEditedState) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(R"(
    <body>
      <form id="f">
        <input id="text_id">
        <select id="select_id">
          <option value="Bar">Bar</option>
          <option value="Foo">Foo</option>
        </select>
        <input id="reset" type="reset">
      </form>
    </body>
  )");

  WebFormControlElement text(
      DynamicTo<HTMLFormControlElement>(GetElementById("text_id")));
  WebFormControlElement select(
      DynamicTo<HTMLFormControlElement>(GetElementById("select_id")));

  text.SetUserHasEditedTheField(true);
  select.SetUserHasEditedTheField(true);

  EXPECT_TRUE(text.UserHasEditedTheField());
  EXPECT_TRUE(select.UserHasEditedTheField());

  To<HTMLFormControlElement>(GetElementById("reset"))->click();

  EXPECT_FALSE(text.UserHasEditedTheField());
  EXPECT_FALSE(select.UserHasEditedTheField());
}

TEST_F(WebFormControlElementTest, TextControlPreviewDisabledInCanvas) {
  if (!RuntimeEnabledFeatures::CanvasDrawElementEnabled()) {
    return;
  }

  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(R"(
    <form>
      <canvas>
        <input id="input_id">
        <textarea id="textarea_id"></textarea>
      </canvas>
    </form>
  )");

  WebFormControlElement input(
      DynamicTo<HTMLFormControlElement>(GetElementById("input_id")));
  WebFormControlElement textarea(
      DynamicTo<HTMLFormControlElement>(GetElementById("textarea_id")));

  input.SetSuggestedValue("suggestion");
  textarea.SetSuggestedValue("suggestion");

  // Elements inside canvas should not show autofill suggestions, as this can
  // leak the information to javascript.
  EXPECT_TRUE(input.SuggestedValue().IsEmpty());
  EXPECT_TRUE(textarea.SuggestedValue().IsEmpty());
}

TEST_F(WebFormControlElementTest,
       TextControlPreviewDisabledWhenMovingToCanvas) {
  if (!RuntimeEnabledFeatures::CanvasDrawElementEnabled()) {
    return;
  }

  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(R"(
    <form>
      <input id="input_id">
      <textarea id="textarea_id"></textarea>
      <canvas id="canvas"></canvas>
    </form>
  )");

  WebFormControlElement input(
      DynamicTo<HTMLFormControlElement>(GetElementById("input_id")));
  WebFormControlElement textarea(
      DynamicTo<HTMLFormControlElement>(GetElementById("textarea_id")));

  input.SetSuggestedValue("suggestion");
  textarea.SetSuggestedValue("suggestion");

  // Suggestions should work outside canvas.
  EXPECT_EQ(input.SuggestedValue().Ascii(), "suggestion");
  EXPECT_EQ(textarea.SuggestedValue().Ascii(), "suggestion");

  // Moving the element into a canvas subtree should disable autofill
  // suggestions, as these can leak the information to javascript.
  GetElementById("canvas")->appendChild(GetElementById("input_id"));
  EXPECT_TRUE(input.SuggestedValue().IsEmpty());
  GetElementById("canvas")->appendChild(GetElementById("textarea_id"));
  EXPECT_TRUE(textarea.SuggestedValue().IsEmpty());
}

TEST_F(WebFormControlElementTest, SelectPreviewDisabledInCanvas) {
  if (!RuntimeEnabledFeatures::CanvasDrawElementEnabled()) {
    return;
  }

  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(R"(
    <form>
      <canvas>
        <select id="select_id">
          <option value="Bar">Bar</option>
          <option value="Foo">Foo</option>
        </select>
      </canvas>
    </form>
  )");

  WebFormControlElement select(
      DynamicTo<HTMLFormControlElement>(GetElementById("select_id")));

  select.SetSuggestedValue("Foo");

  // Elements inside canvas should not show autofill suggestions, as this can
  // leak the information to javascript.
  EXPECT_TRUE(select.SuggestedValue().IsEmpty());
}

TEST_F(WebFormControlElementTest,
       SelectPreviewDisabledInCanvasWhenMovingToCanvas) {
  if (!RuntimeEnabledFeatures::CanvasDrawElementEnabled()) {
    return;
  }

  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(R"(
    <form>
        <select id="select_id">
          <option value="Bar">Bar</option>
          <option value="Foo">Foo</option>
        </select>
      <canvas id="canvas"></canvas>
    </form>
  )");

  WebFormControlElement select(
      DynamicTo<HTMLFormControlElement>(GetElementById("select_id")));

  select.SetSuggestedValue("Foo");

  // Suggestions should work outside canvas.
  EXPECT_EQ(select.SuggestedValue().Ascii(), "Foo");

  // Elements inside canvas should not show autofill suggestions, as this can
  // leak the information to javascript.
  GetElementById("canvas")->appendChild(GetElementById("select_id"));
  EXPECT_TRUE(select.SuggestedValue().IsEmpty());
}

class WebFormControlElementSetAutofillValueTest
    : public WebFormControlElementTest,
      public testing::WithParamInterface<const char*> {
 protected:
  void InsertHTML() {
    GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
        GetParam());
  }

  WebFormControlElement TestElement() {
    HTMLFormControlElement* control_element = DynamicTo<HTMLFormControlElement>(
        GetDocument().getElementById(AtomicString("testElement")));
    DCHECK(control_element);
    return WebFormControlElement(control_element);
  }
};

TEST_P(WebFormControlElementSetAutofillValueTest, SetAutofillValue) {
  InsertHTML();
  WebFormControlElement element = TestElement();
  auto* keypress_handler = MakeGarbageCollected<FakeEventListener>();
  element.Unwrap<HTMLFormControlElement>()->addEventListener(
      event_type_names::kKeydown, keypress_handler);

  EXPECT_EQ(TestElement().Value(), "test value");
  EXPECT_EQ(element.GetAutofillState(), WebAutofillState::kNotFilled);

  // We expect to see one "fake" key press event with an unidentified key.
  element.SetAutofillValue("new value", WebAutofillState::kAutofilled);
  EXPECT_EQ(element.Value(), "new value");
  EXPECT_EQ(element.GetAutofillState(), WebAutofillState::kAutofilled);
  EXPECT_THAT(keypress_handler->codes(), ElementsAre(""));
  EXPECT_THAT(keypress_handler->keys(), ElementsAre("Unidentified"));
}

INSTANTIATE_TEST_SUITE_P(
    WebFormControlElementTest,
    WebFormControlElementSetAutofillValueTest,
    Values("<input type='text' id=testElement value='test value'>",
           "<textarea id=testElement>test value</textarea>"));

TEST_F(WebFormControlElementTest,
       SetAutofillAndSuggestedValueMaxLengthForInput) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<input type='text' id=testElement maxlength='5'>");

  auto element = WebFormControlElement(To<HTMLFormControlElement>(
      GetDocument().getElementById(AtomicString("testElement"))));

  element.SetSuggestedValue("valueTooLong");
  EXPECT_EQ(element.SuggestedValue().Ascii(), "value");

  element.SetAutofillValue("valueTooLong");
  EXPECT_EQ(element.Value().Ascii(), "value");
}

TEST_F(WebFormControlElementTest,
       SetAutofillAndSuggestedValueMaxLengthForTextarea) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<textarea id=testElement maxlength='5'></textarea>");

  auto element = WebFormControlElement(To<HTMLFormControlElement>(
      GetDocument().getElementById(AtomicString("testElement"))));

  element.SetSuggestedValue("valueTooLong");
  EXPECT_EQ(element.SuggestedValue().Ascii(), "value");

  element.SetAutofillValue("valueTooLong");
  EXPECT_EQ(element.Value().Ascii(), "value");
}

class WebFormControlElementGetOwningFormForAutofillTest
    : public WebFormControlElementTest {
 protected:
  template <typename DocumentOrShadowRoot>
  WebFormElement GetFormElementById(
      const DocumentOrShadowRoot& document_or_shadow_root,
      std::string_view id) {
    return WebFormElement(DynamicTo<HTMLFormElement>(
        document_or_shadow_root.getElementById(WebString::FromASCII(id))));
  }  // namespace blink

  template <typename DocumentOrShadowRoot>
  WebFormControlElement GetFormControlElementById(
      const DocumentOrShadowRoot& document_or_shadow_root,
      std::string_view id) {
    return WebFormControlElement(DynamicTo<HTMLFormControlElement>(
        document_or_shadow_root.getElementById(WebString::FromASCII(id))));
  }
};

// Tests that the owning form of a form control element in light DOM is its
// associated form (i.e. the form explicitly set via form attribute or its
// closest ancestor).
TEST_F(WebFormControlElementGetOwningFormForAutofillTest,
       GetOwningFormInLightDom) {
  const Document& document = GetDocument();
  document.body()->SetHTMLUnsafeWithoutTrustedTypes(R"(
    <form id=f>
      <input id=t1>
      <input id=t2>
    </form>
    <input id=t3>)");
  WebFormElement f = GetFormElementById(document, "f");
  WebFormElement f_unowned = WebFormElement();
  WebFormControlElement t1 = GetFormControlElementById(document, "t1");
  WebFormControlElement t2 = GetFormControlElementById(document, "t2");
  WebFormControlElement t3 = GetFormControlElementById(document, "t3");
  EXPECT_EQ(t1.GetOwningFormForAutofill(), f);
  EXPECT_EQ(t2.GetOwningFormForAutofill(), f);
  EXPECT_EQ(t3.GetOwningFormForAutofill(), f_unowned);
}

// Tests that explicit association overrules DOM ancestry when determining the
// owning form.
TEST_F(WebFormControlElementGetOwningFormForAutofillTest,
       GetOwningFormInLightDomWithExplicitAssociation) {
  const Document& document = GetDocument();
  document.body()->SetHTMLUnsafeWithoutTrustedTypes(R"(
    <div>
      <form id=f1>
        <input id=t1>
        <input id=t2 form=f2>
      </form>
    </div>
    <form id=f2>
      <input id=t3>
      <input id=t4 form=f1>
      <input id=t5 form=f_unowned>
    </form>
    <input id=t6 form=f1>
    <input id=t7 form=f2>
    <input id=t8>)");
  WebFormElement f1 = GetFormElementById(document, "f1");
  WebFormElement f2 = GetFormElementById(document, "f2");
  WebFormElement f_unowned = WebFormElement();
  WebFormControlElement t1 = GetFormControlElementById(document, "t1");
  WebFormControlElement t2 = GetFormControlElementById(document, "t2");
  WebFormControlElement t3 = GetFormControlElementById(document, "t3");
  WebFormControlElement t4 = GetFormControlElementById(document, "t4");
  WebFormControlElement t5 = GetFormControlElementById(document, "t5");
  WebFormControlElement t6 = GetFormControlElementById(document, "t6");
  WebFormControlElement t7 = GetFormControlElementById(document, "t7");
  WebFormControlElement t8 = GetFormControlElementById(document, "t8");

  EXPECT_EQ(t1.GetOwningFormForAutofill(), f1);
  EXPECT_EQ(t2.GetOwningFormForAutofill(), f2);
  EXPECT_EQ(t3.GetOwningFormForAutofill(), f2);
  EXPECT_EQ(t4.GetOwningFormForAutofill(), f1);
  EXPECT_EQ(t5.GetOwningFormForAutofill(), f_unowned);
  EXPECT_EQ(t6.GetOwningFormForAutofill(), f1);
  EXPECT_EQ(t7.GetOwningFormForAutofill(), f2);
  EXPECT_EQ(t8.GetOwningFormForAutofill(), f_unowned);
}

// Tests that input elements in shadow DOM whose closest ancestor is in the
// light DOM are extracted correctly.
TEST_F(WebFormControlElementGetOwningFormForAutofillTest,
       GetOwningFormInShadowDomWithoutFormInShadowDom) {
  const Document& document = GetDocument();
  document.body()->SetHTMLUnsafeWithoutTrustedTypes(R"(
    <form id=f1>
      <div id=host1>
        <template shadowrootmode="open">
          <div>
            <input id=t1>
          </div>
        </template>
        <input id=t2>
      </div>
    </form>
    <div id=host2>
      <template shadowrootmode="open">
        <input id=t3>
      </template>
    </div>)");
  const ShadowRoot& shadow_root1 = *GetElementById("host1")->GetShadowRoot();
  const ShadowRoot& shadow_root2 = *GetElementById("host2")->GetShadowRoot();
  WebFormElement f1 = GetFormElementById(document, "f1");
  WebFormElement f_unowned = WebFormElement();
  WebFormControlElement t1 = GetFormControlElementById(shadow_root1, "t1");
  WebFormControlElement t2 = GetFormControlElementById(document, "t2");
  WebFormControlElement t3 = GetFormControlElementById(shadow_root2, "t3");

  EXPECT_EQ(t1.GetOwningFormForAutofill(), f1);
  EXPECT_EQ(t2.GetOwningFormForAutofill(), f1);
  EXPECT_EQ(t3.GetOwningFormForAutofill(), f_unowned);
}

// Tests that the owning form of a form control element is the furthest
// shadow-including ancestor form element (in absence of explicit associations).
TEST_F(WebFormControlElementGetOwningFormForAutofillTest,
       GetOwningFormInShadowDomWithFormInShadowDom) {
  const Document& document = GetDocument();
  document.body()->SetHTMLUnsafeWithoutTrustedTypes(R"(
    <form id=f1>
      <div id=host1>
        <template shadowrootmode=open>
          <div>
            <form id=f2>
              <input id=t1>
            </form>
          </div>
          <input id=t2>
        </template>
      </div>
    </form>
    <div id=host2>
      <template shadowrootmode=open>
        <form id=f3>
          <input id=t3>
        </form>
      </template>
    </div>)");
  const ShadowRoot& shadow_root1 = *GetElementById("host1")->GetShadowRoot();
  const ShadowRoot& shadow_root2 = *GetElementById("host2")->GetShadowRoot();
  WebFormElement f1 = GetFormElementById(document, "f1");
  WebFormElement f3 = GetFormElementById(shadow_root2, "f3");
  WebFormElement f_unowned = WebFormElement();
  WebFormControlElement t1 = GetFormControlElementById(shadow_root1, "t1");
  WebFormControlElement t2 = GetFormControlElementById(shadow_root1, "t2");
  WebFormControlElement t3 = GetFormControlElementById(shadow_root2, "t3");

  EXPECT_EQ(t1.GetOwningFormForAutofill(), f1);
  EXPECT_EQ(t2.GetOwningFormForAutofill(), f1);
  EXPECT_EQ(t3.GetOwningFormForAutofill(), f3);
}

// Tests that the owning form is returned correctly even if there are
// multiple levels of Shadow DOM.
TEST_F(WebFormControlElementGetOwningFormForAutofillTest,
       GetOwningFormInShadowDomWithFormInShadowDomWithMultipleLevels) {
  const Document& document = GetDocument();
  document.body()->SetHTMLUnsafeWithoutTrustedTypes(R"(
    <form id=f1>
      <div id=host1>
        <template shadowrootmode=open>
          <form id=f2>
            <input id=t1>
          </form>
          <div id=host2>
            <template shadowrootmode=open>
              <form id=f3>
                <input id=t2>
              </form>
              <input id=t3>
            </template>
            <input id=t4>
          </div>
          <input id=t5>
        </template>
      </div>
    </form>)");

  const ShadowRoot& shadow_root1 = *GetElementById("host1")->GetShadowRoot();
  const ShadowRoot& shadow_root2 =
      *shadow_root1.getElementById(AtomicString("host2"))->GetShadowRoot();
  WebFormElement f1 = GetFormElementById(document, "f1");
  WebFormElement f_unowned = WebFormElement();
  WebFormControlElement t1 = GetFormControlElementById(shadow_root1, "t1");
  WebFormControlElement t2 = GetFormControlElementById(shadow_root2, "t2");
  WebFormControlElement t3 = GetFormControlElementById(shadow_root2, "t3");
  WebFormControlElement t4 = GetFormControlElementById(shadow_root1, "t4");
  WebFormControlElement t5 = GetFormControlElementById(shadow_root1, "t5");

  EXPECT_EQ(t1.GetOwningFormForAutofill(), f1);
  EXPECT_EQ(t2.GetOwningFormForAutofill(), f1);
  EXPECT_EQ(t3.GetOwningFormForAutofill(), f1);
  EXPECT_EQ(t4.GetOwningFormForAutofill(), f1);
  EXPECT_EQ(t5.GetOwningFormForAutofill(), f1);
}

// Tests that the owning form is computed correctly for form control elements
// inside the shadow DOM that have explicit form attributes.
TEST_F(WebFormControlElementGetOwningFormForAutofillTest,
       GetOwningFormInShadowDomWithFormInShadowDomAndExplicitAssociation) {
  const Document& document = GetDocument();
  document.body()->SetHTMLUnsafeWithoutTrustedTypes(R"(
    <form id=f1>
      <div id=host1>
        <template shadowrootmode=open>
          <form id=f2>
            <input id=t1>
          </form>
          <input id=t2>
          <form id=f3>
            <input id=t3 form=f2>
          </form>
          <input id=t4 form=f2>
          <input id=t5 form=f3>
          <input id=t6 form=f1>
        </template>
      </div>
    </form>
    <div id=host2>
      <template shadowrootmode=open>
        <form id=f4>
          <input id=t7>
        </form>
      </template>
    </div>)");
  const ShadowRoot& shadow_root1 = *GetElementById("host1")->GetShadowRoot();
  const ShadowRoot& shadow_root2 = *GetElementById("host2")->GetShadowRoot();
  WebFormElement f1 = GetFormElementById(document, "f1");
  WebFormElement f4 = GetFormElementById(shadow_root2, "f4");
  WebFormElement f_unowned = WebFormElement();
  WebFormControlElement t1 = GetFormControlElementById(shadow_root1, "t1");
  WebFormControlElement t2 = GetFormControlElementById(shadow_root1, "t2");
  WebFormControlElement t3 = GetFormControlElementById(shadow_root1, "t3");
  WebFormControlElement t4 = GetFormControlElementById(shadow_root1, "t4");
  WebFormControlElement t5 = GetFormControlElementById(shadow_root1, "t5");
  WebFormControlElement t6 = GetFormControlElementById(shadow_root1, "t6");
  WebFormControlElement t7 = GetFormControlElementById(shadow_root2, "t7");

  EXPECT_EQ(t1.GetOwningFormForAutofill(), f1);
  EXPECT_EQ(t2.GetOwningFormForAutofill(), f1);
  EXPECT_EQ(t3.GetOwningFormForAutofill(), f1);
  EXPECT_EQ(t4.GetOwningFormForAutofill(), f1);
  EXPECT_EQ(t5.GetOwningFormForAutofill(), f1);
  EXPECT_EQ(t6.GetOwningFormForAutofill(), f1);
  EXPECT_EQ(t7.GetOwningFormForAutofill(), f4);
}

TEST_F(WebFormControlElementGetOwningFormForAutofillTest,
       GetOwningFormInLightDomWithSlots) {
  const Document& document = GetDocument();
  document.body()->SetHTMLUnsafeWithoutTrustedTypes(R"(
    <form id=f>
      <div>
        <template shadowrootmode=open>
          <form id=f_unowned>
            <slot></slot>
          </form>
        </template>
        <input id=t1>
      </div>
    </form>)");
  WebFormElement f = GetFormElementById(document, "f");
  WebFormControlElement t1 = GetFormControlElementById(document, "t1");
  EXPECT_EQ(t1.GetOwningFormForAutofill(), f);
}

// Tests that FormControlTypeForAutofill() == kInputPassword is sticky unless
// the type changes to a non-text type.
//
// That is, once an <input> has become an <input type=password>,
// FormControlTypeForAutofill() keeps returning kInputPassword, even if it
// the FormControlType() changes, provided it's a text-type.
TEST_F(WebFormControlElementTest, FormControlTypeForAutofill) {
  using enum FormControlType;
  const Document& document = GetDocument();
  document.body()->SetHTMLUnsafeWithoutTrustedTypes("<input id=t>");
  HTMLInputElement* input = To<HTMLInputElement>(GetElementById("t"));
  WebFormControlElement control = input;
  ASSERT_TRUE(input);
  ASSERT_TRUE(control);

  EXPECT_EQ(control.FormControlTypeForAutofill(), kInputText);
  input->setType(input_type_names::kPassword);
  EXPECT_EQ(control.FormControlTypeForAutofill(), kInputPassword);
  input->setType(input_type_names::kText);
  EXPECT_EQ(control.FormControlTypeForAutofill(), kInputPassword);
  input->setType(input_type_names::kNumber);
  EXPECT_EQ(control.FormControlTypeForAutofill(), kInputPassword);
  input->setType(input_type_names::kRadio);
  EXPECT_EQ(control.FormControlTypeForAutofill(), kInputRadio);

  // MaybeSetHasBeenPasswordField() only has an effect on IsTextType() elements.
  input->setType(input_type_names::kUrl);
  EXPECT_EQ(control.FormControlTypeForAutofill(), kInputUrl);
  input->MaybeSetHasBeenPasswordField();
  EXPECT_EQ(control.FormControlTypeForAutofill(), kInputPassword);
  input->setType(input_type_names::kRadio);
  EXPECT_EQ(control.FormControlTypeForAutofill(), kInputRadio);
  input->MaybeSetHasBeenPasswordField();
  EXPECT_EQ(control.FormControlTypeForAutofill(), kInputRadio);
}

}  // namespace blink
