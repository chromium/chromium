// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/form_mcp_schema.h"

#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "third_party/blink/public/mojom/forms/form_control_type.mojom-blink-forward.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/live_node_list.h"
#include "third_party/blink/renderer/core/html/custom/custom_element.h"
#include "third_party/blink/renderer/core/html/custom/element_internals.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_label_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/forms/listed_element.h"
#include "third_party/blink/renderer/core/html/forms/step_range.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/json/json_parser.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

bool ToString(const JSONValue& value, String& out) {
  if (value.AsString(&out)) {
    return true;
  }
  int i;
  if (value.AsInteger(&i)) {
    out = String::Number(i);
    return true;
  }
  double d;
  if (value.AsDouble(&d)) {
    out = String::Number(d);
    return true;
  }
  bool b;
  if (value.AsBoolean(&b)) {
    out = b ? "true" : "false";
    return true;
  }
  return false;
}

bool ToBoolean(const JSONValue& value, bool& out) {
  if (value.AsBoolean(&out)) {
    return true;
  }
  int i;
  if (value.AsInteger(&i)) {
    out = (i != 0);
    return true;
  }
  String s;
  if (value.AsString(&s)) {
    if (EqualIgnoringAsciiCase(s, "true") || s == "1") {
      out = true;
      return true;
    }
    if (EqualIgnoringAsciiCase(s, "false") || s == "0") {
      out = false;
      return true;
    }
  }
  return false;
}

}  // namespace

using mojom::blink::FormControlType;

FormMCPSchema::FormMCPSchema(HTMLFormElement& form) {
  ProcessForm(form);
}

std::unique_ptr<JSONObject> FormMCPSchema::ComputeJSON() {
  auto out = std::make_unique<JSONObject>();
  out->SetString("type", "object");

  auto required = std::make_unique<JSONArray>();
  auto properties = std::make_unique<JSONObject>();

  // We must emit parameters in the same order they (first) appear
  // in the ListedElements() traversal.
  for (const String& name : ordered_names_) {
    bool is_required = false;
    // Note that a nullptr from ComputeParameterSchema() means the parameter
    // is not supported, for whatever reason.
    if (std::unique_ptr<JSONObject> parameter_schema =
            ComputeParameterSchema(name, is_required)) {
      properties->SetObject(name, std::move(parameter_schema));
      if (is_required) {
        required->PushString(name);
      }
    }
  }

  out->SetObject("properties", std::move(properties));
  out->SetArray("required", std::move(required));

  return out;
}

std::optional<WebDocument::ScriptToolError> FormMCPSchema::FillData(
    const JSONObject& json_obj) {
  // If the data isn't valid, form control states remain unchanged.
  for (wtf_size_t i = 0; i < json_obj.size(); ++i) {
    JSONObject::Entry entry = json_obj.at(i);
    const String parameter_name = String(entry.first);
    auto it = name_to_controls_.find(parameter_name);
    if (it == name_to_controls_.end()) {
      return WebDocument::ScriptToolError(
          WebDocument::ScriptToolError::kInvalidInputArguments,
          String("Input contains a parameter \"" + parameter_name +
                 "\" but there is no such parameter for the tool"));
    }
    if (!ValidateParameterData(parameter_name, *entry.second)) {
      String string_value;
      if (ToString(*entry.second, string_value)) {
        return WebDocument::ScriptToolError(
            WebDocument::ScriptToolError::kInvalidInputArguments,
            String("Invalid value \"" + string_value + "\" for parameter " +
                   parameter_name));
      }
      return WebDocument::ScriptToolError(
          WebDocument::ScriptToolError::kInvalidInputArguments,
          String("Invalid value for parameter " + parameter_name));
    }
  }

  // Now apply the values. This step must not fail.
  for (wtf_size_t i = 0; i < json_obj.size(); ++i) {
    JSONObject::Entry entry = json_obj.at(i);
    const String parameter_name = String(entry.first);
    blink::JSONValue* contents = entry.second;
    CHECK(contents);
    FillParameterData(parameter_name, *contents);
  }

  return std::nullopt;
}

bool FormMCPSchema::ValidateParameterData(const String& name,
                                          const JSONValue& value) {
  auto it = name_to_controls_.find(name);
  CHECK_NE(it, name_to_controls_.end());
  ControlVector* controls_for_name = it->value;
  CHECK(controls_for_name);

  if (IsText(*controls_for_name)) {
    return ValidateTextData(*controls_for_name, value);
  }
  if (IsDate(*controls_for_name)) {
    return ValidateTextData(*controls_for_name, value);
  }
  if (IsDatetimeLocal(*controls_for_name)) {
    return ValidateTextData(*controls_for_name, value);
  }
  if (IsMonth(*controls_for_name)) {
    return ValidateTextData(*controls_for_name, value);
  }
  if (IsWeek(*controls_for_name)) {
    return ValidateTextData(*controls_for_name, value);
  }
  if (IsTime(*controls_for_name)) {
    return ValidateTextData(*controls_for_name, value);
  }
  if (IsNumber(*controls_for_name)) {
    return ValidateNumberData(*controls_for_name, value);
  }
  if (IsSelect(*controls_for_name)) {
    return ValidateSelectData(*controls_for_name, value);
  }
  if (IsRange(*controls_for_name)) {
    return ValidateNumberData(*controls_for_name, value);
  }
  if (IsCheckbox(*controls_for_name)) {
    return ValidateCheckboxData(*controls_for_name, value);
  }
  if (IsRadio(*controls_for_name)) {
    return ValidateRadioData(*controls_for_name, value);
  }
  if (IsColor(*controls_for_name)) {
    return ValidateTextData(*controls_for_name, value);
  }
  if (IsCustomElement(*controls_for_name)) {
    // TODO(andruud): How to do validation for custom elements?
    return true;
  }
  if (IsFile(*controls_for_name)) {
    CHECK(RuntimeEnabledFeatures::WebMCPDeclarativeFileInputEnabled());
    return ValidateFileData(*controls_for_name, value);
  }

  return false;
}

