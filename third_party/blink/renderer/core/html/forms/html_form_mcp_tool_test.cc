// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class HTMLFormMcpToolTest : public PageTestBase {
 public:
  HTMLFormElement* GetFormElement(const char* id) {
    return DynamicTo<HTMLFormElement>(
        GetDocument().getElementById(AtomicString(id)));
  }

  // Private functions exposed via class friendship:

  static bool IsValidWebMCPForm(HTMLFormElement& form_element) {
    return form_element.IsValidWebMCPForm();
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

}  // namespace blink
