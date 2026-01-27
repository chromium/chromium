// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class HTMLFormMcpToolTest : public PageTestBase {
 public:
  HTMLInputElement* GetInputElement(const char* id) {
    return DynamicTo<HTMLInputElement>(
        GetDocument().getElementById(AtomicString(id)));
  }

  HTMLFormElement* GetFormElement(const char* id) {
    return DynamicTo<HTMLFormElement>(
        GetDocument().getElementById(AtomicString(id)));
  }

  // Private functions exposed via class friendship:

  static bool IsValidWebMCPForm(HTMLFormElement& form_element) {
    return form_element.IsValidWebMCPForm();
  }

  static bool FillFormControls(HTMLFormElement& form_element,
                               const String& input_arguments) {
    CHECK(IsValidWebMCPForm(form_element));
    CHECK(form_element.active_webmcp_tool_);
    HTMLFormControlElement* submit_button;
    return form_element.active_webmcp_tool_->FillFormControls(input_arguments,
                                                              &submit_button);
  }

 private:
  ScopedWebMCPForTest scoped_feature{true};
};

// Note that both toolname *and* tooldescription must be present
// for a <form> element to become a valid declarative WebMCP tool.

TEST_F(HTMLFormMcpToolTest, NoTool) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form>
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  EXPECT_FALSE(IsValidWebMCPForm(*form_element));
}

TEST_F(HTMLFormMcpToolTest, NoTool_NameOnly) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool">
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  EXPECT_FALSE(IsValidWebMCPForm(*form_element));
}

TEST_F(HTMLFormMcpToolTest, NoTool_DescriptionOnly) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form tooldescription="perform task">
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  EXPECT_FALSE(IsValidWebMCPForm(*form_element));
}

TEST_F(HTMLFormMcpToolTest, ToolPresent_Basic) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="perform task">
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  EXPECT_TRUE(IsValidWebMCPForm(*form_element));
}

TEST_F(HTMLFormMcpToolTest, ToolRemovedWithAttribute_Name) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="perform task">
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  EXPECT_TRUE(IsValidWebMCPForm(*form_element));

  form_element->removeAttribute(html_names::kToolnameAttr);
  EXPECT_FALSE(IsValidWebMCPForm(*form_element));
}

TEST_F(HTMLFormMcpToolTest, ToolRemovedWithAttribute_Description) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="perform task">
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  EXPECT_TRUE(IsValidWebMCPForm(*form_element));

  form_element->removeAttribute(html_names::kTooldescriptionAttr);
  EXPECT_FALSE(IsValidWebMCPForm(*form_element));
}

TEST_F(HTMLFormMcpToolTest, ToolAppearsWhenAttributeSet_NameFirst) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form>
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  EXPECT_FALSE(IsValidWebMCPForm(*form_element));

  form_element->setAttribute(html_names::kToolnameAttr, AtomicString("mytool"));
  EXPECT_FALSE(IsValidWebMCPForm(*form_element));  // Still need a description.
  form_element->setAttribute(html_names::kTooldescriptionAttr,
                             AtomicString("description"));
  EXPECT_TRUE(IsValidWebMCPForm(*form_element));
}

TEST_F(HTMLFormMcpToolTest, ToolAppearsWhenAttributeSet_DescriptionFirst) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form>
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  EXPECT_FALSE(IsValidWebMCPForm(*form_element));

  form_element->setAttribute(html_names::kTooldescriptionAttr,
                             AtomicString("description"));
  EXPECT_FALSE(IsValidWebMCPForm(*form_element));  // Still need a name.
  form_element->setAttribute(html_names::kToolnameAttr, AtomicString("mytool"));
  EXPECT_TRUE(IsValidWebMCPForm(*form_element));
}

TEST_F(HTMLFormMcpToolTest, Tool_AppendFormElement) {
  UpdateAllLifecyclePhasesForTest();

  HTMLFormElement* form_element =
      MakeGarbageCollected<HTMLFormElement>(GetDocument());
  form_element->setAttribute(html_names::kToolnameAttr, AtomicString("mytool"));
  form_element->setAttribute(html_names::kTooldescriptionAttr,
                             AtomicString("description"));
  EXPECT_FALSE(IsValidWebMCPForm(*form_element));  // Not connected.

  GetDocument().body()->AppendChild(form_element);
  EXPECT_TRUE(IsValidWebMCPForm(*form_element));
}

TEST_F(HTMLFormMcpToolTest, FillFormControls_Basic) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="perform task">
      <input id=text1 name=text1 type=text>
      <input id=text2 name=text2 type=text>
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));

  String json_string =
      R"JSON(
        {
          "text1": "foo",
          "text2": "bar"
        }
      )JSON";

  EXPECT_TRUE(FillFormControls(*form_element, json_string));

  HTMLInputElement* text1 = GetInputElement("text1");
  HTMLInputElement* text2 = GetInputElement("text2");
  ASSERT_TRUE(text1);
  ASSERT_TRUE(text2);

  EXPECT_EQ("foo", text1->Value());
  EXPECT_EQ("bar", text2->Value());
}

TEST_F(HTMLFormMcpToolTest, FillFormControls_Partial) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="perform task">
      <input id=text1 name=text1 type=text value="initial1">
      <input id=text2 name=text2 type=text value="initial2">
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));

  String json_string =
      R"JSON(
        {
          "text2": "bar"
        }
      )JSON";

  EXPECT_TRUE(FillFormControls(*form_element, json_string));

  HTMLInputElement* text1 = GetInputElement("text1");
  HTMLInputElement* text2 = GetInputElement("text2");
  ASSERT_TRUE(text1);
  ASSERT_TRUE(text2);

  EXPECT_EQ("initial1", text1->Value());
  EXPECT_EQ("bar", text2->Value());
}

TEST_F(HTMLFormMcpToolTest, FillFormControls_InvalidJsonFailure) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="perform task"> </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));

  EXPECT_FALSE(FillFormControls(*form_element, R"JSON({"x":"y",})JSON"));
  EXPECT_FALSE(FillFormControls(*form_element, R"JSON(["unknown"])JSON"));
}

TEST_F(HTMLFormMcpToolTest, FillFormControls_UnknownParamFailure) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="perform task">
      <input id=text1 name=text1 type=text>
      <input id=text2 name=text2 type=text>
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));

  String json_string =
      R"JSON(
        {
          "unknown": "UNKNOWN"
        }
      )JSON";

  EXPECT_FALSE(FillFormControls(*form_element, json_string));
}

TEST_F(HTMLFormMcpToolTest, FillFormControls_Transactional) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="perform task">
      <input id=text1 name=text1 type=text value="initial1">
      <input id=text2 name=text2 type=text value="initial2">
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));

  String json_string =
      R"JSON(
        {
          "text1": "foo",
          "unknown": "bar",
          "text2": "bar"
        }
      )JSON";

  EXPECT_FALSE(FillFormControls(*form_element, json_string));

  HTMLInputElement* text1 = GetInputElement("text1");
  HTMLInputElement* text2 = GetInputElement("text2");
  ASSERT_TRUE(text1);
  ASSERT_TRUE(text2);

  // A failure means no form control values were changed.
  EXPECT_EQ("initial1", text1->Value());
  EXPECT_EQ("initial2", text2->Value());
}

}  // namespace blink