bool FormMCPSchema::ValidateTextData(const ControlVector& controls_for_name,
                                     const JSONValue& value) {
  if (controls_for_name.size() != 1u) {
    return false;
  }
  String s;
  if (!ToString(value, s)) {
    return false;
  }
  if (s.empty()) {
    return true;
  }
  if (auto* input = DynamicTo<HTMLInputElement>(
          controls_for_name.front()->ToHTMLElement())) {
    return !input->SanitizeValue(s).empty();
  }
  return IsA<HTMLTextAreaElement>(controls_for_name.front()->ToHTMLElement());
}

bool FormMCPSchema::ValidateNumberData(const ControlVector& controls_for_name,
                                       const JSONValue& value) {
  if (controls_for_name.size() != 1u) {
    return false;
  }
  if (auto* input = DynamicTo<HTMLInputElement>(
          controls_for_name.front()->ToHTMLElement())) {
    String number_string;
    if (ToString(value, number_string)) {
      return !number_string.empty() &&
             !input->SanitizeValue(number_string).empty();
    }
  }
  return false;
}

bool FormMCPSchema::ValidateCheckboxData(const ControlVector& controls_for_name,
                                         const JSONValue& value) {
  // Single checkboxes are represented as a boolean in the schema.
  if (controls_for_name.size() == 1u) {
    bool unused;
    return ToBoolean(value, unused);
  }

  // Otherwise, a list of (unique) values.

  HashSet<String> allowed_values;
  for (ListedElement* control : controls_for_name) {
    HTMLInputElement& input = To<HTMLInputElement>(control->ToHTMLElement());
    allowed_values.insert(input.Value());
  }

  const JSONArray* array = JSONArray::Cast(&value);
  if (!array) {
    return false;
  }

  // Each value in the array must have a corresponding form control.
  for (const JSONValue& item : *array) {
    String s;
    if (!ToString(item, s)) {
      return false;
    }
    if (!allowed_values.Contains(s)) {
      return false;
    }
    // Specified values must be unique.
    allowed_values.erase(s);
  }

  return true;
}

bool FormMCPSchema::ValidateRadioData(const ControlVector& controls_for_name,
                                      const JSONValue& value) {
  String string;
  if (!ToString(value, string)) {
    return false;
  }
  // Make sure the provided value matches one of the options.
  for (ListedElement* control : controls_for_name) {
    HTMLInputElement& input = To<HTMLInputElement>(control->ToHTMLElement());
    if (input.Value() == string) {
      return true;
    }
  }
  return false;
}

bool FormMCPSchema::ValidateSelectData(const ControlVector& controls_for_name,
                                       const JSONValue& value) {
  auto* element =
      DynamicTo<HTMLSelectElement>(controls_for_name.front()->ToHTMLElement());
  if (!element) {
    return false;
  }

  HashSet<String> allowed_values;
  for (HTMLOptionElement& option : element->GetOptionList()) {
    allowed_values.insert(option.value());
  }

  if (!element->IsMultiple()) {
    String s;
    return ToString(value, s) && allowed_values.Contains(s);
  }

  const JSONArray* array = JSONArray::Cast(&value);
  if (!array) {
    return false;
  }

  // Each value in the array must have a corresponding option.
  for (const JSONValue& item : *array) {
    String s;
    if (!ToString(item, s)) {
      return false;
    }
    if (!allowed_values.Contains(s)) {
      return false;
    }
    // Specified values must be unique.
    allowed_values.erase(s);
  }

  return true;
}

bool FormMCPSchema::ValidateFileData(const ControlVector& controls_for_name,
                                     const JSONValue& value) {
  CHECK(RuntimeEnabledFeatures::WebMCPDeclarativeFileInputEnabled());
  if (controls_for_name.size() != 1u) {
    return false;
  }
  auto& input =
      To<HTMLInputElement>(controls_for_name.front()->ToHTMLElement());
  auto is_absolute_path_string = [](const JSONValue& value) -> bool {
    String path_string;
    if (ToString(value, path_string)) {
      return StringToFilePath(path_string).IsAbsolute();
    }
    return false;
  };

  if (input.Multiple()) {
    const JSONArray* array = JSONArray::Cast(&value);
    if (!array) {
      return false;
    }
    for (const JSONValue& item : *array) {
      if (!is_absolute_path_string(item)) {
        return false;
      }
    }
    return true;
  }
  return is_absolute_path_string(value);
}

void FormMCPSchema::FillParameterData(const String& name,
                                      const JSONValue& value) {
  auto it = name_to_controls_.find(name);
  CHECK_NE(it, name_to_controls_.end());
  ControlVector* controls_for_name = it->value;
  CHECK(controls_for_name);

  if (IsText(*controls_for_name)) {
    FillTextData(*controls_for_name, value);
  } else if (IsDate(*controls_for_name)) {
    FillTextData(*controls_for_name, value);
  } else if (IsDatetimeLocal(*controls_for_name)) {
    FillTextData(*controls_for_name, value);
  } else if (IsMonth(*controls_for_name)) {
    FillTextData(*controls_for_name, value);
  } else if (IsWeek(*controls_for_name)) {
    FillTextData(*controls_for_name, value);
  } else if (IsTime(*controls_for_name)) {
    FillTextData(*controls_for_name, value);
  } else if (IsNumber(*controls_for_name)) {
    FillNumberData(*controls_for_name, value);
  } else if (IsSelect(*controls_for_name)) {
    FillSelectData(*controls_for_name, value);
  } else if (IsRange(*controls_for_name)) {
    FillNumberData(*controls_for_name, value);
  } else if (IsCheckbox(*controls_for_name)) {
    FillCheckboxData(*controls_for_name, value);
  } else if (IsRadio(*controls_for_name)) {
    FillRadioData(*controls_for_name, value);
  } else if (IsColor(*controls_for_name)) {
    FillTextData(*controls_for_name, value);
  } else if (IsCustomElement(*controls_for_name)) {
    FillCustomElementData(*controls_for_name, value);
  } else if (IsFile(*controls_for_name)) {
    CHECK(RuntimeEnabledFeatures::WebMCPDeclarativeFileInputEnabled());
    FillFileData(*controls_for_name, value);
  }
}

