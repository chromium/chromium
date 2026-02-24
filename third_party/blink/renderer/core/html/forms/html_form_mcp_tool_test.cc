// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/fileapi/file_list.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
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
  ScopedWebMCPDeclarativeFileInputForTest scoped_file_feature{true};
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
      <input id=num1 name=num1 type=number value="7">
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

TEST_F(HTMLFormMcpToolTest, ParameterSchema_Disabled) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <input name="text1" type="text">
      <input name="text2" type="text" disabled>
      <input name="text3" type="text" disabled toolparamtitle="TITLE">
      <textarea name="area1" disabled>
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

TEST_F(HTMLFormMcpToolTest, ParameterSchema_Readonly) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <input name="text1" type="text">
      <input name="text2" type="text" readonly>
      <input name="text3" type="text" readonly toolparamtitle="TITLE">
      <textarea name="area1" readonly>
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

// TODO(crbug.com/475972617): Support duplicate names.
TEST_F(HTMLFormMcpToolTest, ParameterSchema_TextInput_DuplicateName) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <input name="text1" type="text">
      <input name="text2" type="text">
      <input name="text2" type="text">
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));
  String actual = ComputeInputSchema(*form_element);
  // test1 is supported, text2 is not.
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

TEST_F(HTMLFormMcpToolTest, ParameterSchema_ImplicitLabelText) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <label>
        LABEL
        <select name="select" required>
          <option value="Option 1">This is option 1</option>
          <option value="Option 2">This is option 2</option>
          <option value="Option 3">This is option 3</option>
        </select>
        <button>Button text</button>
      </label>
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
           ],
           "enum": ["Option 1", "Option 2", "Option 3"],
           "description": "LABEL"
         }
      },
      "required": ["select"]
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
           ],
           "enum": ["Option 1", "Option 2", "Option 3"]
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
           "enum": ["Option 1"],
           "title": "Possible Options"
         }
      },
      "required": []
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

TEST_F(HTMLFormMcpToolTest, ParameterSchema_Select_Multiple) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <select name="select" multiple required>
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
           "type": "array",
           "items": {
             "type": "string",
             "oneOf": [
               { "const": "Option 1", "title": "This is option 1" },
               { "const": "Option 2", "title": "This is option 2" },
               { "const": "Option 3", "title": "This is option 3" }
             ],
             "enum": ["Option 1", "Option 2", "Option 3"]
           },
           "uniqueItems": true
         }
      },
      "required": ["select"]
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

TEST_F(HTMLFormMcpToolTest, FillFormControls_Select_Multiple) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="perform task">
      <select id="select1" name="select1" multiple>
        <option id="o1" value="v1">Option 1</option>
        <option id="o2" value="v2">Option 2</option>
        <option id="o3" value="v3">Option 3</option>
      </select>
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));

  HTMLOptionElement* o1 = DynamicTo<HTMLOptionElement>(
      GetDocument().getElementById(AtomicString("o1")));
  HTMLOptionElement* o2 = DynamicTo<HTMLOptionElement>(
      GetDocument().getElementById(AtomicString("o2")));
  HTMLOptionElement* o3 = DynamicTo<HTMLOptionElement>(
      GetDocument().getElementById(AtomicString("o3")));
  ASSERT_TRUE(o1);
  ASSERT_TRUE(o2);
  ASSERT_TRUE(o3);

  EXPECT_FALSE(o1->Selected());
  EXPECT_FALSE(o2->Selected());
  EXPECT_FALSE(o3->Selected());

  EXPECT_TRUE(
      FillFormControls(*form_element, R"JSON({"select1": ["v1", "v3"]})JSON"));
  EXPECT_TRUE(o1->Selected());
  EXPECT_FALSE(o2->Selected());
  EXPECT_TRUE(o3->Selected());

  EXPECT_TRUE(
      FillFormControls(*form_element, R"JSON({"select1": ["v2"]})JSON"));
  EXPECT_FALSE(o1->Selected());
  EXPECT_TRUE(o2->Selected());
  EXPECT_FALSE(o3->Selected());

  EXPECT_TRUE(FillFormControls(*form_element, R"JSON({"select1": []})JSON"));
  EXPECT_FALSE(o1->Selected());
  EXPECT_FALSE(o2->Selected());
  EXPECT_FALSE(o3->Selected());
}

