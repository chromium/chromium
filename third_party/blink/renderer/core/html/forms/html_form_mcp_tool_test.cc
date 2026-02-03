// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/json/json_parser.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
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

  HTMLTextAreaElement* GetTextAreaElement(const char* id) {
    return DynamicTo<HTMLTextAreaElement>(
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
    bool require_submit_button = false;
    HTMLFormControlElement* submit_button;
    std::optional<WebDocument::ScriptToolError> error =
        form_element.active_webmcp_tool_->FillFormControls(
            input_arguments, require_submit_button, &submit_button);
    return !error.has_value();
  }

  static String ComputeInputSchema(HTMLFormElement& form_element) {
    CHECK(IsValidWebMCPForm(form_element));
    CHECK(form_element.active_webmcp_tool_);
    return form_element.active_webmcp_tool_->ComputeInputSchema();
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

TEST_F(HTMLFormMcpToolTest, FillFormControls_NumberInput) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="perform task">
      <input id=num1 name=num1 type=number>
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));

  String json_string =
      R"JSON(
        {
          "num1": 42
        }
      )JSON";

  EXPECT_TRUE(FillFormControls(*form_element, json_string));

  HTMLInputElement* num1 = GetInputElement("num1");
  ASSERT_TRUE(num1);
  EXPECT_EQ("42", num1->Value());
}

TEST_F(HTMLFormMcpToolTest, FillFormControls_NumberInput_Double) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="perform task">
      <input id=num1 name=num1 type=number step=0.1>
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));

  String json_string =
      R"JSON(
        {
          "num1": 3.14
        }
      )JSON";

  EXPECT_TRUE(FillFormControls(*form_element, json_string));

  HTMLInputElement* num1 = GetInputElement("num1");
  ASSERT_TRUE(num1);
  EXPECT_EQ("3.14", num1->Value());
}

TEST_F(HTMLFormMcpToolTest, FillFormControls_NumberInput_NumberString) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="perform task">
      <input id=num1 name=num1 type=number>
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));

  String json_string =
      R"JSON(
        {
          "num1": "35"
        }
      )JSON";

  EXPECT_TRUE(FillFormControls(*form_element, json_string));

  HTMLInputElement* num1 = GetInputElement("num1");
  ASSERT_TRUE(num1);
  EXPECT_EQ("35", num1->Value());
}

TEST_F(HTMLFormMcpToolTest, FillFormControls_NumberInput_InvalidString) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="perform task">
      <input id=num1 name=num1 type=number value=15>
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));

  String json_string =
      R"JSON(
        {
          "num1": "foo"
        }
      )JSON";

  EXPECT_FALSE(FillFormControls(*form_element, json_string));

  HTMLInputElement* num1 = GetInputElement("num1");
  ASSERT_TRUE(num1);
  EXPECT_EQ("15", num1->Value());
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

