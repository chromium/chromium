// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/html_select_list_element.h"

#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_autofill_state.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/forms/html_button_element.h"
#include "third_party/blink/renderer/core/html/forms/html_listbox_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {
namespace {

void CheckOptions(HeapVector<Member<HTMLOptionElement>> options,
                  const std::vector<std::string>& expected_option_values) {
  ASSERT_EQ(expected_option_values.size(), options.size());
  for (wtf_size_t i = 0; i < options.size(); ++i) {
    EXPECT_EQ(options[i]->value().Utf8(), expected_option_values[i]);
  }
}

// ChromeClient which counts invocations of
// SelectOrSelectListFieldOptionsChanged().
class OptionsChangedCounterChromeClient : public EmptyChromeClient {
 public:
  OptionsChangedCounterChromeClient() = default;
  ~OptionsChangedCounterChromeClient() override = default;

  void SelectOrSelectListFieldOptionsChanged(HTMLFormControlElement&) override {
    ++option_change_notification_count_;
  }

  size_t GetOptionChangeNotificationCount() const {
    return option_change_notification_count_;
  }

 private:
  size_t option_change_notification_count_{0u};
};

}  // anonymous namespace

class HTMLSelectListElementTest : public PageTestBase {
 public:
  void SetUp() override {
    chrome_client_ = MakeGarbageCollected<OptionsChangedCounterChromeClient>();
    SetupPageWithClients(chrome_client_);
    GetDocument().SetMimeType(AtomicString("text/html"));
    GetFrame().GetSettings()->SetScriptEnabled(true);
  }

  void ExecuteJs(const std::string& js) {
    ClassicScript::CreateUnspecifiedScript(WebString::FromUTF8(js))
        ->RunScript(GetFrame().DomWindow());
  }

 protected:
  Persistent<OptionsChangedCounterChromeClient> chrome_client_;
};

// Tests that HtmlSelectListElement::SetAutofillValue() doesn't change the
// `interacted_state_` attribute of the field.
TEST_F(HTMLSelectListElementTest, SetAutofillValuePreservesEditedState) {
  SetHtmlInnerHTML(
      "<!DOCTYPE HTML><selectlist id='sel'>"
      "<option value='111' selected>111</option>"
      "<option value='222'>222</option></selectlist>");
  HTMLSelectListElement* select_list =
      To<HTMLSelectListElement>(GetElementById("sel"));

  select_list->ClearUserHasEditedTheField();
  select_list->SetAutofillValue("222", WebAutofillState::kAutofilled);
  EXPECT_EQ(select_list->UserHasEditedTheField(), false);

  select_list->SetUserHasEditedTheField();
  select_list->SetAutofillValue("111", WebAutofillState::kAutofilled);
  EXPECT_EQ(select_list->UserHasEditedTheField(), true);
}

// Test that SelectListElement::GetListItems() return value is updated upon
// adding <option>s.
TEST_F(HTMLSelectListElementTest, GetListItemsAdd) {
  SetHtmlInnerHTML(R"HTML(
    <selectlist id='selectlist'>
    <option selected>Default</option>
    </selectlist>
  )HTML");
  HTMLSelectListElement* element =
      To<HTMLSelectListElement>(GetElementById("selectlist"));

  CheckOptions(element->GetListItems(), {"Default"});

  ExecuteJs(
      "let selectlist = document.getElementById('selectlist');"
      "let option = document.createElement('option');"
      "option.textContent = 'New';"
      "selectlist.appendChild(option);");
  CheckOptions(element->GetListItems(), {"Default", "New"});
}

// Test that SelectListElement::GetListItems() return value is updated upon
// removing <option>.
TEST_F(HTMLSelectListElementTest, GetListItemsRemove) {
  SetHtmlInnerHTML(R"HTML(
    <selectlist id='selectlist'>
    <option selected>First</option>
    <option id="second_option">Second</option>
    </selectlist>
  )HTML");
  HTMLSelectListElement* element =
      To<HTMLSelectListElement>(GetElementById("selectlist"));

  CheckOptions(element->GetListItems(), {"First", "Second"});
  ExecuteJs(
      "let selectlist = document.getElementById('selectlist');"
      "let second_option = document.getElementById('second_option');"
      "selectlist.removeChild(second_option);");
  CheckOptions(element->GetListItems(), {"First"});
}

