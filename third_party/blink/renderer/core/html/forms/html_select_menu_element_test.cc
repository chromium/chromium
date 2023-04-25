// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/html_select_menu_element.h"

#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
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

}  // anonymous namespace

class HTMLSelectMenuElementTest : public PageTestBase {
 public:
  void SetUp() override {
    PageTestBase::SetUp();
    GetDocument().SetMimeType("text/html");
    GetFrame().GetSettings()->SetScriptEnabled(true);
  }

  void ExecuteJs(const std::string& js) {
    ClassicScript::CreateUnspecifiedScript(WebString::FromUTF8(js))
        ->RunScript(GetFrame().DomWindow());
  }
};

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

}  // namespace blink
