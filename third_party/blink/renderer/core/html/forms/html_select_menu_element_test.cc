// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/html_select_menu_element.h"

#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_autofill_state.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
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
// SelectOrSelectMenuFieldOptionsChanged().
class OptionsChangedCounterChromeClient : public EmptyChromeClient {
 public:
  OptionsChangedCounterChromeClient() = default;
  ~OptionsChangedCounterChromeClient() override = default;

  void SelectOrSelectMenuFieldOptionsChanged(HTMLFormControlElement&) override {
    ++option_change_notification_count_;
  }

  size_t GetOptionChangeNotificationCount() const {
    return option_change_notification_count_;
  }

 private:
  size_t option_change_notification_count_{0u};
};

}  // anonymous namespace

class HTMLSelectMenuElementTest : public PageTestBase {
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

// Tests that HtmlSelectMenuElement::SetAutofillValue() doesn't change the
// `user_has_edited_the_field_` attribute of the field.
TEST_F(HTMLSelectMenuElementTest, SetAutofillValuePreservesEditedState) {
  SetHtmlInnerHTML(
      "<!DOCTYPE HTML><selectmenu id='sel'>"
      "<option value='111' selected>111</option>"
      "<option value='222'>222</option></selectmenu>");
  HTMLSelectMenuElement* select_menu =
      To<HTMLSelectMenuElement>(GetElementById("sel"));

  select_menu->SetUserHasEditedTheField(false);
  select_menu->SetAutofillValue("222", WebAutofillState::kAutofilled);
  EXPECT_EQ(select_menu->UserHasEditedTheField(), false);

  select_menu->SetUserHasEditedTheField(true);
  select_menu->SetAutofillValue("111", WebAutofillState::kAutofilled);
  EXPECT_EQ(select_menu->UserHasEditedTheField(), true);
}

// Test that SelectMenuElement::GetListItems() return value is updated upon
// adding <option>s.
TEST_F(HTMLSelectMenuElementTest, GetListItemsAdd) {
  SetHtmlInnerHTML(R"HTML(
    <selectmenu id='selectmenu'>
    <option selected>Default</option>
    </selectmenu>
  )HTML");
  HTMLSelectMenuElement* element =
      To<HTMLSelectMenuElement>(GetElementById("selectmenu"));

  CheckOptions(element->GetListItems(), {"Default"});

  ExecuteJs(
      "let selectmenu = document.getElementById('selectmenu');"
      "let option = document.createElement('option');"
      "option.textContent = 'New';"
      "selectmenu.appendChild(option);");
  CheckOptions(element->GetListItems(), {"Default", "New"});
}

// Test that SelectMenuElement::GetListItems() return value is updated upon
// removing <option>.
TEST_F(HTMLSelectMenuElementTest, GetListItemsRemove) {
  SetHtmlInnerHTML(R"HTML(
    <selectmenu id='selectmenu'>
    <option selected>First</option>
    <option id="second_option">Second</option>
    </selectmenu>
  )HTML");
  HTMLSelectMenuElement* element =
      To<HTMLSelectMenuElement>(GetElementById("selectmenu"));

  CheckOptions(element->GetListItems(), {"First", "Second"});
  ExecuteJs(
      "let selectmenu = document.getElementById('selectmenu');"
      "let second_option = document.getElementById('second_option');"
      "selectmenu.removeChild(second_option);");
  CheckOptions(element->GetListItems(), {"First"});
}

// Test that blink::ChromeClient::SelectOrSelectMenuFieldOptionsChanged() is
// called when <option> is added to <selectmenu>.
TEST_F(HTMLSelectMenuElementTest, NotifyClientListItemAdd) {
  SetHtmlInnerHTML(R"HTML(
    <selectmenu id='selectmenu'>
    <option selected>Default</option>
    </selectmenu>
  )HTML");
  HTMLSelectMenuElement* element =
      To<HTMLSelectMenuElement>(GetElementById("selectmenu"));

  EXPECT_EQ(1u, element->GetListItems().size());
  size_t num_notifications_before_change =
      chrome_client_->GetOptionChangeNotificationCount();

  ExecuteJs(
      "let selectmenu = document.getElementById('selectmenu');"
      "let option = document.createElement('option');"
      "option.textContent = 'New';"
      "selectmenu.appendChild(option);");
  EXPECT_EQ(2u, element->GetListItems().size());

  EXPECT_EQ(num_notifications_before_change + 1,
            chrome_client_->GetOptionChangeNotificationCount());
}

// Test that blink::ChromeClient::SelectOrSelectMenuFieldOptionsChanged() is
// called when <option> is removed from <selectmenu>.
TEST_F(HTMLSelectMenuElementTest, NotifyClientListItemRemove) {
  SetHtmlInnerHTML(R"HTML(
    <selectmenu id='selectmenu'>
    <option selected>First</option>
    <option id="second_option">Second</option>
    </selectmenu>
  )HTML");
  HTMLSelectMenuElement* element =
      To<HTMLSelectMenuElement>(GetElementById("selectmenu"));

  EXPECT_EQ(2u, element->GetListItems().size());
  size_t num_notifications_before_change =
      chrome_client_->GetOptionChangeNotificationCount();

  ExecuteJs(
      "let selectmenu = document.getElementById('selectmenu');"
      "let second_option = document.getElementById('second_option');"
      "selectmenu.removeChild(second_option);");
  EXPECT_EQ(1u, element->GetListItems().size());

  EXPECT_EQ(num_notifications_before_change + 1,
            chrome_client_->GetOptionChangeNotificationCount());
}

// Test behavior of HTMLSelectMenuElement::OwnerSelectMenu() if selectmenu uses
// default parts.
TEST_F(HTMLSelectMenuElementTest, OwnerSelectMenu_Parts) {
  SetHtmlInnerHTML(R"HTML(
    <selectmenu id='selectmenu'>
    <b>
      <option>First</option>
      <option>Second</option>
    </b>
    </selectmenu>
  )HTML");

  HTMLSelectMenuElement* select_menu_element =
      To<HTMLSelectMenuElement>(GetElementById("selectmenu"));
  EXPECT_EQ(select_menu_element, HTMLSelectMenuElement::OwnerSelectMenu(
                                     select_menu_element->selectedOption()));
  EXPECT_EQ(select_menu_element, HTMLSelectMenuElement::OwnerSelectMenu(
                                     select_menu_element->ButtonPart()));
}

// Test behavior of HTMLSelectMenuElement::OwnerSelectMenu() if selectmenu uses
// custom parts.
TEST_F(HTMLSelectMenuElementTest, OwnerSelectMenu_PartsCustomSlots) {
  SetHtmlInnerHTML(R"HTML(
    <selectmenu id='selectmenu'>
      <div behavior="button" slot="button" id="selectmenu_button">
        Button
      </div>
      <div behavior="listbox" slot="listbox" id="selectmenu_listbox" popover>
        <b>
          <option id="first_option">First</option>
          <option>Second</option>
        </b>
      </div>
    </selectmenu>
  )HTML");

  HTMLSelectMenuElement* select_menu_element =
      To<HTMLSelectMenuElement>(GetElementById("selectmenu"));
  EXPECT_EQ(select_menu_element, HTMLSelectMenuElement::OwnerSelectMenu(
                                     GetElementById("selectmenu_button")));
  EXPECT_EQ(select_menu_element, HTMLSelectMenuElement::OwnerSelectMenu(
                                     GetElementById("selectmenu_listbox")));
  ASSERT_EQ(select_menu_element, HTMLSelectMenuElement::OwnerSelectMenu(
                                     GetElementById("first_option")));
}

// Test behavior of HTMLSelectMenuElement::OwnerSelectMenu() when a node which
// is not a descendant of the selectmenu is passed.
TEST_F(HTMLSelectMenuElementTest, OwnerSelectMenu_NotInSelectMenu) {
  SetHtmlInnerHTML(R"HTML(
    <selectmenu id='selectmenu'>
      <option>First</option>
      <option>Second</option>
    </selectmenu>
    <div id="Other">other</div>
  )HTML");
  EXPECT_EQ(nullptr,
            HTMLSelectMenuElement::OwnerSelectMenu(GetElementById("other")));
}

// Test that HTMLSelectMenuElement::SetSuggestedValue() does not affect
// HTMLSelectMenuElement::selectedOption().
TEST_F(HTMLSelectMenuElementTest, SetSuggestedValue) {
  SetHtmlInnerHTML(R"HTML(
    <selectmenu id='selectmenu'>
      <option id="first_option" selected>First</option>
      <option id="second_option">Second</option>
    </selectmenu>
  )HTML");
  HTMLSelectMenuElement* selectmenu =
      To<HTMLSelectMenuElement>(GetElementById("selectmenu"));
  HTMLOptionElement* first_option =
      To<HTMLOptionElement>(GetElementById("first_option"));

  ASSERT_EQ(first_option, selectmenu->selectedOption());
  selectmenu->SetSuggestedValue("Second");
  EXPECT_EQ("Second", selectmenu->SuggestedValue());
  EXPECT_EQ(blink::WebAutofillState::kPreviewed,
            selectmenu->GetAutofillState());
  EXPECT_EQ(first_option, selectmenu->selectedOption());
}

// Test that passing an empty string to
// HTMLSelectMenuElement::SetSuggestedValue() clears autofill preview state.
TEST_F(HTMLSelectMenuElementTest, SetSuggestedValueEmptyString) {
  SetHtmlInnerHTML(R"HTML(
    <selectmenu id='selectmenu'>
      <option id="first_option" selected>First</option>
      <option id="second_option">Second</option>
    </selectmenu>
  )HTML");
  HTMLSelectMenuElement* selectmenu =
      To<HTMLSelectMenuElement>(GetElementById("selectmenu"));

  selectmenu->SetSuggestedValue("Second");
  EXPECT_EQ("Second", selectmenu->SuggestedValue());
  EXPECT_EQ(blink::WebAutofillState::kPreviewed,
            selectmenu->GetAutofillState());

  selectmenu->SetSuggestedValue("");
  EXPECT_EQ("", selectmenu->SuggestedValue());
  EXPECT_EQ(blink::WebAutofillState::kNotFilled,
            selectmenu->GetAutofillState());
}

// Test that HTMLSelectMenuElement::SetSuggestedOption() is a noop if the
// passed-in value does not match any of the <option>s.
TEST_F(HTMLSelectMenuElementTest, SetSuggestedValueNoMatchingOption) {
  SetHtmlInnerHTML(R"HTML(
    <selectmenu id='selectmenu'>
      <option id="first_option">First</option>
      <option id="second_option">Second</option>
    </selectmenu>
  )HTML");
  HTMLSelectMenuElement* selectmenu =
      To<HTMLSelectMenuElement>(GetElementById("selectmenu"));

  selectmenu->SetSuggestedValue("nonexistent");
  EXPECT_EQ(blink::WebAutofillState::kNotFilled,
            selectmenu->GetAutofillState());
}

// Test that HTMLSelectMenuElement::setValue() clears the suggested option.
TEST_F(HTMLSelectMenuElementTest, SuggestedValueClearedWhenValueSet) {
  SetHtmlInnerHTML(R"HTML(
    <selectmenu id='selectmenu'>
      <option selected>First</option>
      <option>Second</option>
      <option>Third</option>
    </selectmenu>
  )HTML");
  HTMLSelectMenuElement* selectmenu =
      To<HTMLSelectMenuElement>(GetElementById("selectmenu"));

  selectmenu->SetSuggestedValue("Second");
  EXPECT_EQ(blink::WebAutofillState::kPreviewed,
            selectmenu->GetAutofillState());
  selectmenu->setValue("Third");
  EXPECT_EQ(blink::WebAutofillState::kNotFilled,
            selectmenu->GetAutofillState());
}

Color GetBorderColorForSuggestedOptionPopover(HTMLSelectMenuElement* element) {
  const ComputedStyle& popover_style =
      element->SuggestedOptionPopoverForTesting()->ComputedStyleRef();
  return popover_style.BorderTop().GetColor().GetColor();
}

// Test HTMLSelectMenuElement preview popover inherits border color from the
// button when the <selectmenu> button has a custom color.
TEST_F(HTMLSelectMenuElementTest, PreviewButtonHasCustomBorder) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      selectmenu::part(button) {
        border-color:#00ff00;
      }
    </style>
    <selectmenu id='selectmenu'>
      <option>First</option>
    </selectmenu>
  )HTML");

  HTMLSelectMenuElement* selectmenu =
      To<HTMLSelectMenuElement>(GetElementById("selectmenu"));
  selectmenu->SetSuggestedValue("First");

  EXPECT_EQ(Color::FromRGB(0, 0xff, 0),
            GetBorderColorForSuggestedOptionPopover(selectmenu));
}