TEST_F(HTMLFormMcpToolTest, ParameterSchema_TextInput) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <input name="text1" type="text">
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));
  String actual = ComputeInputSchema(*form_element);
  std::unique_ptr<JSONValue> expected_json = ParseJSON(R"JSON(
    {
      "type": "object",
      "properties": {
         "text1": {
           "type": "string"
         }
      },
      "required": []
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

TEST_F(HTMLFormMcpToolTest, ParameterSchema_TextInput_Required) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <input name="text1" type="text">
      <input name="text2" type="text" required>
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));
  String actual = ComputeInputSchema(*form_element);
  std::unique_ptr<JSONValue> expected_json = ParseJSON(R"JSON(
    {
      "type": "object",
      "properties": {
         "text1": {
           "type": "string"
         },
         "text2": {
           "type": "string"
         }
      },
      "required": ["text2"]
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

TEST_F(HTMLFormMcpToolTest, ParameterSchema_TextInput_Title) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <input name="text1" type="text" toolparamtitle="Surname">
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));
  String actual = ComputeInputSchema(*form_element);
  std::unique_ptr<JSONValue> expected_json = ParseJSON(R"JSON(
    {
      "type": "object",
      "properties": {
         "text1": {
           "type": "string",
           "title": "Surname"
         }
      },
      "required": []
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

TEST_F(HTMLFormMcpToolTest, ParameterSchema_TextInput_Description) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <input name="text1" type="text" toolparamdescription="Some nice text">
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));
  String actual = ComputeInputSchema(*form_element);
  std::unique_ptr<JSONValue> expected_json = ParseJSON(R"JSON(
    {
      "type": "object",
      "properties": {
         "text1": {
           "type": "string",
           "description": "Some nice text"
         }
      },
      "required": []
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

TEST_F(HTMLFormMcpToolTest, ParameterSchema_TextInput_Label) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <label for="text">Some text</label>
      <input id="text" name="text1" type="text">
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));
  String actual = ComputeInputSchema(*form_element);
  std::unique_ptr<JSONValue> expected_json = ParseJSON(R"JSON(
    {
      "type": "object",
      "properties": {
         "text1": {
           "type": "string",
           "description": "Some text"
         }
      },
      "required": []
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

TEST_F(HTMLFormMcpToolTest, ParameterSchema_TextInput_Label_Multiple) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <label for="text">Label one</label>
      <label for="text">Label two</label>
      <input id="text" name="text1" type="text">
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));
  String actual = ComputeInputSchema(*form_element);
  std::unique_ptr<JSONValue> expected_json = ParseJSON(R"JSON(
    {
      "type": "object",
      "properties": {
         "text1": {
           "type": "string",
           "description": "Label one; Label two"
         }
      },
      "required": []
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

TEST_F(HTMLFormMcpToolTest, ParameterSchema_TextInput_AriaDescription) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <input name="text1" type="text" aria-description="ARIA">
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));
  String actual = ComputeInputSchema(*form_element);
  std::unique_ptr<JSONValue> expected_json = ParseJSON(R"JSON(
    {
      "type": "object",
      "properties": {
         "text1": {
           "type": "string",
           "description": "ARIA"
         }
      },
      "required": []
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

TEST_F(HTMLFormMcpToolTest, ParameterSchema_TextInput_PreferLabelOverAria) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <label for="text">Label</label>
      <input id="text" name="text1" type="text" aria-description="ARIA">
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));
  String actual = ComputeInputSchema(*form_element);
  std::unique_ptr<JSONValue> expected_json = ParseJSON(R"JSON(
    {
      "type": "object",
      "properties": {
         "text1": {
           "type": "string",
           "description": "Label"
         }
      },
      "required": []
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

TEST_F(HTMLFormMcpToolTest, ParameterSchema_TextInput_PreferAttrOverLabel) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <label for="text">Label</label>
      <input
        id="text"
        name="text1"
        type="text"
        toolparamdescription="ATTR"
        aria-description="ARIA">
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));
  String actual = ComputeInputSchema(*form_element);
  std::unique_ptr<JSONValue> expected_json = ParseJSON(R"JSON(
    {
      "type": "object",
      "properties": {
         "text1": {
           "type": "string",
           "description": "ATTR"
         }
      },
      "required": []
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

TEST_F(HTMLFormMcpToolTest, ParameterSchema_Select) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <select name="select" required>
        <option value="Option 1">This is option 1</option>
        <option value="Option 2">This is option 2</option>
        <option value="Option 3">This is option 3</option>
      </select>
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));
  String actual = ComputeInputSchema(*form_element);
  std::unique_ptr<JSONValue> expected_json = ParseJSON(R"JSON(
    {
      "type": "object",
      "properties": {
         "select": {
           "type": "string",
           "oneOf": [
             { "const": "Option 1", "title": "This is option 1" },
             { "const": "Option 2", "title": "This is option 2" },
             { "const": "Option 3", "title": "This is option 3" }
           ]
         }
      },
      "required": ["select"]
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

TEST_F(HTMLFormMcpToolTest, ParameterSchema_Select_Title) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <select name="select" toolparamtitle="Possible Options">
        <option value="Option 1">This is option 1</option>
      </select>
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));
  String actual = ComputeInputSchema(*form_element);
  std::unique_ptr<JSONValue> expected_json = ParseJSON(R"JSON(
    {
      "type": "object",
      "properties": {
         "select": {
           "type": "string",
           "oneOf": [
             { "const": "Option 1", "title": "This is option 1" }
           ],
           "title": "Possible Options"
         }
      },
      "required": []
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

TEST_F(HTMLFormMcpToolTest, ParameterSchema_NumberInput) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <input name="num1" type="number" min="10" max="100">
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));
  String actual = ComputeInputSchema(*form_element);
  std::unique_ptr<JSONValue> expected_json = ParseJSON(R"JSON(
    {
      "type": "object",
      "properties": {
         "num1": {
           "type": "number",
           "minimum": 10,
           "maximum": 100
         }
      },
      "required": []
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

TEST_F(HTMLFormMcpToolTest, ParameterSchema_NumberInput_MinOnly) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <input name="num1" type="number" min="10">
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));
  String actual = ComputeInputSchema(*form_element);
  std::unique_ptr<JSONValue> expected_json = ParseJSON(R"JSON(
    {
      "type": "object",
      "properties": {
         "num1": {
           "type": "number",
           "minimum": 10
         }
      },
      "required": []
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

TEST_F(HTMLFormMcpToolTest, ParameterSchema_NumberInput_MaxOnly) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <input name="num1" type="number" max="100">
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));
  String actual = ComputeInputSchema(*form_element);
  std::unique_ptr<JSONValue> expected_json = ParseJSON(R"JSON(
    {
      "type": "object",
      "properties": {
         "num1": {
           "type": "number",
           "maximum": 100
         }
      },
      "required": []
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

TEST_F(HTMLFormMcpToolTest, ParameterSchema_Checkbox) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <input name="check1" type="checkbox">
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));
  String actual = ComputeInputSchema(*form_element);
  std::unique_ptr<JSONValue> expected_json = ParseJSON(R"JSON(
    {
      "type": "object",
      "properties": {
         "check1": {
           "type": "boolean"
         }
      },
      "required": []
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

TEST_F(HTMLFormMcpToolTest, FillFormControls_Checkbox) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="perform task">
      <input id="check1" name="check1" type="checkbox">
      <input id="check2" name="check2" type="checkbox" checked>
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));

  String json_string =
      R"JSON(
        {
          "check1": true,
          "check2": false
        }
      )JSON";

  EXPECT_TRUE(FillFormControls(*form_element, json_string));

  HTMLInputElement* check1 = GetInputElement("check1");
  HTMLInputElement* check2 = GetInputElement("check2");
  ASSERT_TRUE(check1);
  ASSERT_TRUE(check2);

  EXPECT_TRUE(check1->Checked());
  EXPECT_FALSE(check2->Checked());
}