// Test that blink::ChromeClient::SelectOrSelectListFieldOptionsChanged() is
// called when <option> is added to <selectlist>.
TEST_F(HTMLSelectListElementTest, NotifyClientListItemAdd) {
  SetHtmlInnerHTML(R"HTML(
    <selectlist id='selectlist'>
    <option selected>Default</option>
    </selectlist>
  )HTML");
  HTMLSelectListElement* element =
      To<HTMLSelectListElement>(GetElementById("selectlist"));

  EXPECT_EQ(1u, element->GetListItems().size());
  size_t num_notifications_before_change =
      chrome_client_->GetOptionChangeNotificationCount();

  ExecuteJs(
      "let selectlist = document.getElementById('selectlist');"
      "let option = document.createElement('option');"
      "option.textContent = 'New';"
      "selectlist.appendChild(option);");
  EXPECT_EQ(2u, element->GetListItems().size());

  EXPECT_EQ(num_notifications_before_change + 1,
            chrome_client_->GetOptionChangeNotificationCount());
}

// Test that blink::ChromeClient::SelectOrSelectListFieldOptionsChanged() is
// called when <option> is removed from <selectlist>.
TEST_F(HTMLSelectListElementTest, NotifyClientListItemRemove) {
  SetHtmlInnerHTML(R"HTML(
    <selectlist id='selectlist'>
    <option selected>First</option>
    <option id="second_option">Second</option>
    </selectlist>
  )HTML");
  HTMLSelectListElement* element =
      To<HTMLSelectListElement>(GetElementById("selectlist"));

  EXPECT_EQ(2u, element->GetListItems().size());
  size_t num_notifications_before_change =
      chrome_client_->GetOptionChangeNotificationCount();

  ExecuteJs(
      "let selectlist = document.getElementById('selectlist');"
      "let second_option = document.getElementById('second_option');"
      "selectlist.removeChild(second_option);");
  EXPECT_EQ(1u, element->GetListItems().size());

  EXPECT_EQ(num_notifications_before_change + 1,
            chrome_client_->GetOptionChangeNotificationCount());
}

// Test behavior of HTMLSelectListElement::OwnerSelectList() if selectlist uses
// custom parts.
TEST_F(HTMLSelectListElementTest, OwnerSelectList_PartsCustomSlots) {
  SetHtmlInnerHTML(R"HTML(
    <selectlist id='selectlist'>
      <button type=selectlist id=selectlist_button>
        Button
      </button>
      <listbox id=selectlist_listbox>
        <b>
          <option id="first_option">First</option>
          <option>Second</option>
        </b>
      </listbox>
    </selectlist>
  )HTML");

  HTMLSelectListElement* select_list_element =
      To<HTMLSelectListElement>(GetElementById("selectlist"));
  EXPECT_EQ(select_list_element,
            To<HTMLButtonElement>(GetElementById("selectlist_button"))
                ->OwnerSelectList());
  EXPECT_EQ(select_list_element,
            To<HTMLListboxElement>(GetElementById("selectlist_listbox"))
                ->OwnerSelectList());
  EXPECT_EQ(select_list_element,
            To<HTMLOptionElement>(GetElementById("first_option"))
                ->OwnerSelectList());
}

// Test that HTMLSelectListElement::SetSuggestedValue() does not affect
// HTMLSelectListElement::selectedOption().
TEST_F(HTMLSelectListElementTest, SetSuggestedValue) {
  SetHtmlInnerHTML(R"HTML(
    <selectlist id='selectlist'>
      <option id="first_option" selected>First</option>
      <option id="second_option">Second</option>
    </selectlist>
  )HTML");
  HTMLSelectListElement* selectlist =
      To<HTMLSelectListElement>(GetElementById("selectlist"));
  HTMLOptionElement* first_option =
      To<HTMLOptionElement>(GetElementById("first_option"));

  ASSERT_EQ(first_option, selectlist->selectedOption());
  selectlist->SetSuggestedValue("Second");
  EXPECT_EQ("Second", selectlist->SuggestedValue());
  EXPECT_EQ(blink::WebAutofillState::kPreviewed,
            selectlist->GetAutofillState());
  EXPECT_EQ(first_option, selectlist->selectedOption());
}

