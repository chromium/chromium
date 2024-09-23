// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/html_select_element.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/forms/form_controller.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/select_type.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class HTMLSelectElementTest : public PageTestBase {
 protected:
  void SetUp() override;
  void TearDown() override;

  SelectType& GetSelectType(const HTMLSelectElement& select) {
    return *select.select_type_;
  }

  HTMLOptionElement* FirstSelectableOption(const HTMLSelectElement& select) {
    return GetSelectType(select).FirstSelectableOption();
  }
  HTMLOptionElement* LastSelectableOption(const HTMLSelectElement& select) {
    return GetSelectType(select).LastSelectableOption();
  }
  HTMLOptionElement* NextSelectableOption(const HTMLSelectElement& select,
                                          HTMLOptionElement* option) {
    return GetSelectType(select).NextSelectableOption(option);
  }
  HTMLOptionElement* PreviousSelectableOption(const HTMLSelectElement& select,
                                              HTMLOptionElement* option) {
    return GetSelectType(select).PreviousSelectableOption(option);
  }

  bool FirstSelectIsConnectedAfterSelectMultiple(const Vector<int>& indices) {
    auto* select = To<HTMLSelectElement>(GetDocument().body()->firstChild());
    select->Focus();
    select->SelectMultipleOptionsByPopup(indices);
    return select->isConnected();
  }

  String MenuListLabel() const {
    auto* select = To<HTMLSelectElement>(GetDocument().body()->firstChild());
    return select->InnerElement().textContent();
  }

 private:
  bool original_delegates_flag_;
};

void HTMLSelectElementTest::SetUp() {
  PageTestBase::SetUp();
  GetDocument().SetMimeType(AtomicString("text/html"));
  original_delegates_flag_ =
      LayoutTheme::GetTheme().DelegatesMenuListRendering();
}

void HTMLSelectElementTest::TearDown() {
  LayoutTheme::GetTheme().SetDelegatesMenuListRenderingForTesting(
      original_delegates_flag_);
  PageTestBase::TearDown();
}

// Tests that HtmlSelectElement::SetAutofillValue() doesn't change the
// `user_has_edited_the_field_` attribute of the field.
TEST_F(HTMLSelectElementTest, SetAutofillValuePreservesEditedState) {
  SetHtmlInnerHTML(
      "<!DOCTYPE HTML><select id='sel'>"
      "<option value='111' selected>111</option>"
      "<option value='222'>222</option></select>");
  HTMLSelectElement* select = To<HTMLSelectElement>(GetElementById("sel"));

  select->ClearUserHasEditedTheField();
  select->SetAutofillValue("222", WebAutofillState::kAutofilled);
  EXPECT_EQ(select->UserHasEditedTheField(), false);

  select->SetUserHasEditedTheField();
  select->SetAutofillValue("111", WebAutofillState::kAutofilled);
  EXPECT_EQ(select->UserHasEditedTheField(), true);
}

TEST_F(HTMLSelectElementTest, SaveRestoreSelectSingleFormControlState) {
  SetHtmlInnerHTML(
      "<!DOCTYPE HTML><select id='sel'>"
      "<option value='111' id='0'>111</option>"
      "<option value='222'>222</option>"
      "<option value='111' selected id='2'>!666</option>"
      "<option value='999'>999</option></select>");
  Element* element = GetElementById("sel");
  auto* opt0 = To<HTMLOptionElement>(GetElementById("0"));
  auto* opt2 = To<HTMLOptionElement>(GetElementById("2"));

  // Save the select element state, and then restore again.
  // Test passes if the restored state is not changed.
  EXPECT_EQ(2, To<HTMLSelectElement>(element)->selectedIndex());
  EXPECT_FALSE(opt0->Selected());
  EXPECT_TRUE(opt2->Selected());
  HTMLFormControlElementWithState* select = To<HTMLSelectElement>(element);
  FormControlState select_state = select->SaveFormControlState();
  EXPECT_EQ(2U, select_state.ValueSize());

  // Clear the selected state, to be restored by restoreFormControlState.
  To<HTMLSelectElement>(select)->setSelectedIndex(-1);
  ASSERT_FALSE(opt2->Selected());

  // Restore
  select->RestoreFormControlState(select_state);
  EXPECT_EQ(2, To<HTMLSelectElement>(element)->selectedIndex());
  EXPECT_FALSE(opt0->Selected());
  EXPECT_TRUE(opt2->Selected());
  EXPECT_EQ("!666",
            To<HTMLSelectElement>(element)->InnerElement().textContent());
}

