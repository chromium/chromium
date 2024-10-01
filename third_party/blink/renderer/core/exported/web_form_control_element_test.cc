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
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
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
  GetDocument().documentElement()->setInnerHTML(R"(
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

class WebFormControlElementSetAutofillValueTest
    : public WebFormControlElementTest,
      public testing::WithParamInterface<const char*> {
 protected:
  void InsertHTML() {
    GetDocument().documentElement()->setInnerHTML(GetParam());
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
  GetDocument().documentElement()->setInnerHTML(
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
  GetDocument().documentElement()->setInnerHTML(
      "<textarea id=testElement maxlength='5'></textarea>");

  auto element = WebFormControlElement(To<HTMLFormControlElement>(
      GetDocument().getElementById(AtomicString("testElement"))));

  element.SetSuggestedValue("valueTooLong");
  EXPECT_EQ(element.SuggestedValue().Ascii(), "value");

  element.SetAutofillValue("valueTooLong");
  EXPECT_EQ(element.Value().Ascii(), "value");
}

}  // namespace blink
