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
        <selectlist id="selectlist_id">
          <option value="Bar">Bar</option>
          <option value="Foo">Foo</option>
        </selectlist>
        <input id="reset" type="reset">
      </form>
    </body>
  )");

  WebFormControlElement text(
      DynamicTo<HTMLFormControlElement>(GetElementById("text_id")));
  WebFormControlElement select(
      DynamicTo<HTMLFormControlElement>(GetElementById("select_id")));
  WebFormControlElement selectlist(
      DynamicTo<HTMLFormControlElement>(GetElementById("selectlist_id")));

  text.SetUserHasEditedTheField(true);
  select.SetUserHasEditedTheField(true);
  selectlist.SetUserHasEditedTheField(true);

  EXPECT_TRUE(text.UserHasEditedTheField());
  EXPECT_TRUE(select.UserHasEditedTheField());
  EXPECT_TRUE(selectlist.UserHasEditedTheField());

  To<HTMLFormControlElement>(GetElementById("reset"))->click();

  EXPECT_FALSE(text.UserHasEditedTheField());
  EXPECT_FALSE(select.UserHasEditedTheField());
  EXPECT_FALSE(selectlist.UserHasEditedTheField());
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

using FormControlType = WebFormControlElement::Type;

class WebFormControlElementFormControlTypeTest
    : public WebFormControlElementTest,
      public testing::WithParamInterface<
          std::tuple<const char*, const char*, FormControlType>> {
 protected:
  const char* tag_name() const { return std::get<0>(GetParam()); }
  const char* attributes() const { return std::get<1>(GetParam()); }
  FormControlType expected_type() const { return std::get<2>(GetParam()); }
};

TEST_P(WebFormControlElementFormControlTypeTest, FormControlType) {
  std::string html =
      base::StringPrintf("<%s %s id=x>", tag_name(), attributes());
  if (tag_name() != std::string_view("input")) {
    html += base::StringPrintf("</%s>", tag_name());
  }
  SCOPED_TRACE(testing::Message() << html);
  GetDocument().documentElement()->setInnerHTML(html.c_str());
  auto* form_control = To<HTMLFormControlElement>(
      GetDocument().getElementById(AtomicString("x")));
  WebFormControlElement web_form_control(form_control);
  EXPECT_EQ(web_form_control.FormControlType(), expected_type())
      << form_control->FormControlType().Ascii();
}

INSTANTIATE_TEST_SUITE_P(
    WebFormControlElementTest,
    WebFormControlElementFormControlTypeTest,
    Values(
        std::make_tuple("button", "", FormControlType::kButtonSubmit),
        std::make_tuple("button",
                        "type=button",
                        FormControlType::kButtonButton),
        std::make_tuple("button",
                        "type=submit",
                        FormControlType::kButtonSubmit),
        std::make_tuple("button", "type=reset", FormControlType::kButtonReset),
        std::make_tuple("button",
                        "type=selectlist",
                        FormControlType::kButtonSelectList),
        std::make_tuple("fieldset", "", FormControlType::kFieldset),
        std::make_tuple("input", "", FormControlType::kInputText),
        std::make_tuple("input", "type=button", FormControlType::kInputButton),
        std::make_tuple("input",
                        "type=checkbox",
                        FormControlType::kInputCheckbox),
        std::make_tuple("input", "type=color", FormControlType::kInputColor),
        std::make_tuple("input", "type=date", FormControlType::kInputDate),
        // While there is a blink::input_type_names::kDatetime, <input
        // type=datetime> is just a text field.
        std::make_tuple("input", "type=datetime", FormControlType::kInputText),
        std::make_tuple("input",
                        "type=datetime-local",
                        FormControlType::kInputDatetimeLocal),
        std::make_tuple("input", "type=email", FormControlType::kInputEmail),
        std::make_tuple("input", "type=file", FormControlType::kInputFile),
        std::make_tuple("input", "type=hidden", FormControlType::kInputHidden),
        std::make_tuple("input", "type=image", FormControlType::kInputImage),
        std::make_tuple("input", "type=month", FormControlType::kInputMonth),
        std::make_tuple("input", "type=number", FormControlType::kInputNumber),
        std::make_tuple("input",
                        "type=password",
                        FormControlType::kInputPassword),
        std::make_tuple("input", "type=radio", FormControlType::kInputRadio),
        std::make_tuple("input", "type=range", FormControlType::kInputRange),
        std::make_tuple("input", "type=reset", FormControlType::kInputReset),
        std::make_tuple("input", "type=search", FormControlType::kInputSearch),
        std::make_tuple("input", "type=submit", FormControlType::kInputSubmit),
        std::make_tuple("input", "type=tel", FormControlType::kInputTelephone),
        std::make_tuple("input", "type=text", FormControlType::kInputText),
        std::make_tuple("input", "type=time", FormControlType::kInputTime),
        std::make_tuple("input", "type=url", FormControlType::kInputUrl),
        std::make_tuple("input", "type=week", FormControlType::kInputWeek),
        std::make_tuple("output", "", FormControlType::kOutput),
        std::make_tuple("select", "", FormControlType::kSelectOne),
        std::make_tuple("select", "multiple", FormControlType::kSelectMultiple),
        std::make_tuple("selectlist", "", FormControlType::kSelectList),
        std::make_tuple("textarea", "", FormControlType::kTextArea)));

// <button type=selectlist> should not be confused with <selectlist> for
// autofill.
TEST_F(WebFormControlElementTest, ButtonTypeSelectlist) {
  GetDocument().documentElement()->setInnerHTML(
      "<button id=selectbutton type=selectlist>button</button>"
      "<button id=normalbutton type=button>button</button>");
  auto selectbutton = WebFormControlElement(To<HTMLFormControlElement>(
      GetDocument().getElementById(AtomicString("selectbutton"))));
  auto normalbutton = WebFormControlElement(To<HTMLFormControlElement>(
      GetDocument().getElementById(AtomicString("normalbutton"))));
  EXPECT_EQ(normalbutton.FormControlTypeForAutofill(),
            WebFormControlElement::Type::kButtonButton);
  EXPECT_EQ(selectbutton.FormControlTypeForAutofill(),
            WebFormControlElement::Type::kButtonSelectList);
}

}  // namespace blink