std::unique_ptr<JSONObject> FormMCPSchema::ComputeParameterSchema(
    const String& name,
    bool& required) {
  auto it = name_to_controls_.find(name);
  if (it == name_to_controls_.end()) {
    // Already added.
    return nullptr;
  }
  ControlVector* controls_for_name = it->value;
  CHECK(controls_for_name);

  name_to_controls_.erase(name);  // Emit this parameter once.

  if (IsText(*controls_for_name)) {
    return ComputeTextParameterSchema(*controls_for_name, required);
  }
  if (IsDate(*controls_for_name)) {
    return ComputeDateParameterSchema(*controls_for_name, required);
  }
  if (IsDatetimeLocal(*controls_for_name)) {
    return ComputeDatetimeLocalParameterSchema(*controls_for_name, required);
  }
  if (IsMonth(*controls_for_name)) {
    return ComputeMonthParameterSchema(*controls_for_name, required);
  }
  if (IsWeek(*controls_for_name)) {
    return ComputeWeekParameterSchema(*controls_for_name, required);
  }
  if (IsTime(*controls_for_name)) {
    return ComputeTimeParameterSchema(*controls_for_name, required);
  }
  if (IsNumber(*controls_for_name)) {
    return ComputeNumberParameterSchema(*controls_for_name, required);
  }
  if (IsSelect(*controls_for_name)) {
    return ComputeSelectParameterSchema(*controls_for_name, required);
  }
  if (IsRange(*controls_for_name)) {
    return ComputeRangeParameterSchema(*controls_for_name, required);
  }
  if (IsCheckbox(*controls_for_name)) {
    return ComputeCheckboxParameterSchema(*controls_for_name, required);
  }
  if (IsRadio(*controls_for_name)) {
    return ComputeRadioParameterSchema(*controls_for_name, required);
  }
  if (IsColor(*controls_for_name)) {
    return ComputeColorParameterSchema(*controls_for_name, required);
  }
  if (IsCustomElement(*controls_for_name)) {
    return ComputeCustomElementParameterSchema(*controls_for_name, required);
  }
  if (IsFile(*controls_for_name)) {
    CHECK(RuntimeEnabledFeatures::WebMCPDeclarativeFileInputEnabled());
    return ComputeFileParameterSchema(*controls_for_name, required);
  }

  return nullptr;
}

std::unique_ptr<JSONObject> FormMCPSchema::ComputeTextParameterSchema(
    const ControlVector& controls_for_name,
    bool& required) {
  CHECK(IsText(controls_for_name));
  // Note that this function is used for both <input type=text> and <textarea>.
  auto& element =
      To<HTMLFormControlElement>(controls_for_name.front()->ToHTMLElement());
  auto schema = std::make_unique<JSONObject>();
  schema->SetString("type", "string");
  AddTitle(element, *schema);
  AddDescription(element, *schema);
  required = element.IsRequired();
  return schema;
}

std::unique_ptr<JSONObject> FormMCPSchema::ComputeDateParameterSchema(
    const ControlVector& controls_for_name,
    bool& required) {
  CHECK(IsDate(controls_for_name));
  auto& element =
      To<HTMLFormControlElement>(controls_for_name.front()->ToHTMLElement());
  auto schema = std::make_unique<JSONObject>();
  schema->SetString("type", "string");
  schema->SetString("format", "date");
  // Note that the "minimum" and "maximum" fields must contains numbers;
  // they cannot be used for dates.
  AddTitle(element, *schema);
  AddDescription(element, *schema,
                 "Dates MUST be provided in 'YYYY-MM-DD' format.");
  required = element.IsRequired();
  return schema;
}

std::unique_ptr<JSONObject> FormMCPSchema::ComputeDatetimeLocalParameterSchema(
    const ControlVector& controls_for_name,
    bool& required) {
  CHECK(IsDatetimeLocal(controls_for_name));
  auto& element =
      To<HTMLInputElement>(controls_for_name.front()->ToHTMLElement());
  auto schema = std::make_unique<JSONObject>();
  schema->SetString("type", "string");
  StepRange range = element.CreateStepRange(kAnyIsDefaultStep);
  // The format is "yyyy-MM-ddThh:mm" followed by optional ":ss" or ":ss.SSS".
  // The regex format is based on the valid time microsyntax in HTML:
  // https://html.spec.whatwg.org/multipage/common-microsyntaxes.html#local-dates-and-times
  // We cannot use the "date-time" type from json schema because that accepts
  // timezone which is not valid for <input type=datetime-local>.
  //
  // We vary the regexp according to 'step' to increase the likelihood of
  // correctly adhere to the step range, but full validation would be a lot more
  // complicated to express.
  if (range.Step() < 1000) {
    // Allow fractional seconds
    schema->SetString("format",
                      "^"
                      "[0-9]{4}-(0[1-9]|1[0-2])-[0-9]{2}"  // yyyy-MM-dd
                      "T([01][0-9]|2[0-3]):[0-5][0-9]"     // Thh:mm
                      "(:[0-5][0-9](\\.[0-9]{1,3})?)?"     // :ss (or :ss.SSS)
                      "$");
  } else if (range.Step() < 60000) {
    // Allow seconds
    schema->SetString("format",
                      "^"
                      "[0-9]{4}-(0[1-9]|1[0-2])-[0-9]{2}"  // yyyy-MM-dd
                      "T([01][0-9]|2[0-3]):[0-5][0-9]"     // Thh:mm
                      "(:[0-5][0-9])?"                     // :ss
                      "$");
  } else {
    // Allow hh:mm only
    schema->SetString("format",
                      "^"
                      "[0-9]{4}-(0[1-9]|1[0-2])-[0-9]{2}"  // yyyy-MM-dd
                      "T([01][0-9]|2[0-3]):[0-5][0-9]"     // Thh:mm
                      "$");
  }
  AddTitle(element, *schema);
  AddDescription(element, *schema);
  required = element.IsRequired();
  return schema;
}