// Test HTMLSelectMenuElement preview popover inherits border color from the
// button when the <selectmenu> button has an autofill-specific custom color.
TEST_F(HTMLSelectMenuElementTest, PreviewButtonHasCustomAutofillBorder) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      selectmenu::part(button):autofill {
        border-color:#00ff00;
      }
    </style>
    <selectmenu id='selectmenu'>
      <option>First</option>
    </selectmenu>
  )HTML");

  HTMLSelectMenuElement* selectmenu =
      To<HTMLSelectMenuElement>(GetElementById("selectmenu"));
  selectmenu->SetSuggestedValue("First");

  EXPECT_EQ(Color::FromRGB(0, 0xff, 0),
            GetBorderColorForSuggestedOptionPopover(selectmenu));
}

// Test HTMLSelectMenuElement preview popover uses default color and does not
// inherit color from selectmenu button when selectmenu button does not specify
// a custom border color.
TEST_F(HTMLSelectMenuElementTest, PreviewButtonHasNoCustomBorder) {
  SetHtmlInnerHTML(R"HTML(
    <selectmenu id='selectmenu'>
      <option>First</option>
    </selectmenu>
  )HTML");

  HTMLSelectMenuElement* selectmenu =
      To<HTMLSelectMenuElement>(GetElementById("selectmenu"));
  selectmenu->SetSuggestedValue("First");

  EXPECT_EQ(Color::FromRGBA(0, 0, 0, 0.15 * 255),
            GetBorderColorForSuggestedOptionPopover(selectmenu));
}

}  // namespace blink