TEST_F(HTMLSelectElementTest, SaveRestoreSelectMultipleFormControlState) {
  SetHtmlInnerHTML(
      "<!DOCTYPE HTML><select id='sel' multiple>"
      "<option value='111' id='0'>111</option>"
      "<option value='222'>222</option>"
      "<option value='111' selected id='2'>!666</option>"
      "<option value='999' selected id='3'>999</option></select>");

  auto* opt0 = To<HTMLOptionElement>(GetElementById("0"));
  auto* opt2 = To<HTMLOptionElement>(GetElementById("2"));
  auto* opt3 = To<HTMLOptionElement>(GetElementById("3"));

  // Save the select element state, and then restore again.
  // Test passes if the selected options are not changed.
  EXPECT_FALSE(opt0->Selected());
  EXPECT_TRUE(opt2->Selected());
  EXPECT_TRUE(opt3->Selected());
  HTMLFormControlElementWithState* select =
      To<HTMLSelectElement>(GetElementById("sel"));
  FormControlState select_state = select->SaveFormControlState();
  EXPECT_EQ(4U, select_state.ValueSize());

  // Clear the selected state, to be restored by restoreFormControlState.
  opt2->SetSelected(false);
  opt3->SetSelected(false);
  ASSERT_FALSE(opt2->Selected());
  ASSERT_FALSE(opt3->Selected());

  // Restore
  select->RestoreFormControlState(select_state);
  EXPECT_FALSE(opt0->Selected());
  EXPECT_TRUE(opt2->Selected());
  EXPECT_TRUE(opt3->Selected());
}

TEST_F(HTMLSelectElementTest, RestoreUnmatchedFormControlState) {
  // We had a bug that SelectedOption() and last_on_change_option_ were
  // mismatched in OptionToBeShown(). It happened when
  // RestoreFormControlState() couldn't find matched OPTIONs.
  // crbug.com/627833.

  SetHtmlInnerHTML(R"HTML(
    <select id='sel'>
    <option selected>Default</option>
    <option id='2'>222</option>
    </select>
  )HTML");
  Element* element = GetElementById("sel");
  auto* opt2 = To<HTMLOptionElement>(GetElementById("2"));

  To<HTMLSelectElement>(element)->setSelectedIndex(1);
  // Save the current state.
  HTMLFormControlElementWithState* select = To<HTMLSelectElement>(element);
  FormControlState select_state = select->SaveFormControlState();
  EXPECT_EQ(2U, select_state.ValueSize());

  // Reset the status.
  select->Reset();
  ASSERT_FALSE(opt2->Selected());
  element->RemoveChild(opt2);

  // Restore
  select->RestoreFormControlState(select_state);
  EXPECT_EQ(-1, To<HTMLSelectElement>(element)->selectedIndex());
  EXPECT_EQ(nullptr, To<HTMLSelectElement>(element)->OptionToBeShown());
}

TEST_F(HTMLSelectElementTest, VisibleBoundsInLocalRoot) {
  SetHtmlInnerHTML(
      "<select style='position:fixed; top:12.3px; height:24px; "
      "-webkit-appearance:none;'><option>o1</select>");
  auto* select = To<HTMLSelectElement>(GetDocument().body()->firstChild());
  ASSERT_NE(select, nullptr);
  gfx::Rect bounds = select->VisibleBoundsInLocalRoot();
  EXPECT_EQ(24, bounds.height());
}

TEST_F(HTMLSelectElementTest, PopupIsVisible) {
  SetHtmlInnerHTML("<select><option>o1</option></select>");
  auto* select = To<HTMLSelectElement>(GetDocument().body()->firstChild());
  ASSERT_NE(select, nullptr);
  EXPECT_FALSE(select->PopupIsVisible());
  select->ShowPopup();
  EXPECT_TRUE(select->PopupIsVisible());
  GetDocument().Shutdown();
  EXPECT_FALSE(select->PopupIsVisible());
}