std::unique_ptr<JSONObject> FormMCPSchema::ComputeMonthParameterSchema(
    const ControlVector& controls_for_name,
    bool& required) {
  CHECK(IsMonth(controls_for_name));
  auto& element = To<HTMLFormControlElement>(*controls_for_name.front().Get());
  auto schema = std::make_unique<JSONObject>();
  schema->SetString("type", "string");
  // The format is "yyyy-MM".
  // The regex format is based on the valid time microsyntax in HTML:
  // https://html.spec.whatwg.org/multipage/common-microsyntaxes.html#months
  schema->SetString("format", "^[0-9]{4}-(0[1-9]|1[0-2])$");
  AddTitle(element, *schema);
  AddDescription(element, *schema);
  required = element.IsRequired();
  return schema;
}

std::unique_ptr<JSONObject> FormMCPSchema::ComputeWeekParameterSchema(
    const ControlVector& controls_for_name,
    bool& required) {
  CHECK(IsWeek(controls_for_name));
  auto& element = To<HTMLFormControlElement>(*controls_for_name.front().Get());
  auto schema = std::make_unique<JSONObject>();
  schema->SetString("type", "string");
  // The format is "yyyy-Www".
  // The regex format is based on the valid time microsyntax in HTML:
  // https://html.spec.whatwg.org/multipage/common-microsyntaxes.html#weeks
  schema->SetString("format", "^[0-9]{4}-W(0[1-9]|[1-4][0-9]|5[0-3])$");
  AddTitle(element, *schema);
  AddDescription(element, *schema);
  required = element.IsRequired();
  return schema;
}

std::unique_ptr<JSONObject> FormMCPSchema::ComputeTimeParameterSchema(
    const ControlVector& controls_for_name,
    bool& required) {
  CHECK(IsTime(controls_for_name));
  auto& element =
      To<HTMLInputElement>(controls_for_name.front()->ToHTMLElement());
  auto schema = std::make_unique<JSONObject>();
  schema->SetString("type", "string");
  StepRange range = element.CreateStepRange(kAnyIsDefaultStep);
  // The format is "HH:mm", "HH:mm:ss" or "HH:mm:ss.SSS".
  // The regex format is based on the valid time microsyntax in HTML:
  // https://html.spec.whatwg.org/multipage/common-microsyntaxes.html#times
  // We cannot use the "time" type from json schema because that accepts
  // timezone which is not valid for <input type=time>.
  //
  // We vary the regexp according to 'step' to increase the likelihood of
  // correctly adhere to the step range, but full validation would be a lot more
  // complicated to express.
  if (range.Step() < 1000) {
    // Allow fractional seconds
    schema->SetString(
        "format",
        "^([01][0-9]|2[0-3]):[0-5][0-9](:[0-5][0-9](\\.[0-9]{1,3})?)?$");
  } else if (range.Step() < 60000) {
    // Allow seconds
    schema->SetString("format",
                      "^([01][0-9]|2[0-3]):[0-5][0-9](:[0-5][0-9])?$");
  } else {
    // Allow HH:MM only
    schema->SetString("format", "^([01][0-9]|2[0-3]):[0-5][0-9]$");
  }
  AddTitle(element, *schema);
  AddDescription(element, *schema);
  required = element.IsRequired();
  return schema;
}

std::unique_ptr<JSONObject> FormMCPSchema::ComputeNumberParameterSchema(
    const ControlVector& controls_for_name,
    bool& required) {
  CHECK(IsNumber(controls_for_name));
  auto& element =
      To<HTMLInputElement>(controls_for_name.front()->ToHTMLElement());
  auto schema = std::make_unique<JSONObject>();
  // TODO(crbug.com/475972617): Consider type:integer for matching StepRanges?
  schema->SetString("type", "number");
  StepRange step_range = element.CreateStepRange(kRejectAny);
  if (step_range.HasMin()) {
    schema->SetDouble("minimum", step_range.Minimum().ToDouble());
  }
  if (step_range.HasMax()) {
    schema->SetDouble("maximum", step_range.Maximum().ToDouble());
  }
  if (step_range.HasStep()) {
    Decimal step = step_range.Step();
    if (step_range.StepBase().Remainder(step).IsZero()) {
      // The valid values can be constrained by multipleOf, but only if the step
      // base is also a multiple of the step.
      schema->SetDouble("multipleOf", step.ToDouble());
    }
  }
  AddTitle(element, *schema);
  AddDescription(element, *schema);
  required = element.IsRequired();
  return schema;
}