TEST_F(HTMLFormMcpToolTest, ParameterSchema_RangeInput) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <input name="range1" type="range" min="0" max="50">
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));
  String actual = ComputeInputSchema(*form_element);
  std::unique_ptr<JSONValue> expected_json = ParseJSON(R"JSON(
    {
      "type": "object",
      "properties": {
         "range1": {
           "type": "number",
           "minimum": 0,
           "maximum": 50,
           "multipleOf": 1
         }
      },
      "required": []
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

TEST_F(HTMLFormMcpToolTest, ParameterSchema_RangeInput_Defaults) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <input name="range1" type="range">
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));
  String actual = ComputeInputSchema(*form_element);
  std::unique_ptr<JSONValue> expected_json = ParseJSON(R"JSON(
    {
      "type": "object",
      "properties": {
         "range1": {
           "type": "number",
           "minimum": 0,
           "maximum": 100,
           "multipleOf": 1
         }
      },
      "required": []
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

TEST_F(HTMLFormMcpToolTest, ParameterSchema_RangeInput_Step) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <input name="range1" type="range" min="0" max="10" step="2">
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));
  String actual = ComputeInputSchema(*form_element);
  std::unique_ptr<JSONValue> expected_json = ParseJSON(R"JSON(
    {
      "type": "object",
      "properties": {
         "range1": {
           "type": "number",
           "minimum": 0,
           "maximum": 10,
           "multipleOf": 2
         }
      },
      "required": []
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

TEST_F(HTMLFormMcpToolTest, ParameterSchema_RangeInput_StepBaseOffset) {
  // multipleOf should be absent here, because "min" is not a multiple
  // of "step".
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <input name="range1" type="range" min="1" max="11" step="2">
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));
  String actual = ComputeInputSchema(*form_element);
  std::unique_ptr<JSONValue> expected_json = ParseJSON(R"JSON(
    {
      "type": "object",
      "properties": {
         "range1": {
           "type": "number",
           "minimum": 1,
           "maximum": 11
         }
      },
      "required": []
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

TEST_F(HTMLFormMcpToolTest, FillFormControls_RangeInput) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="perform task">
      <input id="range1" name="range1" type=range min=0 max=100>
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));

  String json_string =
      R"JSON(
        {
          "range1": 75
        }
      )JSON";

  EXPECT_TRUE(FillFormControls(*form_element, json_string));

  HTMLInputElement* range1 = GetInputElement("range1");
  ASSERT_TRUE(range1);
  EXPECT_EQ("75", range1->Value());
}