TEST_F(HTMLSelectElementTest, FirstSelectableOption) {
  {
    SetHtmlInnerHTML("<select></select>");
    auto* select = To<HTMLSelectElement>(GetDocument().body()->firstChild());
    EXPECT_EQ(nullptr, FirstSelectableOption(*select));
  }
  {
    SetHtmlInnerHTML(
        "<select><option id=o1></option><option id=o2></option></select>");
    auto* select = To<HTMLSelectElement>(GetDocument().body()->firstChild());
    EXPECT_EQ("o1", FirstSelectableOption(*select)->FastGetAttribute(
                        html_names::kIdAttr));
  }
  {
    SetHtmlInnerHTML(
        "<select><option id=o1 disabled></option><option "
        "id=o2></option></select>");
    auto* select = To<HTMLSelectElement>(GetDocument().body()->firstChild());
    EXPECT_EQ("o2", FirstSelectableOption(*select)->FastGetAttribute(
                        html_names::kIdAttr));
  }
  {
    SetHtmlInnerHTML(
        "<select><option id=o1 style='display:none'></option><option "
        "id=o2></option></select>");
    auto* select = To<HTMLSelectElement>(GetDocument().body()->firstChild());
    EXPECT_EQ("o2", FirstSelectableOption(*select)->FastGetAttribute(
                        html_names::kIdAttr));
  }
  {
    SetHtmlInnerHTML(
        "<select><optgroup><option id=o1></option><option "
        "id=o2></option></optgroup></select>");
    auto* select = To<HTMLSelectElement>(GetDocument().body()->firstChild());
    EXPECT_EQ("o1", FirstSelectableOption(*select)->FastGetAttribute(
                        html_names::kIdAttr));
  }
}

TEST_F(HTMLSelectElementTest, LastSelectableOption) {
  {
    SetHtmlInnerHTML("<select></select>");
    auto* select = To<HTMLSelectElement>(GetDocument().body()->firstChild());
    EXPECT_EQ(nullptr, LastSelectableOption(*select));
  }
  {
    SetHtmlInnerHTML(
        "<select><option id=o1></option><option id=o2></option></select>");
    auto* select = To<HTMLSelectElement>(GetDocument().body()->firstChild());
    EXPECT_EQ("o2", LastSelectableOption(*select)->FastGetAttribute(
                        html_names::kIdAttr));
  }
  {
    SetHtmlInnerHTML(
        "<select><option id=o1></option><option id=o2 "
        "disabled></option></select>");
    auto* select = To<HTMLSelectElement>(GetDocument().body()->firstChild());
    EXPECT_EQ("o1", LastSelectableOption(*select)->FastGetAttribute(
                        html_names::kIdAttr));
  }
  {
    SetHtmlInnerHTML(
        "<select><option id=o1></option><option id=o2 "
        "style='display:none'></option></select>");
    auto* select = To<HTMLSelectElement>(GetDocument().body()->firstChild());
    EXPECT_EQ("o1", LastSelectableOption(*select)->FastGetAttribute(
                        html_names::kIdAttr));
  }
  {
    SetHtmlInnerHTML(
        "<select><optgroup><option id=o1></option><option "
        "id=o2></option></optgroup></select>");
    auto* select = To<HTMLSelectElement>(GetDocument().body()->firstChild());
    EXPECT_EQ("o2", LastSelectableOption(*select)->FastGetAttribute(
                        html_names::kIdAttr));
  }
}