std::unique_ptr<JSONObject> FormMCPSchema::ComputeSelectParameterSchema(
    const ControlVector& controls_for_name,
    bool& required) {
  CHECK(IsSelect(controls_for_name));
  auto& element =
      To<HTMLSelectElement>(controls_for_name.front()->ToHTMLElement());
  auto schema = std::make_unique<JSONObject>();

  auto one_of = std::make_unique<JSONArray>();
  auto enum_array = std::make_unique<JSONArray>();
  for (HTMLOptionElement& option : element.GetOptionList()) {
    auto option_object = std::make_unique<JSONObject>();
    option_object->SetString("const", option.value());
    option_object->SetString("title", option.textContent());
    one_of->PushObject(std::move(option_object));
    enum_array->PushString(option.value());
  }

  if (element.IsMultiple()) {
    schema->SetString("type", "array");
    auto items_schema = std::make_unique<JSONObject>();
    items_schema->SetString("type", "string");
    items_schema->SetArray("oneOf", std::move(one_of));
    items_schema->SetArray("enum", std::move(enum_array));
    schema->SetObject("items", std::move(items_schema));
    schema->SetBoolean("uniqueItems", true);
  } else {
    schema->SetString("type", "string");
    schema->SetArray("oneOf", std::move(one_of));
    schema->SetArray("enum", std::move(enum_array));
  }

  AddTitle(element, *schema);
  AddDescription(element, *schema);

  required = element.IsRequired();

  return schema;
}

std::unique_ptr<JSONObject> FormMCPSchema::ComputeRangeParameterSchema(
    const ControlVector& controls_for_name,
    bool& required) {
  CHECK(IsRange(controls_for_name));
  auto& element =
      To<HTMLInputElement>(controls_for_name.front()->ToHTMLElement());
  auto schema = std::make_unique<JSONObject>();
  schema->SetString("type", "number");
  StepRange step_range = element.CreateStepRange(kRejectAny);
  // Range input types always have a minimum and maximum, either from
  // the attributes or from the default values (0 and 100, respectively).
  schema->SetDouble("minimum", step_range.Minimum().ToDouble());
  schema->SetDouble("maximum", step_range.Maximum().ToDouble());
  // Range input types always have a step (default: 1).
  // This corresponds to multipleOf only when the step base is also
  // a multiple of the step.
  Decimal step = step_range.Step();
  if (step_range.StepBase().Remainder(step).IsZero()) {
    schema->SetDouble("multipleOf", step.ToDouble());
  }
  AddTitle(element, *schema);
  AddDescription(element, *schema);
  required = element.IsRequired();
  return schema;
}

std::unique_ptr<JSONObject> FormMCPSchema::ComputeCheckboxParameterSchema(
    const ControlVector& controls_for_name,
    bool& required) {
  CHECK(IsCheckbox(controls_for_name));
  if (controls_for_name.size() == 1u) {
    auto schema = std::make_unique<JSONObject>();
    auto& control =
        To<HTMLFormControlElement>(controls_for_name.front()->ToHTMLElement());
    schema->SetString("type", "boolean");
    required = control.IsRequired();
    return schema;
  }

  // There are multiple checkboxes with the same name. In this case,
  // we accept an array of values, each item corresponding to some
  // checkbox value. (Noting that "value" refers to the "value" attribute,
  // not the checked state of the checkbox.)

  CHECK_GT(controls_for_name.size(), 1u);

  auto schema = std::make_unique<JSONObject>();
  schema->SetString("type", "array");

  // Restrict each item in the list to one of the checkbox values.
  auto items_schema = std::make_unique<JSONObject>();
  items_schema->SetString("type", "string");
  std::unique_ptr<JSONArray> enum_array;
  items_schema->SetArray(
      "oneOf", ComputeOneOfArray(controls_for_name, enum_array, required));
  items_schema->SetArray("enum", std::move(enum_array));
  // Each checkbox value must at most appear *once* in the input.

  schema->SetObject("items", std::move(items_schema));
  schema->SetBoolean("uniqueItems", true);

  // Add title/description from the first control for now.
  AddTitleAndDescriptionFromToolAttributesOnly(*controls_for_name.front(),
                                               *schema);

  return schema;
}

std::unique_ptr<JSONObject> FormMCPSchema::ComputeRadioParameterSchema(
    const ControlVector& controls_for_name,
    bool& required) {
  CHECK(IsRadio(controls_for_name));
  auto schema = std::make_unique<JSONObject>();
  schema->SetString("type", "string");

  std::unique_ptr<JSONArray> enum_array;
  schema->SetArray("oneOf",
                   ComputeOneOfArray(controls_for_name, enum_array, required));
  schema->SetArray("enum", std::move(enum_array));
  // Add title/description from the first control for now.
  AddTitleAndDescriptionFromToolAttributesOnly(*controls_for_name.front(),
                                               *schema);
  return schema;
}

std::unique_ptr<JSONArray> FormMCPSchema::ComputeOneOfArray(
    const ControlVector& controls_for_name,
    std::unique_ptr<JSONArray>& enum_array,
    bool& required) {
  auto one_of = std::make_unique<JSONArray>();
  enum_array = std::make_unique<JSONArray>();
  for (ListedElement* control : controls_for_name) {
    HTMLInputElement& input = To<HTMLInputElement>(control->ToHTMLElement());
    auto checkbox_object = std::make_unique<JSONObject>();
    checkbox_object->SetString("const", input.Value());
    if (String title = LabelText(input); !title.empty()) {
      checkbox_object->SetString("title", title);
    }
    one_of->PushObject(std::move(checkbox_object));
    enum_array->PushString(input.Value());
    required |= input.IsRequired();
  }
  return one_of;
}