// The same as the previous test, but size=1 makes the <select> use MenuList.
TEST_F(HTMLFormMcpToolTest, FillFormControls_Select_Multiple_MenuList) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="perform task">
      <select id="select1" name="select1" size=1 multiple>
        <option id="o1" value="v1">Option 1</option>
        <option id="o2" value="v2">Option 2</option>
        <option id="o3" value="v3">Option 3</option>
      </select>
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));

  HTMLOptionElement* o1 = DynamicTo<HTMLOptionElement>(
      GetDocument().getElementById(AtomicString("o1")));
  HTMLOptionElement* o2 = DynamicTo<HTMLOptionElement>(
      GetDocument().getElementById(AtomicString("o2")));
  HTMLOptionElement* o3 = DynamicTo<HTMLOptionElement>(
      GetDocument().getElementById(AtomicString("o3")));
  ASSERT_TRUE(o1);
  ASSERT_TRUE(o2);
  ASSERT_TRUE(o3);

  EXPECT_FALSE(o1->Selected());
  EXPECT_FALSE(o2->Selected());
  EXPECT_FALSE(o3->Selected());

  EXPECT_TRUE(
      FillFormControls(*form_element, R"JSON({"select1": ["v1", "v3"]})JSON"));
  EXPECT_TRUE(o1->Selected());
  EXPECT_FALSE(o2->Selected());
  EXPECT_TRUE(o3->Selected());

  EXPECT_TRUE(
      FillFormControls(*form_element, R"JSON({"select1": ["v2"]})JSON"));
  EXPECT_FALSE(o1->Selected());
  EXPECT_TRUE(o2->Selected());
  EXPECT_FALSE(o3->Selected());

  EXPECT_TRUE(FillFormControls(*form_element, R"JSON({"select1": []})JSON"));
  EXPECT_FALSE(o1->Selected());
  EXPECT_FALSE(o2->Selected());
  EXPECT_FALSE(o3->Selected());
}

TEST_F(HTMLFormMcpToolTest, ParameterSchema_NumberInput) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <input name="num1" type="number" min="10" max="100">
      <input name="num2" type="number" min="30" max="60" step="10">
      <input name="num3" type="number" min="15" step="10">
      <input name="num4" type="number" step="13">
      <input name="num5" type="number" step="0.1">
      <input name="num6" type="number" min="0.15" step="0.1">
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
           "maximum": 100,
           "multipleOf": 1
         },
         "num2": {
           "type": "number",
           "minimum": 30,
           "maximum": 60,
           "multipleOf": 10
         },
         "num3": {
           "type": "number",
           "minimum": 15
         },
         "num4": {
           "type": "number",
           "multipleOf": 13
         },
         "num5": {
           "type": "number",
           "multipleOf": 0.1
         },
         "num6": {
           "type": "number",
           "minimum": 0.15
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
           "minimum": 10,
           "multipleOf": 1
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

TEST_F(HTMLFormMcpToolTest, ParameterSchema_Checkbox_Multiple) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="pick fruits you like">
      <label>
        <input id="apple" name="fruit" type="checkbox" value="apple">
        Apple
      </label>
      <label>
        <input id="melon" name="fruit" type="checkbox" value="melon">
        Melon
      </label>
      <label>
        <input id="grape" name="fruit" type="checkbox" value="grape">
        Grape
      </label>
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
         "fruit": {
           "type": "array",
           "items": {
             "type": "string",
             "oneOf": [
               {
                 "const": "apple",
                 "title": "Apple"
               },
               {
                 "const": "melon",
                 "title": "Melon"
               },
               {
                 "const": "grape",
                 "title": "Grape"
               }
             ],
             "enum": ["apple", "melon", "grape"]
           },
           "uniqueItems": true
         }
      },
      "required": []
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

TEST_F(HTMLFormMcpToolTest, ParameterSchema_Checkbox_ToolParamAttributes) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="pick fruits you like">
      <input
        id="apple"
        name="fruit"
        type="checkbox"
        value="apple"
        toolparamtitle="TITLE"
        toolparamdescription="DESC"
        >
      <input
        id="melon"
        name="fruit"
        type="checkbox"
        value="melon"
        toolparamtitle="ERR"
        toolparamdescription="ERR"
        >
      <input id="grape" name="fruit" type="checkbox" value="grape">
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
         "fruit": {
           "type": "array",
           "items": {
             "type": "string",
             "oneOf": [
               {
                 "const": "apple"
               },
               {
                 "const": "melon"
               },
               {
                 "const": "grape"
               }
             ],
             "enum": ["apple", "melon", "grape"]
           },
           "uniqueItems": true,
           "title": "TITLE",
           "description": "DESC"
         }
      },
      "required": []
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