TEST_F(HTMLSelectElementTest, NextSelectableOption) {
  {
    SetHtmlInnerHTML("<select></select>");
    auto* select = To<HTMLSelectElement>(GetDocument().body()->firstChild());
    EXPECT_EQ(nullptr, NextSelectableOption(*select, nullptr));
  }
  {
    SetHtmlInnerHTML(
        "<select><option id=o1></option><option id=o2></option></select>");
    auto* select = To<HTMLSelectElement>(GetDocument().body()->firstChild());
    EXPECT_EQ("o1", NextSelectableOption(*select, nullptr)
                        ->FastGetAttribute(html_names::kIdAttr));
  }
  {
    SetHtmlInnerHTML(
        "<select><option id=o1 disabled></option><option "
        "id=o2></option></select>");
    auto* select = To<HTMLSelectElement>(GetDocument().body()->firstChild());
    EXPECT_EQ("o2", NextSelectableOption(*select, nullptr)
                        ->FastGetAttribute(html_names::kIdAttr));
  }
  {
    SetHtmlInnerHTML(
        "<select><option id=o1 style='display:none'></option><option "
        "id=o2></option></select>");
    auto* select = To<HTMLSelectElement>(GetDocument().body()->firstChild());
    EXPECT_EQ("o2", NextSelectableOption(*select, nullptr)
                        ->FastGetAttribute(html_names::kIdAttr));
  }
  {
    SetHtmlInnerHTML(
        "<select><optgroup><option id=o1></option><option "
        "id=o2></option></optgroup></select>");
    auto* select = To<HTMLSelectElement>(GetDocument().body()->firstChild());
    EXPECT_EQ("o1", NextSelectableOption(*select, nullptr)
                        ->FastGetAttribute(html_names::kIdAttr));
  }
  {
    SetHtmlInnerHTML(
        "<select><option id=o1></option><option id=o2></option></select>");
    auto* select = To<HTMLSelectElement>(GetDocument().body()->firstChild());
    auto* option = To<HTMLOptionElement>(GetElementById("o1"));
    EXPECT_EQ("o2", NextSelectableOption(*select, option)
                        ->FastGetAttribute(html_names::kIdAttr));

    EXPECT_EQ(nullptr,
              NextSelectableOption(
                  *select, To<HTMLOptionElement>(GetElementById("o2"))));
  }
  {
    SetHtmlInnerHTML(
        "<select><option id=o1></option><optgroup><option "
        "id=o2></option></optgroup></select>");
    auto* select = To<HTMLSelectElement>(GetDocument().body()->firstChild());
    auto* option = To<HTMLOptionElement>(GetElementById("o1"));
    EXPECT_EQ("o2", NextSelectableOption(*select, option)
                        ->FastGetAttribute(html_names::kIdAttr));
  }
}

TEST_F(HTMLSelectElementTest, PreviousSelectableOption) {
  {
    SetHtmlInnerHTML("<select></select>");
    auto* select = To<HTMLSelectElement>(GetDocument().body()->firstChild());
    EXPECT_EQ(nullptr, PreviousSelectableOption(*select, nullptr));
  }
  {
    SetHtmlInnerHTML(
        "<select><option id=o1></option><option id=o2></option></select>");
    auto* select = To<HTMLSelectElement>(GetDocument().body()->firstChild());
    EXPECT_EQ("o2", PreviousSelectableOption(*select, nullptr)
                        ->FastGetAttribute(html_names::kIdAttr));
  }
  {
    SetHtmlInnerHTML(
        "<select><option id=o1></option><option id=o2 "
        "disabled></option></select>");
    auto* select = To<HTMLSelectElement>(GetDocument().body()->firstChild());
    EXPECT_EQ("o1", PreviousSelectableOption(*select, nullptr)
                        ->FastGetAttribute(html_names::kIdAttr));
  }
  {
    SetHtmlInnerHTML(
        "<select><option id=o1></option><option id=o2 "
        "style='display:none'></option></select>");
    auto* select = To<HTMLSelectElement>(GetDocument().body()->firstChild());
    EXPECT_EQ("o1", PreviousSelectableOption(*select, nullptr)
                        ->FastGetAttribute(html_names::kIdAttr));
  }
  {
    SetHtmlInnerHTML(
        "<select><optgroup><option id=o1></option><option "
        "id=o2></option></optgroup></select>");
    auto* select = To<HTMLSelectElement>(GetDocument().body()->firstChild());
    EXPECT_EQ("o2", PreviousSelectableOption(*select, nullptr)
                        ->FastGetAttribute(html_names::kIdAttr));
  }
  {
    SetHtmlInnerHTML(
        "<select><option id=o1></option><option id=o2></option></select>");
    auto* select = To<HTMLSelectElement>(GetDocument().body()->firstChild());
    auto* option = To<HTMLOptionElement>(GetElementById("o2"));
    EXPECT_EQ("o1", PreviousSelectableOption(*select, option)
                        ->FastGetAttribute(html_names::kIdAttr));

    EXPECT_EQ(nullptr,
              PreviousSelectableOption(
                  *select, To<HTMLOptionElement>(GetElementById("o1"))));
  }
  {
    SetHtmlInnerHTML(
        "<select><option id=o1></option><optgroup><option "
        "id=o2></option></optgroup></select>");
    auto* select = To<HTMLSelectElement>(GetDocument().body()->firstChild());
    auto* option = To<HTMLOptionElement>(GetElementById("o2"));
    EXPECT_EQ("o1", PreviousSelectableOption(*select, option)
                        ->FastGetAttribute(html_names::kIdAttr));
  }
}