std::unique_ptr<JSONObject> FormMCPSchema::ComputeColorParameterSchema(
    const ControlVector& controls_for_name,
    bool& required) {
  CHECK(IsColor(controls_for_name));
  auto& element =
      To<HTMLFormControlElement>(controls_for_name.front()->ToHTMLElement());
  auto schema = std::make_unique<JSONObject>();
  schema->SetString("type", "string");
  // We only support setting color input values to a 6-digit hex rgba value, so
  // need to make sure that's the only color value syntax the agent uses.
  // TODO: With the runtime feature ColorInputAcceptsCSSColors enabled, we may
  // support more color syntaxes.
  schema->SetString("format", "^#[0-9a-zA-Z]{6}$");
  AddTitle(element, *schema);
  AddDescription(element, *schema);
  required = element.IsRequired();
  return schema;
}

std::unique_ptr<JSONObject> FormMCPSchema::ComputeCustomElementParameterSchema(
    const ControlVector& controls_for_name,
    bool& required) {
  CHECK(IsCustomElement(controls_for_name));
  auto& element_internals =
      To<ElementInternals>(*controls_for_name.front().Get());
  std::unique_ptr<JSONObject> schema =
      JSONObject::From(ParseJSON(element_internals.ToolParamSchema()));
  // Note that the above ParseJSON() call (and conversion to JSONObject)
  // is guaranteed to succeed by IsCustomElement().
  CHECK(schema);
  AddTitle(element_internals, *schema);
  AddDescription(element_internals, *schema);
  required = false;
  return schema;
}

std::unique_ptr<JSONObject> FormMCPSchema::ComputeFileParameterSchema(
    const ControlVector& controls_for_name,
    bool& required) {
  CHECK(RuntimeEnabledFeatures::WebMCPDeclarativeFileInputEnabled());
  HTMLInputElement& element =
      To<HTMLInputElement>(controls_for_name.front()->ToHTMLElement());
  auto schema = std::make_unique<JSONObject>();
  if (element.Multiple()) {
    schema->SetString("type", "array");
    auto items_object = std::make_unique<JSONObject>();
    items_object->SetString("type", "string");
    schema->SetObject("items", std::move(items_object));
  } else {
    schema->SetString("type", "string");
  }
  AddTitle(element, *schema);
  AddDescription(element, *schema);
  required = element.IsRequired();
  return schema;
}

// Note: Fill* functions may assume that the incoming value passed
// the corresponding Validate* function.

void FormMCPSchema::FillTextData(const ControlVector& controls_for_name,
                                 const JSONValue& value) {
  String string;
  if (!ToString(value, string)) {
    return;
  }
  if (auto* input = DynamicTo<HTMLInputElement>(
          controls_for_name.front()->ToHTMLElement())) {
    input->SetValue(string);
  } else if (auto* textarea = DynamicTo<HTMLTextAreaElement>(
                 controls_for_name.front()->ToHTMLElement())) {
    textarea->SetValue(string);
  }
}

void FormMCPSchema::FillNumberData(const ControlVector& controls_for_name,
                                   const JSONValue& value) {
  if (auto* input = DynamicTo<HTMLInputElement>(
          controls_for_name.front()->ToHTMLElement())) {
    String number_string;
    bool success = ToString(value, number_string);
    CHECK(success) << "ValidateNumberData should be called first";
    input->SetValue(number_string);
  }
}

void FormMCPSchema::FillCheckboxData(const ControlVector& controls_for_name,
                                     const JSONValue& value) {
  if (controls_for_name.size() == 1u) {
    bool checked;
    CHECK(ToBoolean(value, checked));
    To<HTMLInputElement>(controls_for_name.front()->ToHTMLElement())
        .SetChecked(checked);
    return;
  }

  CHECK(!controls_for_name.empty());

  // Values that are present in the array are checked; values that are not
  // are unchecked.

  const JSONArray* array = JSONArray::Cast(&value);
  CHECK(array);

  HashSet<String> checked_values;
  for (const JSONValue& item : *array) {
    String s;
    CHECK(ToString(item, s));
    checked_values.insert(s);
  }

  // Check (or uncheck) each value.
  for (ListedElement* control : controls_for_name) {
    HTMLInputElement& input = To<HTMLInputElement>(control->ToHTMLElement());
    input.SetChecked(checked_values.Contains(input.Value()));
  }
}

void FormMCPSchema::FillRadioData(const ControlVector& controls_for_name,
                                  const JSONValue& value) {
  String string;
  if (!ToString(value, string)) {
    return;
  }
  for (ListedElement* control : controls_for_name) {
    HTMLInputElement& input = To<HTMLInputElement>(control->ToHTMLElement());
    if (input.Value() == string) {
      input.SetChecked(true, TextFieldEventBehavior::kDispatchChangeEvent);
    }
  }
}

void FormMCPSchema::FillSelectData(const ControlVector& controls_for_name,
                                   const JSONValue& value) {
  HTMLSelectElement& select =
      To<HTMLSelectElement>(controls_for_name.front()->ToHTMLElement());

  if (!select.IsMultiple()) {
    String selected_value;
    CHECK(ToString(value, selected_value));
    select.SetValue(selected_value, /*send_events=*/true,
                    WebAutofillState::kNotFilled);
    return;
  }

  const JSONArray* array = JSONArray::Cast(&value);
  CHECK(array);

  HashSet<String> selected_values;
  for (const JSONValue& item : *array) {
    String s;
    CHECK(ToString(item, s));
    selected_values.insert(s);
  }

  Vector<int> selected_indices;
  int index = 0;
  for (HTMLOptionElement& option : select.GetOptionList()) {
    if (selected_values.Contains(option.value())) {
      selected_indices.push_back(index);
    }
    ++index;
  }

  select.SelectMultipleOptions(selected_indices);
}

void FormMCPSchema::FillCustomElementData(
    const ControlVector& controls_for_name,
    const JSONValue& value) {
  auto& element_internals =
      To<ElementInternals>(*controls_for_name.front().Get());
  CustomElement::EnqueueToolFillCallback(element_internals.Target(),
                                         value.ToJSONString());
}