TEST_F(HTMLFormMcpToolTest, FillFormControls_CheckboxMultiple) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="pick fruits you like">
      <input id="apple" name="fruit" type="checkbox" value="apple">
      <input id="melon" name="fruit" type="checkbox" value="melon">
      <input id="grape" name="fruit" type="checkbox" value="grape">
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));

  HTMLInputElement* apple = GetInputElement("apple");
  HTMLInputElement* melon = GetInputElement("melon");
  HTMLInputElement* grape = GetInputElement("grape");
  ASSERT_TRUE(apple);
  ASSERT_TRUE(melon);
  ASSERT_TRUE(grape);
  EXPECT_FALSE(apple->Checked());
  EXPECT_FALSE(melon->Checked());
  EXPECT_FALSE(grape->Checked());

  // Select some option.
  EXPECT_TRUE(FillFormControls(*form_element, R"JSON(
      {
        "fruit": [ "melon" ]
      }
    )JSON"));
  EXPECT_FALSE(apple->Checked());
  EXPECT_TRUE(melon->Checked());
  EXPECT_FALSE(grape->Checked());

  // Select some other option. (Should automatically uncheck other options.)
  EXPECT_TRUE(FillFormControls(*form_element, R"JSON(
      {
        "fruit": [ "grape" ]
      }
    )JSON"));
  EXPECT_FALSE(apple->Checked());
  EXPECT_FALSE(melon->Checked());
  EXPECT_TRUE(grape->Checked());

  // Select no options.
  EXPECT_TRUE(FillFormControls(*form_element, R"JSON(
      {
        "fruit": []
      }
    )JSON"));
  EXPECT_FALSE(apple->Checked());
  EXPECT_FALSE(melon->Checked());
  EXPECT_FALSE(grape->Checked());

  // Select many options.
  EXPECT_TRUE(FillFormControls(*form_element, R"JSON(
      {
        "fruit": ["melon", "grape", "apple"]
      }
    )JSON"));
  EXPECT_TRUE(apple->Checked());
  EXPECT_TRUE(melon->Checked());
  EXPECT_TRUE(grape->Checked());

  // Uncheck one of them.
  EXPECT_TRUE(FillFormControls(*form_element, R"JSON(
      {
        "fruit": ["grape", "apple"]
      }
    )JSON"));
  EXPECT_TRUE(apple->Checked());
  EXPECT_FALSE(melon->Checked());
  EXPECT_TRUE(grape->Checked());
}

TEST_F(HTMLFormMcpToolTest, FillFormControls_CheckboxMultiple_Invalid) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="pick fruits you like">
      <input id="apple" name="fruit" type="checkbox" value="apple">
      <input id="melon" name="fruit" type="checkbox" value="melon">
      <input id="grape" name="fruit" type="checkbox" value="grape">
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));

  HTMLInputElement* apple = GetInputElement("apple");
  HTMLInputElement* melon = GetInputElement("melon");
  HTMLInputElement* grape = GetInputElement("grape");
  ASSERT_TRUE(apple);
  ASSERT_TRUE(melon);
  ASSERT_TRUE(grape);
  EXPECT_FALSE(apple->Checked());
  EXPECT_FALSE(melon->Checked());
  EXPECT_FALSE(grape->Checked());

  // Unknown option.
  EXPECT_FALSE(FillFormControls(*form_element, R"JSON(
      {
        "fruit": [ "unknown" ]
      }
    )JSON"));
  EXPECT_FALSE(apple->Checked());
  EXPECT_FALSE(melon->Checked());
  EXPECT_FALSE(grape->Checked());

  // Unknown option among valid options.
  EXPECT_FALSE(FillFormControls(*form_element, R"JSON(
      {
        "fruit": [ "apple", "unknown", "grape" ]
      }
    )JSON"));
  EXPECT_FALSE(apple->Checked());
  EXPECT_FALSE(melon->Checked());
  EXPECT_FALSE(grape->Checked());

  // Extra option among valid options.
  EXPECT_FALSE(FillFormControls(*form_element, R"JSON(
      {
        "fruit": [ "apple", "melon", "grape", "extra" ]
      }
    )JSON"));
  EXPECT_FALSE(apple->Checked());
  EXPECT_FALSE(melon->Checked());
  EXPECT_FALSE(grape->Checked());

  // Non-unique options.
  EXPECT_FALSE(FillFormControls(*form_element, R"JSON(
      {
        "fruit": [ "apple", "melon", "grape", "apple" ]
      }
    )JSON"));
  EXPECT_FALSE(apple->Checked());
  EXPECT_FALSE(melon->Checked());
  EXPECT_FALSE(grape->Checked());

  // Invalid data types.
  EXPECT_FALSE(FillFormControls(*form_element, R"JSON(
      {
        "fruit": true
      }
    )JSON"));
  EXPECT_FALSE(FillFormControls(*form_element, R"JSON(
      {
        "fruit": {}
      }
    )JSON"));
  EXPECT_FALSE(FillFormControls(*form_element, R"JSON(
      {
        "fruit": "melon"
      }
    )JSON"));
  EXPECT_FALSE(apple->Checked());
  EXPECT_FALSE(melon->Checked());
  EXPECT_FALSE(grape->Checked());
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
           "format": "date",
           "description": "Dates MUST be provided in 'YYYY-MM-DD' format."
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

TEST_F(HTMLFormMcpToolTest, ParameterSchema_DatetimeLocalInput) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <input name="datetime1" type="datetime-local">
      <input name="datetime2" type="datetime-local" step="1">
      <input name="datetime3" type="datetime-local" step="0.001">
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
         "datetime1": {
           "type": "string",
           "format": "^[0-9]{4}-(0[1-9]|1[0-2])-[0-9]{2}T([01][0-9]|2[0-3]):[0-5][0-9]$"
         },
         "datetime2": {
           "type": "string",
           "format": "^[0-9]{4}-(0[1-9]|1[0-2])-[0-9]{2}T([01][0-9]|2[0-3]):[0-5][0-9](:[0-5][0-9])?$"
         },
         "datetime3": {
           "type": "string",
           "format": "^[0-9]{4}-(0[1-9]|1[0-2])-[0-9]{2}T([01][0-9]|2[0-3]):[0-5][0-9](:[0-5][0-9](\\.[0-9]{1,3})?)?$"
         }
      },
      "required": []
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

TEST_F(HTMLFormMcpToolTest, FillFormControls_DatetimeLocalInput) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="perform task">
      <input id=datetime1 name=datetime1 type=datetime-local>
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));

  String json_string =
      R"JSON(
        {
          "datetime1": "2026-02-11T14:13"
        }
      )JSON";

  EXPECT_TRUE(FillFormControls(*form_element, json_string));

  HTMLInputElement* datetime1 = GetInputElement("datetime1");
  ASSERT_TRUE(datetime1);
  EXPECT_EQ("2026-02-11T14:13", datetime1->Value());
}

TEST_F(HTMLFormMcpToolTest, ParameterSchema_MonthInput) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <input name="month1" type="month">
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
         "month1": {
           "type": "string",
           "format": "^[0-9]{4}-(0[1-9]|1[0-2])$"
         }
      },
      "required": []
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

TEST_F(HTMLFormMcpToolTest, FillFormControls_MonthInput) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="perform task">
      <input id=month1 name=month1 type=month>
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));

  String json_string =
      R"JSON(
        {
          "month1": "2026-02"
        }
      )JSON";

  EXPECT_TRUE(FillFormControls(*form_element, json_string));

  HTMLInputElement* month1 = GetInputElement("month1");
  ASSERT_TRUE(month1);
  EXPECT_EQ("2026-02", month1->Value());
}