TEST_F(HTMLSelectElementTest, ActiveSelectionEndAfterOptionRemoval) {
  SetHtmlInnerHTML(
      "<select size=4>"
      "<optgroup><option selected>o1</option></optgroup></select>");
  auto* select = To<HTMLSelectElement>(GetDocument().body()->firstChild());
  // ActiveSelectionEnd*() work only in the listbox mode, which Android
  // doesn't have.
  if (select->UsesMenuList())
    return;
  auto* option = To<HTMLOptionElement>(select->firstChild()->firstChild());
  EXPECT_EQ(1, select->ActiveSelectionEndListIndex());
  select->firstChild()->removeChild(option);
  EXPECT_EQ(-1, select->ActiveSelectionEndListIndex());
  select->AppendChild(option);
  EXPECT_EQ(1, select->ActiveSelectionEndListIndex());
}

TEST_F(HTMLSelectElementTest, DefaultToolTip) {
  SetHtmlInnerHTML(
      "<select size=4><option value="
      ">Placeholder</option><optgroup><option>o2</option></optgroup></select>");
  auto* select = To<HTMLSelectElement>(GetDocument().body()->firstChild());
  auto* option = To<Element>(select->firstChild());
  auto* optgroup = To<Element>(option->nextSibling());

  EXPECT_EQ(String(), select->DefaultToolTip())
      << "defaultToolTip for SELECT without FORM and without required "
         "attribute should return null string.";
  EXPECT_EQ(select->DefaultToolTip(), option->DefaultToolTip());
  EXPECT_EQ(select->DefaultToolTip(), optgroup->DefaultToolTip());

  select->SetBooleanAttribute(html_names::kRequiredAttr, true);
  EXPECT_EQ("<<ValidationValueMissingForSelect>>", select->DefaultToolTip())
      << "defaultToolTip for SELECT without FORM and with required attribute "
         "should return a valueMissing message.";
  EXPECT_EQ(select->DefaultToolTip(), option->DefaultToolTip());
  EXPECT_EQ(select->DefaultToolTip(), optgroup->DefaultToolTip());

  auto* form = MakeGarbageCollected<HTMLFormElement>(GetDocument());
  GetDocument().body()->AppendChild(form);
  form->AppendChild(select);
  EXPECT_EQ("<<ValidationValueMissingForSelect>>", select->DefaultToolTip())
      << "defaultToolTip for SELECT with FORM and required attribute should "
         "return a valueMissing message.";
  EXPECT_EQ(select->DefaultToolTip(), option->DefaultToolTip());
  EXPECT_EQ(select->DefaultToolTip(), optgroup->DefaultToolTip());

  form->SetBooleanAttribute(html_names::kNovalidateAttr, true);
  EXPECT_EQ(String(), select->DefaultToolTip())
      << "defaultToolTip for SELECT with FORM[novalidate] and required "
         "attribute should return null string.";
  EXPECT_EQ(select->DefaultToolTip(), option->DefaultToolTip());
  EXPECT_EQ(select->DefaultToolTip(), optgroup->DefaultToolTip());

  option->remove();
  optgroup->remove();
  EXPECT_EQ(String(), option->DefaultToolTip());
  EXPECT_EQ(String(), optgroup->DefaultToolTip());
}

TEST_F(HTMLSelectElementTest, SetRecalcListItemsByOptgroupRemoval) {
  SetHtmlInnerHTML(
      "<select><optgroup><option>sub1</option><option>sub2</option></"
      "optgroup></select>");
  auto* select = To<HTMLSelectElement>(GetDocument().body()->firstChild());
  select->setInnerHTML("");
  // PASS if setInnerHTML didn't have a check failure.
}

TEST_F(HTMLSelectElementTest, ScrollToOptionAfterLayoutCrash) {
  // crbug.com/737447
  // This test passes if no crash.
  SetHtmlInnerHTML(R"HTML(
    <style>*:checked { position:fixed; }</style>
    <select multiple><<option>o1</option><option
    selected>o2</option></select>
  )HTML");
}