void FormMCPSchema::FillFileData(const ControlVector& controls_for_name,
                                 const JSONValue& value) {
  // TODO(crbug.com/481211432): NEEDS PRIVACY REVIEW BEFORE SHIPPING
  CHECK(RuntimeEnabledFeatures::WebMCPDeclarativeFileInputEnabled());
  Vector<String> paths;
  auto& file_input =
      To<HTMLInputElement>(controls_for_name.front()->ToHTMLElement());
  if (file_input.Multiple()) {
    const JSONArray* array = JSONArray::Cast(&value);
    if (!array) {
      return;
    }
    for (const JSONValue& item : *array) {
      String path;
      if (!ToString(item, path)) {
        return;
      }
      paths.push_back(path);
    }
  } else {
    String path;
    if (!ToString(value, path)) {
      return;
    }
    paths.push_back(path);
  }
  file_input.SetFilesFromPaths(paths);
}

void FormMCPSchema::AddTitle(ListedElement& control, JSONObject& obj) {
  if (String title = ToolParamTitleAttribute(control); !title.empty()) {
    obj.SetString("title", title);
  }
}

void FormMCPSchema::AddDescription(ListedElement& control,
                                   JSONObject& obj,
                                   String extra_context) {
  String description = ComputeDescription(control);
  if (!extra_context.empty()) {
    if (!description.empty()) {
      description = description + " (" + extra_context + ")";
    } else {
      description = extra_context;
    }
  }
  if (!description.empty()) {
    obj.SetString("description", description);
  }
}

void FormMCPSchema::AddTitleAndDescriptionFromToolAttributesOnly(
    ListedElement& control,
    JSONObject& obj) {
  if (String title = ToolParamTitleAttribute(control); !title.empty()) {
    obj.SetString("title", title);
  }
  if (String description = ToolParamDescriptionAttribute(control);
      !description.empty()) {
    obj.SetString("description", description);
  }
}

String FormMCPSchema::ToolParamTitleAttribute(ListedElement& control) const {
  return control.ToHTMLElement().FastGetAttribute(
      html_names::kToolparamtitleAttr);
}

String FormMCPSchema::ToolParamDescriptionAttribute(
    ListedElement& control) const {
  return control.ToHTMLElement().FastGetAttribute(
      html_names::kToolparamdescriptionAttr);
}

String FormMCPSchema::ComputeDescription(ListedElement& control) {
  // Prefer 'toolparamdescription' when present.
  if (String description = ToolParamDescriptionAttribute(control);
      !description.empty()) {
    return description;
  }

  // Absent a 'toolparamdescription' attribute, use concatenated label text.
  if (String label_text = LabelText(control); !label_text.empty()) {
    return label_text;
  }

  // Last resort: aria-description.
  if (String description = control.ToHTMLElement().FastGetAttribute(
          html_names::kAriaDescriptionAttr);
      !description.empty()) {
    return description;
  }

  return g_null_atom;
}

String FormMCPSchema::LabelText(ListedElement& control) {
  if (LiveNodeList* list = control.ToHTMLElement().labels()) {
    StringBuilder builder;

    for (wtf_size_t i = 0; i < list->length(); ++i) {
      if (i != 0) {
        builder.Append("; ");
      }
      HTMLLabelElement& label = To<HTMLLabelElement>(*list->item(i));
      builder.Append(label.TextContentExcludingLabelable().StripWhiteSpace());
    }

    if (!builder.empty()) {
      return builder.ReleaseString();
    }
  }
  return g_null_atom;
}

void FormMCPSchema::ProcessForm(HTMLFormElement& form) {
  for (ListedElement* element : form.ListedElements()) {
    if (auto* form_control = DynamicTo<HTMLFormControlElement>(element)) {
      if (form_control->IsDisabledOrReadOnly()) {
        continue;
      }
      String name = form_control->GetWebMCPParameterName();
      EnsureControlVector(name).push_back(form_control);
      ordered_names_.push_back(name);
      if (form_control->IsSuccessfulSubmitButton() && !submit_button_) {
        submit_button_ = form_control;
      }
    } else if (auto* element_internals = DynamicTo<ElementInternals>(element)) {
      String name = element_internals->GetName();
      if (!name.empty()) {
        EnsureControlVector(name).push_back(element_internals);
        ordered_names_.push_back(name);
      }
    }
  }
}

FormMCPSchema::ControlVector& FormMCPSchema::EnsureControlVector(
    const String& name) {
  auto entry = name_to_controls_.insert(name, nullptr);
  if (!entry.stored_value->value) {
    entry.stored_value->value = MakeGarbageCollected<ControlVector>();
  }
  return *entry.stored_value->value;
}

bool FormMCPSchema::IsText(ListedElement& control) const {
  if (auto* input = DynamicTo<HTMLInputElement>(control.ToHTMLElement())) {
    switch (input->FormControlType()) {
      case FormControlType::kInputText:
      case FormControlType::kInputEmail:
      case FormControlType::kInputSearch:
      case FormControlType::kInputTelephone:
      case FormControlType::kInputUrl:
      case FormControlType::kInputPassword:
        return true;
      case FormControlType::kInputHidden:
        return !ToolParamTitleAttribute(control).empty() ||
               !ToolParamDescriptionAttribute(control).empty();
      default:
        break;
    }
  }
  return IsA<HTMLTextAreaElement>(control.ToHTMLElement());
}

bool FormMCPSchema::IsDate(ListedElement& control) const {
  auto* input = DynamicTo<HTMLInputElement>(control.ToHTMLElement());
  return input && input->FormControlType() == FormControlType::kInputDate;
}