TEST_F(HTMLFormMcpToolTest, ParameterSchema_WeekInput) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <input name="week1" type="week">
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
         "week1": {
           "type": "string",
           "format": "^[0-9]{4}-W(0[1-9]|[1-4][0-9]|5[0-3])$"
         }
      },
      "required": []
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

TEST_F(HTMLFormMcpToolTest, FillFormControls_WeekInput) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="perform task">
      <input id=week1 name=week1 type=week>
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));

  String json_string =
      R"JSON(
        {
          "week1": "2026-W05"
        }
      )JSON";

  EXPECT_TRUE(FillFormControls(*form_element, json_string));

  HTMLInputElement* week1 = GetInputElement("week1");
  ASSERT_TRUE(week1);
  EXPECT_EQ("2026-W05", week1->Value());
}

TEST_F(HTMLFormMcpToolTest, ParameterSchema_TimeInput) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <input name="time1" type="time">
      <input name="time2" type="time" step="1">
      <input name="time3" type="time" step="0.001">
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
         "time1": {
           "type": "string",
           "format": "^([01][0-9]|2[0-3]):[0-5][0-9]$"
         },
         "time2": {
           "type": "string",
           "format": "^([01][0-9]|2[0-3]):[0-5][0-9](:[0-5][0-9])?$"
         },
         "time3": {
           "type": "string",
           "format": "^([01][0-9]|2[0-3]):[0-5][0-9](:[0-5][0-9](\\.[0-9]{1,3})?)?$"
         }
      },
      "required": []
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

TEST_F(HTMLFormMcpToolTest, FillFormControls_TimeInput) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="perform task">
      <input id=time1 name=time1 type=time>
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));

  String json_string =
      R"JSON(
        {
          "time1": "20:20:39"
        }
      )JSON";

  EXPECT_TRUE(FillFormControls(*form_element, json_string));

  HTMLInputElement* time1 = GetInputElement("time1");
  ASSERT_TRUE(time1);
  EXPECT_EQ("20:20:39", time1->Value());
}