TEST_F(HTMLSelectElementTest, CrashOnAttachingMenuList) {
  // crbug.com/1044834
  // This test passes if no crash.
  SetHtmlInnerHTML("<select><option selected style='direction:rtl'>o1");
  GetDocument().UpdateStyleAndLayoutTree();
  auto* select = To<HTMLSelectElement>(GetDocument().body()->firstChild());
  ASSERT_TRUE(select->GetLayoutObject());

  // Detach LayoutMenuList.
  select->setAttribute(html_names::kStyleAttr, AtomicString("display:none;"));
  GetDocument().UpdateStyleAndLayoutTree();
  ASSERT_FALSE(select->GetLayoutObject());

  // Attach LayoutMenuList again.  It triggered null-dereference in
  // LayoutMenuList::AdjustInnerStyle().
  select->removeAttribute(html_names::kStyleAttr);
  GetDocument().UpdateStyleAndLayoutTree();
  ASSERT_TRUE(select->GetLayoutObject());
}

TEST_F(HTMLSelectElementTest, CrashOnAttachingMenuList2) {
  // crbug.com/1065125
  // This test passes if no crash.
  SetHtmlInnerHTML("<select><optgroup><option>o1</select>");
  auto* select = To<HTMLSelectElement>(GetDocument().body()->firstChild());
  select->setTextContent("foo");

  // Detach LayoutObject.
  select->setAttribute(html_names::kStyleAttr, AtomicString("display:none;"));
  GetDocument().UpdateStyleAndLayoutTree();

  // Attach LayoutObject.  It triggered a DCHECK failure in
  // MenuListSelectType::OptionToBeShown()
  select->removeAttribute(html_names::kStyleAttr);
  GetDocument().UpdateStyleAndLayoutTree();
}

TEST_F(HTMLSelectElementTest, SlotAssignmentRecalcDuringOptionRemoval) {
  // crbug.com/1056094
  // This test passes if no CHECK failure about IsSlotAssignmentRecalcForbidden.
  SetHtmlInnerHTML("<div dir=auto><select><option>option0");
  auto* select = GetDocument().body()->firstChild()->firstChild();
  auto* option = select->firstChild();
  select->appendChild(option);
  option->remove();
}

// crbug.com/1060039
TEST_F(HTMLSelectElementTest, SelectMultipleOptionsByPopup) {
  GetDocument().GetSettings()->SetScriptEnabled(true);
  LayoutTheme::GetTheme().SetDelegatesMenuListRenderingForTesting(true);

  // Select the same set of options.
  {
    SetHtmlInnerHTML(
        "<select multiple onchange='this.remove();'>"
        "<option>o0</option><option>o1</option></select>");
    EXPECT_TRUE(FirstSelectIsConnectedAfterSelectMultiple(Vector<int>{}))
        << "Onchange handler should not be executed.";
  }
  {
    SetHtmlInnerHTML(
        "<select multiple onchange='this.remove();'>"
        "<option>o0</option><option selected>o1</option></select>");
    EXPECT_TRUE(FirstSelectIsConnectedAfterSelectMultiple(Vector<int>{1}))
        << "Onchange handler should not be executed.";
  }

  // 0 old selected options -> 1+ selected options
  {
    SetHtmlInnerHTML(
        "<select multiple onchange='this.remove();'>"
        "<option>o0</option><option>o1</option></select>");
    EXPECT_FALSE(FirstSelectIsConnectedAfterSelectMultiple(Vector<int>{0}))
        << "Onchange handler should be executed.";
  }

  // 1+ old selected options -> more selected options
  {
    SetHtmlInnerHTML(
        "<select multiple onchange='this.remove();'>"
        "<option>o0</option><option selected>o1</option></select>");
    EXPECT_FALSE(FirstSelectIsConnectedAfterSelectMultiple(Vector<int>{0, 1}))
        << "Onchange handler should be executed.";
  }

  // 1+ old selected options -> 0 selected options
  {
    SetHtmlInnerHTML(
        "<select multiple onchange='this.remove();'>"
        "<option>o0</option><option selected>o1</option></select>");
    EXPECT_FALSE(FirstSelectIsConnectedAfterSelectMultiple(Vector<int>{}))
        << "Onchange handler should be executed.";
  }

  // Multiple old selected options -> less selected options
  {
    SetHtmlInnerHTML(
        "<select multiple onchange='this.remove();'>"
        "<option selected>o0</option><option selected>o1</option></select>");
    EXPECT_FALSE(FirstSelectIsConnectedAfterSelectMultiple(Vector<int>{1}))
        << "Onchange handler should be executed.";
  }

  // Check if the label is correctly updated.
  {
    SetHtmlInnerHTML(
        "<select multiple>"
        "<option selected>o0</option><option selected>o1</option></select>");
    EXPECT_EQ("2 selected", MenuListLabel());
    EXPECT_TRUE(FirstSelectIsConnectedAfterSelectMultiple(Vector<int>{1}));
    EXPECT_EQ("o1", MenuListLabel());
  }
}