// Test that passing an empty string to
// HTMLSelectListElement::SetSuggestedValue() clears autofill preview state.
TEST_F(HTMLSelectListElementTest, SetSuggestedValueEmptyString) {
  SetHtmlInnerHTML(R"HTML(
    <selectlist id='selectlist'>
      <option id="first_option" selected>First</option>
      <option id="second_option">Second</option>
    </selectlist>
  )HTML");
  HTMLSelectListElement* selectlist =
      To<HTMLSelectListElement>(GetElementById("selectlist"));

  selectlist->SetSuggestedValue("Second");
  EXPECT_EQ("Second", selectlist->SuggestedValue());
  EXPECT_EQ(blink::WebAutofillState::kPreviewed,
            selectlist->GetAutofillState());

  selectlist->SetSuggestedValue("");
  EXPECT_EQ("", selectlist->SuggestedValue());
  EXPECT_EQ(blink::WebAutofillState::kNotFilled,
            selectlist->GetAutofillState());
}

// Test that HTMLSelectListElement::SetSuggestedOption() is a noop if the
// passed-in value does not match any of the <option>s.
TEST_F(HTMLSelectListElementTest, SetSuggestedValueNoMatchingOption) {
  SetHtmlInnerHTML(R"HTML(
    <selectlist id='selectlist'>
      <option id="first_option">First</option>
      <option id="second_option">Second</option>
    </selectlist>
  )HTML");
  HTMLSelectListElement* selectlist =
      To<HTMLSelectListElement>(GetElementById("selectlist"));

  selectlist->SetSuggestedValue("nonexistent");
  EXPECT_EQ(blink::WebAutofillState::kNotFilled,
            selectlist->GetAutofillState());
}

// Test that HTMLSelectListElement::setValue() clears the suggested option.
TEST_F(HTMLSelectListElementTest, SuggestedValueClearedWhenValueSet) {
  SetHtmlInnerHTML(R"HTML(
    <selectlist id='selectlist'>
      <option selected>First</option>
      <option>Second</option>
      <option>Third</option>
    </selectlist>
  )HTML");
  HTMLSelectListElement* selectlist =
      To<HTMLSelectListElement>(GetElementById("selectlist"));

  selectlist->SetSuggestedValue("Second");
  EXPECT_EQ(blink::WebAutofillState::kPreviewed,
            selectlist->GetAutofillState());
  selectlist->setValue("Third");
  EXPECT_EQ(blink::WebAutofillState::kNotFilled,
            selectlist->GetAutofillState());
}

Color GetBorderColorForSuggestedOptionPopover(HTMLSelectListElement* element) {
  const ComputedStyle& popover_style =
      element->SuggestedOptionPopoverForTesting()->ComputedStyleRef();
  return popover_style.VisitedDependentColor(GetCSSPropertyBorderTopColor());
}

// Test HTMLSelectListElement preview popover inherits border color from the
// button when the <selectlist> button has a custom color.
TEST_F(HTMLSelectListElementTest, PreviewButtonHasCustomBorder) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      selectlist::part(button) {
        border-color:#00ff00;
      }
    </style>
    <selectlist id='selectlist'>
      <option>First</option>
    </selectlist>
  )HTML");

  HTMLSelectListElement* selectlist =
      To<HTMLSelectListElement>(GetElementById("selectlist"));
  selectlist->SetSuggestedValue("First");

  EXPECT_EQ(Color::FromRGB(0, 0xff, 0),
            GetBorderColorForSuggestedOptionPopover(selectlist));
}

// Test HTMLSelectListElement preview popover inherits border color from the
// button when the <selectlist> button has an autofill-specific custom color.
TEST_F(HTMLSelectListElementTest, PreviewButtonHasCustomAutofillBorder) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      selectlist::part(button):autofill {
        border-color:#00ff00;
      }
    </style>
    <selectlist id='selectlist'>
      <option>First</option>
    </selectlist>
  )HTML");

  HTMLSelectListElement* selectlist =
      To<HTMLSelectListElement>(GetElementById("selectlist"));
  selectlist->SetSuggestedValue("First");

  EXPECT_EQ(Color::FromRGB(0, 0xff, 0),
            GetBorderColorForSuggestedOptionPopover(selectlist));
}

// Test HTMLSelectListElement preview popover uses default color and does not
// inherit color from selectlist button when selectlist button does not specify
// a custom border color.
TEST_F(HTMLSelectListElementTest, PreviewButtonHasNoCustomBorder) {
  SetHtmlInnerHTML(R"HTML(
    <selectlist id='selectlist'>
      <option>First</option>
    </selectlist>
  )HTML");

  HTMLSelectListElement* selectlist =
      To<HTMLSelectListElement>(GetElementById("selectlist"));
  selectlist->SetSuggestedValue("First");

  EXPECT_EQ(Color::FromRGBA(0, 0, 0, 0.15 * 255),
            GetBorderColorForSuggestedOptionPopover(selectlist));
}

}  // namespace blink