TEST_F(HTMLFormMcpToolTest, ParameterSchema_ColorInput) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <input name="color1" type="color">
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
         "color1": {
           "type": "string",
           "format": "^#[0-9a-zA-Z]{6}$"
         }
      },
      "required": []
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

TEST_F(HTMLFormMcpToolTest, FillFormControls_ColorInput) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="perform task">
      <input id=color1 name=color1 type=color>
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));

  String json_string =
      R"JSON(
        {
          "color1": "#FaB000"
        }
      )JSON";

  EXPECT_TRUE(FillFormControls(*form_element, json_string));

  HTMLInputElement* color1 = GetInputElement("color1");
  ASSERT_TRUE(color1);
  EXPECT_EQ("#fab000", color1->Value());
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

TEST_F(HTMLFormMcpToolTest, ParameterSchema_BaseTextInput) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <input name="email1" type="email">
      <input name="search1" type="search">
      <input name="tel1" type="tel">
      <input name="url1" type="url">
      <input name="hidden1" type="hidden">
      <input name="hidden2" type="hidden" toolparamtitle="TITLE">
      <input name="hidden3" type="hidden" toolparamdescription="DESC">
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
         "email1": {
           "type": "string"
         },
         "search1": {
           "type": "string"
         },
         "tel1": {
           "type": "string"
         },
         "url1": {
           "type": "string"
         },
         "hidden2": {
           "type": "string",
           "title": "TITLE"
         },
         "hidden3": {
           "type": "string",
           "description": "DESC"
         }
      },
      "required": []
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

TEST_F(HTMLFormMcpToolTest, FillFormControls_EmailInput) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="perform task">
      <input id=email1 name=email1 type=email>
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));

  String json_string =
      R"JSON(
        {
          "email1": "john@doe.org"
        }
      )JSON";

  EXPECT_TRUE(FillFormControls(*form_element, json_string));

  HTMLInputElement* email1 = GetInputElement("email1");
  ASSERT_TRUE(email1);
  EXPECT_EQ("john@doe.org", email1->Value());
}

TEST_F(HTMLFormMcpToolTest, FillFormControls_SearchInput) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="perform task">
      <input id=search1 name=search1 type=search>
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));

  String json_string =
      R"JSON(
        {
          "search1": "cat videos"
        }
      )JSON";

  EXPECT_TRUE(FillFormControls(*form_element, json_string));

  HTMLInputElement* search1 = GetInputElement("search1");
  ASSERT_TRUE(search1);
  EXPECT_EQ("cat videos", search1->Value());
}

TEST_F(HTMLFormMcpToolTest, FillFormControls_TelephoneInput) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="perform task">
      <input id=tel1 name=tel1 type=tel>
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));

  String json_string =
      R"JSON(
        {
          "tel1": "+47 555 55 555"
        }
      )JSON";

  EXPECT_TRUE(FillFormControls(*form_element, json_string));

  HTMLInputElement* tel1 = GetInputElement("tel1");
  ASSERT_TRUE(tel1);
  EXPECT_EQ("+47 555 55 555", tel1->Value());
}

TEST_F(HTMLFormMcpToolTest, FillFormControls_UrlInput) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="perform task">
      <input id=url1 name=url1 type=url>
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));

  String json_string =
      R"JSON(
        {
          "url1": "https://www.google.com"
        }
      )JSON";

  EXPECT_TRUE(FillFormControls(*form_element, json_string));

  HTMLInputElement* url1 = GetInputElement("url1");
  ASSERT_TRUE(url1);
  EXPECT_EQ("https://www.google.com", url1->Value());
}

TEST_F(HTMLFormMcpToolTest, FillFormControls_HiddenInput) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="perform task">
      <input id=hidden1 name=hidden1 type=hidden value="initial1">
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));

  String json_string =
      R"JSON(
        {
          "hidden1": "1234"
        }
      )JSON";

  EXPECT_FALSE(FillFormControls(*form_element, json_string));

  HTMLInputElement* hidden1 = GetInputElement("hidden1");
  ASSERT_TRUE(hidden1);

  // A failure means no form control values were changed.
  EXPECT_EQ("initial1", hidden1->Value());
}

TEST_F(HTMLFormMcpToolTest, FillFormControls_HiddenInput_Title) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="perform task">
      <input id=hidden1 name=hidden1 type=hidden value="initial1" toolparamtitle="TITLE">
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));

  String json_string =
      R"JSON(
        {
          "hidden1": "1234"
        }
      )JSON";

  EXPECT_TRUE(FillFormControls(*form_element, json_string));

  HTMLInputElement* hidden1 = GetInputElement("hidden1");
  ASSERT_TRUE(hidden1);
  EXPECT_EQ("1234", hidden1->Value());
}