TEST_F(HTMLSelectElementTest, IntrinsicInlineSizeOverflow) {
  // crbug.com/1068338
  // This test passes if UBSAN doesn't complain.
  SetHtmlInnerHTML(
      "<select style='word-spacing:1073741824em;'>"
      "<option>abc def</option></select>");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
}

TEST_F(HTMLSelectElementTest, AddingNotOwnedOption) {
  // crbug.com/1077556
  auto& doc = GetDocument();
  SetHtmlInnerHTML("<select>");
  auto* select = To<HTMLSelectElement>(doc.body()->firstChild());
  // Append <div><optgroup></optgroup></div> to the SELECT.
  // We can't do it with the HTML parser.
  auto* optgroup = doc.CreateRawElement(html_names::kOptgroupTag);
  select->appendChild(doc.CreateRawElement(html_names::kDivTag))
      ->appendChild(optgroup);
  optgroup->appendChild(doc.CreateRawElement(html_names::kOptionTag));
  // This test passes if the above appendChild() doesn't cause a DCHECK failure.
}

TEST_F(HTMLSelectElementTest, ChangeRenderingCrash) {
  SetHtmlInnerHTML(R"HTML(
    <select id="sel">
      <option id="opt"></option>
    </select>
  )HTML");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  // Make the option element the style recalc root.
  GetElementById("opt")->SetInlineStyleProperty(CSSPropertyID::kColor, "green");
  // Changing the size attribute changes the rendering. This should not trigger
  // a DCHECK failure updating the style recalc root.
  GetElementById("sel")->setAttribute(html_names::kSizeAttr, AtomicString("2"));
}

TEST_F(HTMLSelectElementTest, ChangeRenderingCrash2) {
  SetHtmlInnerHTML(R"HTML(
    <select id="sel">
      <optgroup id="grp">
        <option id="opt"></option>
      </optgroup>
    </select>
  )HTML");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  // Make the select UA slot the style recalc root.
  GetElementById("opt")->SetInlineStyleProperty(CSSPropertyID::kColor, "green");
  GetElementById("grp")->SetInlineStyleProperty(CSSPropertyID::kColor, "green");
  // Changing the multiple attribute changes the rendering. This should not
  // trigger a DCHECK failure updating the style recalc root.
  GetElementById("sel")->setAttribute(html_names::kMultipleAttr,
                                      AtomicString("true"));
}

TEST_F(HTMLSelectElementTest, ChangeRenderingCrash3) {
  SetHtmlInnerHTML(R"HTML(
    <div id="host">
      <select id="select">
        <option></option>
      </select>
    </div>
    <div id="green">Green</div>
  )HTML");

  auto* host = GetDocument().getElementById(AtomicString("host"));
  auto* select = GetDocument().getElementById(AtomicString("select"));
  auto* green = GetDocument().getElementById(AtomicString("green"));

  // Make sure the select is outside the flat tree.
  host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  // Changing the select rendering should not clear the style recalc root set by
  // the color change on #green.
  green->SetInlineStyleProperty(CSSPropertyID::kColor, "green");
  select->setAttribute(html_names::kMultipleAttr, AtomicString("true"));

  EXPECT_TRUE(GetDocument().GetStyleEngine().NeedsStyleRecalc());
  EXPECT_TRUE(green->NeedsStyleRecalc());
}

TEST_F(HTMLSelectElementTest, ChangeRenderingSelectRoot) {
  // This test exercises the path in StyleEngine::ChangeRenderingForHTMLSelect()
  // where the select does not have a GetStyleRecalcParent().
  SetHtmlInnerHTML(R"HTML(
    <select id="sel">
      <option></option>
    </select>
  )HTML");

  auto* select = GetElementById("sel");

  // Make the select the root element.
  select->remove();
  GetDocument().documentElement()->remove();
  GetDocument().appendChild(select);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  // Changing the multiple attribute changes the rendering.
  select->setAttribute(html_names::kMultipleAttr, AtomicString("true"));
  EXPECT_TRUE(GetDocument().GetStyleEngine().NeedsStyleRecalc());
  EXPECT_TRUE(select->NeedsStyleRecalc());
}

}  // namespace blink