TEST_F(HTMLFormMcpToolTest, ParameterSchema_DateInput) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <input name="date1" type="date">
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));
  String actual = ComputeInputSchema(*form_element);
  std::unique_ptr<JSONValue> expected_json = ParseJSON(R"JSON(
    {
      "type": "object",
      "properties": {
         "date1": {
           "type": "string",
           "format": "date"
         }
      },
      "required": []
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

TEST_F(HTMLFormMcpToolTest, FillFormControls_DateInput) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="perform task">
      <input id=date1 name=date1 type=date>
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));

  String json_string =
      R"JSON(
        {
          "date1": "2026-01-27"
        }
      )JSON";

  EXPECT_TRUE(FillFormControls(*form_element, json_string));

  HTMLInputElement* date1 = GetInputElement("date1");
  ASSERT_TRUE(date1);
  EXPECT_EQ("2026-01-27", date1->Value());
}

TEST_F(HTMLFormMcpToolTest, ParameterSchema_TextArea) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <textarea name="area1">
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));
  String actual = ComputeInputSchema(*form_element);
  std::unique_ptr<JSONValue> expected_json = ParseJSON(R"JSON(
    {
      "type": "object",
      "properties": {
         "area1": {
           "type": "string"
         }
      },
      "required": []
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

TEST_F(HTMLFormMcpToolTest, FillFormControls_TextArea) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="perform task">
      <textarea id=area1 name=area1>
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));

  String json_string =
      R"JSON(
        {
          "area1": "This is textarea content"
        }
      )JSON";

  EXPECT_TRUE(FillFormControls(*form_element, json_string));

  HTMLTextAreaElement* area1 = GetTextAreaElement("area1");
  ASSERT_TRUE(area1);
  EXPECT_EQ("This is textarea content", area1->Value());
}

TEST_F(HTMLFormMcpToolTest, FillFormControls_InvalidValue) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="perform task">
      <input id=date name=date type=date>
      <input id=number name=number type=number>
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));

  String inputs[] = {
      R"JSON({ "date": "error" })JSON",
      R"JSON({ "number": "error" })JSON",
  };
  for (auto json : inputs) {
    EXPECT_FALSE(FillFormControls(*form_element, json)) << json;
  }
}

TEST_F(HTMLFormMcpToolTest, FillFormControls_EmptyStringValid) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="perform task">
      <input id=date name=date type=date>
      <input id=number name=number type=number>
      <input id=range name=range type=range>
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));

  String inputs[] = {
      R"JSON({ "date": "" })JSON",
      R"JSON({ "number": "" })JSON",
      R"JSON({ "range": "" })JSON",
  };
  for (auto json : inputs) {
    EXPECT_TRUE(FillFormControls(*form_element, json)) << json;
  }
}

}  // namespace blink