TEST_F(HTMLFormMcpToolTest, FillFormControls_HiddenInput_Description) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="perform task">
      <input id=hidden1 name=hidden1 type=hidden value="initial1" toolparamdescription="DESC">
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));

  String json_string =
      R"JSON(
        {
          "hidden1": "1234"
        }
      )JSON";

  EXPECT_TRUE(FillFormControls(*form_element, json_string));

  HTMLInputElement* hidden1 = GetInputElement("hidden1");
  ASSERT_TRUE(hidden1);
  EXPECT_EQ("1234", hidden1->Value());
}

TEST_F(HTMLFormMcpToolTest, FillFormControls_InvalidValue) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="perform task">
      <input id=date name=date type=date>
      <input id=number name=number type=number>
      <input id=time name=time type=time>
      <select id=select-multi name=select-multi multiple>
        <option value=v1>Option 1</option>
        <option value=v2>Option 2</option>
      </select>
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));

  String inputs[] = {
      R"JSON({ "date": "error" })JSON",
      R"JSON({ "number": "error" })JSON",
      R"JSON({ "time": "25:00:00" })JSON",
      R"JSON({ "time": "20:20:39+00:00" })JSON",
      R"JSON({ "select-multi": ["unknown"] })JSON",
      R"JSON({ "select-multi": ["unknown", "v1"] })JSON",
      R"JSON({ "select-multi": ["v1", "unknown"] })JSON",
      R"JSON({ "select-multi": "v1" })JSON",
      R"JSON({ "select-multi": ["v1", "v1"] })JSON",
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
      <input id=email name=email type=email>
      <input id=number name=number type=number>
      <input id=password name=password type=password>
      <input id=range name=range type=range>
      <input id=search name=search type=search>
      <input id=tel name=tel type=tel>
      <input id=url name=url type=url>
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));

  String inputs[] = {
      R"JSON({ "date": "" })JSON",     R"JSON({ "email": "" })JSON",
      R"JSON({ "password": "" })JSON", R"JSON({ "search": "" })JSON",
      R"JSON({ "tel": "" })JSON",      R"JSON({ "url": "" })JSON",
  };
  for (auto json : inputs) {
    EXPECT_TRUE(FillFormControls(*form_element, json)) << json;
  }
}

TEST_F(HTMLFormMcpToolTest, ParameterSchema_Radio) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <label>
        <input type=radio name=size value=s>
        Small
      </label>
      <label>
        <input type=radio name=size value=m>
        Medium
      </label>
      <label>
        <input type=radio name=size value=l>
        Large
      </label>
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
         "size": {
           "type": "string",
           "oneOf": [
             {
               "const": "s",
               "title": "Small"
             },
             {
               "const": "m",
               "title": "Medium"
             },
             {
               "const": "l",
               "title": "Large"
             }
           ],
           "enum": ["s", "m", "l"]
         }
      },
      "required": []
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

TEST_F(HTMLFormMcpToolTest, ParameterSchema_Radio_Multiple) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <input type=radio name=size value=s>
      <input type=radio name=size value=m>
      <input type=radio name=size value=l>
      <input type=radio name=item value=hoodie>
      <input type=radio name=item value=shirt>
      <input type=radio name=item value=hat>
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
         "size": {
           "type": "string",
           "oneOf": [
             {
               "const": "s"
             },
             {
               "const": "m"
             },
             {
               "const": "l"
             }
           ],
           "enum": ["s", "m", "l"]
         },
         "item": {
           "type": "string",
           "oneOf": [
             {
               "const": "hoodie"
             },
             {
               "const": "shirt"
             },
             {
               "const": "hat"
             }
           ],
           "enum": ["hoodie", "shirt", "hat"]
         }
      },
      "required": []
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