bool FormMCPSchema::IsDatetimeLocal(ListedElement& control) const {
  auto* input = DynamicTo<HTMLInputElement>(control.ToHTMLElement());
  return input &&
         input->FormControlType() == FormControlType::kInputDatetimeLocal;
}

bool FormMCPSchema::IsMonth(ListedElement& control) const {
  auto* input = DynamicTo<HTMLInputElement>(control.ToHTMLElement());
  return input && input->FormControlType() == FormControlType::kInputMonth;
}

bool FormMCPSchema::IsWeek(ListedElement& control) const {
  auto* input = DynamicTo<HTMLInputElement>(control.ToHTMLElement());
  return input && input->FormControlType() == FormControlType::kInputWeek;
}

bool FormMCPSchema::IsTime(ListedElement& control) const {
  auto* input = DynamicTo<HTMLInputElement>(control.ToHTMLElement());
  return input && input->FormControlType() == FormControlType::kInputTime;
}

bool FormMCPSchema::IsNumber(ListedElement& control) const {
  auto* input = DynamicTo<HTMLInputElement>(control.ToHTMLElement());
  return input && input->FormControlType() == FormControlType::kInputNumber;
}

bool FormMCPSchema::IsSelect(ListedElement& control) const {
  return IsA<HTMLSelectElement>(control.ToHTMLElement());
}

bool FormMCPSchema::IsRange(ListedElement& control) const {
  auto* input = DynamicTo<HTMLInputElement>(control.ToHTMLElement());
  return input && input->FormControlType() == FormControlType::kInputRange;
}

bool FormMCPSchema::IsCheckbox(ListedElement& control) const {
  auto* input = DynamicTo<HTMLInputElement>(control.ToHTMLElement());
  return input && input->FormControlType() == FormControlType::kInputCheckbox;
}

bool FormMCPSchema::IsRadio(ListedElement& control) const {
  auto* input = DynamicTo<HTMLInputElement>(control.ToHTMLElement());
  return input && input->FormControlType() == FormControlType::kInputRadio;
}

bool FormMCPSchema::IsColor(ListedElement& control) const {
  auto* input = DynamicTo<HTMLInputElement>(control.ToHTMLElement());
  return input && input->FormControlType() == FormControlType::kInputColor;
}

bool FormMCPSchema::IsCustomElement(ListedElement& control) const {
  auto* element_internals = DynamicTo<ElementInternals>(control);
  if (!element_internals) {
    return false;
  }
  String schema_string = element_internals->ToolParamSchema();
  if (schema_string.empty()) {
    return false;
  }
  std::unique_ptr<JSONValue> json = ParseJSON(schema_string);
  return json && JSONObject::Cast(json.get());
}

bool FormMCPSchema::IsFile(ListedElement& control) const {
  if (!RuntimeEnabledFeatures::WebMCPDeclarativeFileInputEnabled()) {
    return false;
  }
  auto* input = DynamicTo<HTMLInputElement>(control.ToHTMLElement());
  return input && input->FormControlType() == FormControlType::kInputFile;
}

bool FormMCPSchema::IsText(const ControlVector& controls_for_name) const {
  return controls_for_name.size() == 1u && IsText(*controls_for_name.front());
}

bool FormMCPSchema::IsDate(const ControlVector& controls_for_name) const {
  return controls_for_name.size() == 1u && IsDate(*controls_for_name.front());
}

bool FormMCPSchema::IsDatetimeLocal(
    const ControlVector& controls_for_name) const {
  return controls_for_name.size() == 1u &&
         IsDatetimeLocal(*controls_for_name.front());
}

bool FormMCPSchema::IsMonth(const ControlVector& controls_for_name) const {
  return controls_for_name.size() == 1u && IsMonth(*controls_for_name.front());
}

bool FormMCPSchema::IsWeek(const ControlVector& controls_for_name) const {
  return controls_for_name.size() == 1u && IsWeek(*controls_for_name.front());
}

bool FormMCPSchema::IsTime(const ControlVector& controls_for_name) const {
  return controls_for_name.size() == 1u && IsTime(*controls_for_name.front());
}

bool FormMCPSchema::IsNumber(const ControlVector& controls_for_name) const {
  return controls_for_name.size() == 1u && IsNumber(*controls_for_name.front());
}

bool FormMCPSchema::IsSelect(const ControlVector& controls_for_name) const {
  return controls_for_name.size() == 1u && IsSelect(*controls_for_name.front());
}

bool FormMCPSchema::IsRange(const ControlVector& controls_for_name) const {
  return controls_for_name.size() == 1u && IsRange(*controls_for_name.front());
}

bool FormMCPSchema::IsCheckbox(const ControlVector& controls_for_name) const {
  CHECK(!controls_for_name.empty());
  for (ListedElement* control : controls_for_name) {
    if (!IsCheckbox(*control)) {
      return false;
    }
  }
  return true;
}

bool FormMCPSchema::IsRadio(const ControlVector& controls_for_name) const {
  CHECK(!controls_for_name.empty());
  for (ListedElement* control : controls_for_name) {
    if (!IsRadio(*control)) {
      return false;
    }
  }
  return true;
}

bool FormMCPSchema::IsColor(const ControlVector& controls_for_name) const {
  return controls_for_name.size() == 1u && IsColor(*controls_for_name.front());
}

bool FormMCPSchema::IsCustomElement(
    const ControlVector& controls_for_name) const {
  return controls_for_name.size() == 1u &&
         IsCustomElement(*controls_for_name.front());
}

bool FormMCPSchema::IsFile(const ControlVector& controls_for_name) const {
  return RuntimeEnabledFeatures::WebMCPDeclarativeFileInputEnabled() &&
         controls_for_name.size() == 1u && IsFile(*controls_for_name.front());
}

}  // namespace blink