TEST_F(HTMLFormMcpToolTest, ParameterSchema_Radio_MixedType) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <input type=text name=foo>
      <input type=radio name=size value=s>
      <input type=radio name=size value=m>
      <input type=radio name=size value=l>
      <input type=text name=size> <!-- Oops! -->
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));
  String actual = ComputeInputSchema(*form_element);
  // No "size" parameter is expected here, because the name is used
  // for both type=radio and type=text.
  std::unique_ptr<JSONValue> expected_json = ParseJSON(R"JSON(
    {
      "type": "object",
      "properties": {
         "foo": {
           "type": "string"
         }
      },
      "required": []
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

TEST_F(HTMLFormMcpToolTest, ParameterSchema_Radio_Required) {
  // The whole parameter becomes required if one of the radio buttons
  // are required.
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <input type=radio name=size value=s>
      <input type=radio name=size value=m required>
      <input type=radio name=size value=l>
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
         "size": {
           "type": "string",
           "oneOf": [
             {
               "const": "s"
             },
             {
               "const": "m"
             },
             {
               "const": "l"
             }
           ],
           "enum": ["s", "m", "l"]
         }
      },
      "required": ["size"]
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

// The toolparamdescription for the parameter (as a whole) is
// sources from the first <input type=radio> in the group.
TEST_F(HTMLFormMcpToolTest, ParameterSchema_Radio_ToolParamDescription) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <input type=radio name=size value=s toolparamdescription="DESC">
      <input type=radio name=size value=m toolparamdescription="ERR1">
      <input type=radio name=size value=l toolparamdescription="ERR2">
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
         "size": {
           "type": "string",
           "oneOf": [
             {
               "const": "s"
             },
             {
               "const": "m"
             },
             {
               "const": "l"
             }
           ],
           "enum": ["s", "m", "l"],
           "description": "DESC"
         }
      },
      "required": []
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

// The toolparamtitle for the parameter (as a whole) is
// sources from the first <input type=radio> in the group.
TEST_F(HTMLFormMcpToolTest, ParameterSchema_Radio_ToolParamTitle) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <input type=radio name=size value=s toolparamtitle="TITLE">
      <input type=radio name=size value=m toolparamtitle="ERR1">
      <input type=radio name=size value=l toolparamtitle="ERR2">
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
         "size": {
           "type": "string",
           "oneOf": [
             {
               "const": "s"
             },
             {
               "const": "m"
             },
             {
               "const": "l"
             }
           ],
           "enum": ["s", "m", "l"],
           "title": "TITLE"
         }
      },
      "required": []
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

TEST_F(HTMLFormMcpToolTest, FillFormControls_FillRadio) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <input id=s type=radio name=size value=s>
      <input id=m type=radio name=size value=m>
      <input id=l type=radio name=size value=l>
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));

  HTMLInputElement* s = GetInputElement("s");
  HTMLInputElement* m = GetInputElement("m");
  HTMLInputElement* l = GetInputElement("l");
  ASSERT_TRUE(s);
  ASSERT_TRUE(m);
  ASSERT_TRUE(l);

  EXPECT_FALSE(s->Checked());
  EXPECT_FALSE(m->Checked());
  EXPECT_FALSE(l->Checked());

  {
    String json_string =
        R"JSON(
        {
          "size": "s"
        }
      )JSON";
    EXPECT_TRUE(FillFormControls(*form_element, json_string));
    EXPECT_TRUE(s->Checked());
    EXPECT_FALSE(m->Checked());
    EXPECT_FALSE(l->Checked());
  }

  {
    String json_string =
        R"JSON(
        {
          "size": "m"
        }
      )JSON";
    EXPECT_TRUE(FillFormControls(*form_element, json_string));
    EXPECT_FALSE(s->Checked());
    EXPECT_TRUE(m->Checked());
    EXPECT_FALSE(l->Checked());
  }

  {
    String json_string =
        R"JSON(
        {
          "size": "l"
        }
      )JSON";
    EXPECT_TRUE(FillFormControls(*form_element, json_string));
    EXPECT_FALSE(s->Checked());
    EXPECT_FALSE(m->Checked());
    EXPECT_TRUE(l->Checked());
  }
}

TEST_F(HTMLFormMcpToolTest, FillFormControls_FillRadio_Invalid) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <input id=s type=radio name=size value=s>
      <input id=m type=radio name=size value=m>
      <input id=l type=radio name=size value=l>
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));

  HTMLInputElement* s = GetInputElement("s");
  HTMLInputElement* m = GetInputElement("m");
  HTMLInputElement* l = GetInputElement("l");
  ASSERT_TRUE(s);
  ASSERT_TRUE(m);
  ASSERT_TRUE(l);

  EXPECT_FALSE(s->Checked());
  EXPECT_FALSE(m->Checked());
  EXPECT_FALSE(l->Checked());

  String json_string =
      R"JSON(
      {
        "size": "xl"
      }
    )JSON";
  EXPECT_FALSE(FillFormControls(*form_element, json_string));
  EXPECT_FALSE(s->Checked());
  EXPECT_FALSE(m->Checked());
  EXPECT_FALSE(l->Checked());
}

TEST_F(HTMLFormMcpToolTest, ParameterSchema_FormAssociatedCustom) {
  GetDocument().GetSettings()->SetScriptEnabled(true);
  ClassicScript::CreateUnspecifiedScript(R"JS(
      class MyInput extends HTMLElement {
        static formAssociated = true;
        constructor() {
          super();
          this._internals = this.attachInternals();
          this._internals.setToolParamSchema(`{ "type":"string" }`);
        }
      }
      customElements.define('my-input', MyInput);

      document.body.innerHTML = `
        <form id="form" toolname="mytool" tooldescription="perform task">
          <input type=text name=text1>
          <my-input
            id=my-input
            name=custom-input
            toolparamdescription="Just some custom text"
          ></my-input>
        </form>
      `;
   )JS")
      ->RunScript(GetDocument().domWindow());

  UpdateAllLifecyclePhasesForTest();

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);

  auto* my_input = DynamicTo<HTMLElement>(
      GetDocument().getElementById(AtomicString("my-input")));
  ASSERT_TRUE(my_input);
  EXPECT_EQ(CustomElementState::kCustom, my_input->GetCustomElementState());
  ASSERT_TRUE(my_input->GetCustomElementDefinition());
  ASSERT_TRUE(my_input->IsFormAssociatedCustomElement());

  ASSERT_TRUE(IsValidWebMCPForm(*form_element));
  String actual = ComputeInputSchema(*form_element);
  std::unique_ptr<JSONValue> expected_json = ParseJSON(R"JSON(
    {
      "type": "object",
      "properties": {
         "text1": {
            "type": "string"
         },
         "custom-input": {
            "type": "string",
            "description": "Just some custom text"
         }
      },
      "required": []
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

TEST_F(HTMLFormMcpToolTest, ParameterSchema_FileInput) {
  SetBodyInnerHTML(
      R"HTML(
    <form id="form" toolname="mytool" tooldescription="perform task">
      <input name="file1" type="file">
      <input name="file2" type="file" multiple>
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
         "file1": {
           "type": "string"
         },
         "file2": {
           "type": "array",
           "items": {
             "type": "string"
           }
         }
      },
      "required": []
    }
  )JSON");
  ASSERT_TRUE(expected_json);
  EXPECT_EQ(expected_json->ToJSONString(), actual);
}

TEST_F(HTMLFormMcpToolTest, FillFormControls_FileInput) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="perform task">
      <input id=file1 name=file1 type=file>
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));

  String json_string =
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
      R"JSON(
        {
          "file1": "C:\\Users\\johndoe\\avatar.png"
        }
      )JSON"
#else
      R"JSON(
        {
          "file1": "/home/johndoe/avatar.png"
        }
      )JSON"
#endif
      ;

  EXPECT_TRUE(FillFormControls(*form_element, json_string));

  HTMLInputElement* file1 = GetInputElement("file1");
  ASSERT_TRUE(file1);
  FileList* file_list = file1->files();
  ASSERT_TRUE(file_list);
  ASSERT_EQ(file_list->length(), 1);
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
  EXPECT_EQ(file_list->item(0)->GetPath(), "C:\\Users\\johndoe\\avatar.png");
#else
  EXPECT_EQ(file_list->item(0)->GetPath(), "/home/johndoe/avatar.png");
#endif
}

TEST_F(HTMLFormMcpToolTest, FillFormControls_FileInput_Multiple) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="perform task">
      <input id=file1 name=file1 type=file multiple>
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));

  String json_string =
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
      R"JSON(
        {
          "file1": [ "C:\\Users\\johndoe\\avatar.png",
                     "C:\\Users\\johndoe\\avatar_old.png" ]
        }
      )JSON"
#else
      R"JSON(
        {
          "file1": [ "/home/johndoe/avatar.png",
                     "/home/johndoe/avatar_old.png" ]
        }
      )JSON"
#endif
      ;

  EXPECT_TRUE(FillFormControls(*form_element, json_string));

  HTMLInputElement* file1 = GetInputElement("file1");
  ASSERT_TRUE(file1);
  FileList* file_list = file1->files();
  ASSERT_TRUE(file_list);
  ASSERT_EQ(file_list->length(), 2);
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
  EXPECT_EQ(file_list->item(0)->GetPath(), "C:\\Users\\johndoe\\avatar.png");
  EXPECT_EQ(file_list->item(1)->GetPath(),
            "C:\\Users\\johndoe\\avatar_old.png");
#else
  EXPECT_EQ(file_list->item(0)->GetPath(), "/home/johndoe/avatar.png");
  EXPECT_EQ(file_list->item(1)->GetPath(), "/home/johndoe/avatar_old.png");
#endif
}

TEST_F(HTMLFormMcpToolTest, FillFormControls_FileInput_Invalid) {
  SetBodyInnerHTML(
      R"HTML(
    <form id=form toolname="mytool" tooldescription="perform task">
      <input id=file1 name=file1 type=file>
    </form>
  )HTML");

  HTMLFormElement* form_element = GetFormElement("form");
  ASSERT_TRUE(form_element);
  ASSERT_TRUE(IsValidWebMCPForm(*form_element));

  String json_string =
      R"JSON(
        {
          "file1": "avatar.png"
        }
      )JSON";

  // A relative path is not allowed
  EXPECT_FALSE(FillFormControls(*form_element, json_string));
}

}  // namespace blink
