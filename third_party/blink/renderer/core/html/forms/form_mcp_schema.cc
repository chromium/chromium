// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/form_mcp_schema.h"

#include <optional>

#include "third_party/blink/public/mojom/forms/form_control_type.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/live_node_list.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/forms/listed_element.h"
#include "third_party/blink/renderer/core/html/forms/step_range.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

String ToNumberString(const JSONValue& value) {
  String string;
  int i;
  double d;
  if (value.AsInteger(&i)) {
    return String::Number(i);
  }
  if (value.AsDouble(&d)) {
    return String::Number(d);
  }
  return g_null_atom;
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
      return WebDocument::ScriptToolError::kInvalidInputArguments;
    }
    if (!ValidateParameterData(parameter_name, *entry.second)) {
      return WebDocument::ScriptToolError::kInvalidInputArguments;
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

  return false;
}

bool FormMCPSchema::ValidateTextData(const ControlVector& controls_for_name,
                                     const JSONValue& value) {
  if (controls_for_name.size() != 1u) {
    return false;
  }
  String s;
  if (!value.AsString(&s)) {
    return false;
  }
  if (s.empty()) {
    return true;
  }
  if (auto* input =
          DynamicTo<HTMLInputElement>(controls_for_name.front().Get())) {
    return !input->SanitizeValue(s).empty();
  }
  return IsA<HTMLTextAreaElement>(controls_for_name.front().Get());
}

bool FormMCPSchema::ValidateNumberData(const ControlVector& controls_for_name,
                                       const JSONValue& value) {
  if (controls_for_name.size() != 1u) {
    return false;
  }
  if (auto* input =
          DynamicTo<HTMLInputElement>(controls_for_name.front().Get())) {
    String number_string = ToNumberString(value);
    return !number_string.empty() && input->SanitizeValue(number_string);
  }
  return false;
}

bool FormMCPSchema::ValidateCheckboxData(const ControlVector& controls_for_name,
                                         const JSONValue& value) {
  bool unused;
  return controls_for_name.size() == 1u && value.AsBoolean(&unused);
}

bool FormMCPSchema::ValidateRadioData(const ControlVector& controls_for_name,
                                      const JSONValue& value) {
  String string;
  if (!value.AsString(&string)) {
    return false;
  }
  // Make sure the provided value matches one of the options.
  for (HTMLFormControlElement* control : controls_for_name) {
    HTMLInputElement& input = To<HTMLInputElement>(*control);
    if (input.Value() == string) {
      return true;
    }
  }
  return false;
}

bool FormMCPSchema::ValidateSelectData(const ControlVector& controls_for_name,
                                       const JSONValue& value) {
  // TODO(crbug.com/480888831): Check that it's one of the options.
  String unused;
  return controls_for_name.size() == 1u && value.AsString(&unused);
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

  return nullptr;
}

std::unique_ptr<JSONObject> FormMCPSchema::ComputeTextParameterSchema(
    const ControlVector& controls_for_name,
    bool& required) {
  // Note that this function is used for both <input type=text> and <textarea>.
  HTMLFormControlElement* element = controls_for_name.front().Get();
  CHECK(element);
  auto schema = std::make_unique<JSONObject>();
  schema->SetString("type", "string");
  AddTitle(*element, *schema);
  AddDescription(*element, *schema);
  required = element->IsRequired();
  return schema;
}

std::unique_ptr<JSONObject> FormMCPSchema::ComputeDateParameterSchema(
    const ControlVector& controls_for_name,
    bool& required) {
  HTMLFormControlElement* element = controls_for_name.front().Get();
  CHECK(element);
  auto schema = std::make_unique<JSONObject>();
  schema->SetString("type", "string");
  schema->SetString("format", "date");
  // Note that the "minimum" and "maximum" fields must contains numbers;
  // they cannot be used for dates.
  AddTitle(*element, *schema);
  AddDescription(*element, *schema);
  required = element->IsRequired();
  return schema;
}

std::unique_ptr<JSONObject> FormMCPSchema::ComputeTimeParameterSchema(
    const ControlVector& controls_for_name,
    bool& required) {
  HTMLFormControlElement* element = controls_for_name.front().Get();
  CHECK(element);
  auto schema = std::make_unique<JSONObject>();
  schema->SetString("type", "string");
  // The regex format is based on the valid time microsyntax in HTML:
  // https://html.spec.whatwg.org/multipage/common-microsyntaxes.html#times
  // We cannot use the "time" type from json schema because that accepts
  // timezone which is not valid for <input type=time>.
  schema->SetString(
      "format",
      "^([01][0-9]|2[0-3]):[0-5][0-9](:[0-5][0-9](\\.[0-9]{1,3})?)?$");
  AddTitle(*element, *schema);
  AddDescription(*element, *schema);
  required = element->IsRequired();
  return schema;
}

std::unique_ptr<JSONObject> FormMCPSchema::ComputeNumberParameterSchema(
    const ControlVector& controls_for_name,
    bool& required) {
  auto* element = DynamicTo<HTMLInputElement>(controls_for_name.front().Get());
  if (!element) {
    return nullptr;
  }

  auto schema = std::make_unique<JSONObject>();
  // TODO(crbug.com/475972617): Consider type:integer for matching StepRanges?
  schema->SetString("type", "number");
  StepRange step_range = element->CreateStepRange(kRejectAny);
  if (step_range.HasMin()) {
    schema->SetDouble("minimum", step_range.Minimum().ToDouble());
  }
  if (step_range.HasMax()) {
    schema->SetDouble("maximum", step_range.Maximum().ToDouble());
  }
  AddTitle(*element, *schema);
  AddDescription(*element, *schema);
  required = element->IsRequired();
  // TODO(crbug.com/475972617): Add multipleOf?
  return schema;
}

std::unique_ptr<JSONObject> FormMCPSchema::ComputeSelectParameterSchema(
    const ControlVector& controls_for_name,
    bool& required) {
  auto* element = DynamicTo<HTMLSelectElement>(controls_for_name.front().Get());
  if (!element) {
    return nullptr;
  }

  auto schema = std::make_unique<JSONObject>();
  schema->SetString("type", "string");

  auto one_of = std::make_unique<JSONArray>();
  for (HTMLOptionElement& option : element->GetOptionList()) {
    auto option_object = std::make_unique<JSONObject>();
    option_object->SetString("const", option.value());
    option_object->SetString("title", option.textContent());
    one_of->PushObject(std::move(option_object));
  }
  schema->SetArray("oneOf", std::move(one_of));

  AddTitle(*element, *schema);
  AddDescription(*element, *schema);

  required = element->IsRequired();

  return schema;
}

std::unique_ptr<JSONObject> FormMCPSchema::ComputeRangeParameterSchema(
    const ControlVector& controls_for_name,
    bool& required) {
  auto* element = DynamicTo<HTMLInputElement>(controls_for_name.front().Get());
  if (!element) {
    return nullptr;
  }
  auto schema = std::make_unique<JSONObject>();
  schema->SetString("type", "number");
  StepRange step_range = element->CreateStepRange(kRejectAny);
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
  AddTitle(*element, *schema);
  AddDescription(*element, *schema);
  required = element->IsRequired();
  return schema;
}

std::unique_ptr<JSONObject> FormMCPSchema::ComputeCheckboxParameterSchema(
    const ControlVector& controls_for_name,
    bool& required) {
  HTMLFormControlElement* element = controls_for_name.front().Get();
  CHECK(element);
  auto schema = std::make_unique<JSONObject>();
  schema->SetString("type", "boolean");
  AddTitle(*element, *schema);
  AddDescription(*element, *schema);
  required = element->IsRequired();
  return schema;
}

std::unique_ptr<JSONObject> FormMCPSchema::ComputeRadioParameterSchema(
    const ControlVector& controls_for_name,
    bool& required) {
  auto schema = std::make_unique<JSONObject>();
  schema->SetString("type", "string");

  auto one_of = std::make_unique<JSONArray>();
  for (HTMLFormControlElement* control : controls_for_name) {
    HTMLInputElement& radio = To<HTMLInputElement>(*control);
    auto radio_object = std::make_unique<JSONObject>();
    radio_object->SetString("const", radio.Value());
    if (String title = LabelText(radio); !title.empty()) {
      radio_object->SetString("title", title);
    }
    one_of->PushObject(std::move(radio_object));
    required |= radio.IsRequired();
  }
  schema->SetArray("oneOf", std::move(one_of));

  // It's not clear yet where to host the toolparamtitle/description
  // attributes for <input type=radio>. For now, we use the attributes
  // set on the first radio button of the group.
  //
  // https://github.com/webmachinelearning/webmcp/issues/71
  HTMLFormControlElement* first_radio = controls_for_name.front().Get();
  CHECK(first_radio);
  // Note that we should not use AddTitle()/AddDescription() here,
  // since we don't want e.g. a <label> for a specific radio button
  // to describe the parameter as a whole.
  if (String title = ToolParamTitleAttribute(*first_radio); !title.empty()) {
    schema->SetString("title", title);
  }
  if (String description = ToolParamDescriptionAttribute(*first_radio);
      !description.empty()) {
    schema->SetString("description", description);
  }
  return schema;
}

std::unique_ptr<JSONObject> FormMCPSchema::ComputeColorParameterSchema(
    const ControlVector& controls_for_name,
    bool& required) {
  HTMLFormControlElement* element = controls_for_name.front().Get();
  CHECK(element);
  auto schema = std::make_unique<JSONObject>();
  schema->SetString("type", "string");
  // We only support setting color input values to a 6-digit hex rgba value, so
  // need to make sure that's the only color value syntax the agent uses.
  // TODO: With the runtime feature ColorInputAcceptsCSSColors enabled, we may
  // support more color syntaxes.
  schema->SetString("format", "^#[0-9a-zA-Z]{6}$");
  AddTitle(*element, *schema);
  AddDescription(*element, *schema);
  required = element->IsRequired();
  return schema;
}

void FormMCPSchema::FillTextData(const ControlVector& controls_for_name,
                                 const JSONValue& value) {
  String string;
  if (!value.AsString(&string)) {
    return;
  }
  if (auto* input =
          DynamicTo<HTMLInputElement>(controls_for_name.front().Get())) {
    input->SetValue(string);
  } else if (auto* textarea = DynamicTo<HTMLTextAreaElement>(
                 controls_for_name.front().Get())) {
    textarea->SetValue(string);
  }
}

void FormMCPSchema::FillNumberData(const ControlVector& controls_for_name,
                                   const JSONValue& value) {
  if (auto* input =
          DynamicTo<HTMLInputElement>(controls_for_name.front().Get())) {
    input->SetValue(ToNumberString(value));
  }
}

void FormMCPSchema::FillCheckboxData(const ControlVector& controls_for_name,
                                     const JSONValue& value) {
  bool checked;
  if (!value.AsBoolean(&checked)) {
    return;
  }
  if (auto* input =
          DynamicTo<HTMLInputElement>(controls_for_name.front().Get())) {
    input->SetChecked(checked, TextFieldEventBehavior::kDispatchChangeEvent);
  }
}

void FormMCPSchema::FillRadioData(const ControlVector& controls_for_name,
                                  const JSONValue& value) {
  String string;
  if (!value.AsString(&string)) {
    return;
  }
  for (HTMLFormControlElement* control : controls_for_name) {
    HTMLInputElement& input = To<HTMLInputElement>(*control);
    if (input.Value() == string) {
      input.SetChecked(true, TextFieldEventBehavior::kDispatchChangeEvent);
    }
  }
}

void FormMCPSchema::FillSelectData(const ControlVector& controls_for_name,
                                   const JSONValue& value) {
  String selected_value;
  if (!value.AsString(&selected_value)) {
    return;
  }
  if (auto* select =
          DynamicTo<HTMLSelectElement>(controls_for_name.front().Get())) {
    select->SetValue(selected_value, /*send_events=*/true,
                     WebAutofillState::kNotFilled);
  }
}

void FormMCPSchema::AddTitle(HTMLFormControlElement& control, JSONObject& obj) {
  if (String title = ToolParamTitleAttribute(control); !title.empty()) {
    obj.SetString("title", title);
  }
}

void FormMCPSchema::AddDescription(HTMLFormControlElement& control,
                                   JSONObject& obj) {
  if (String description = ComputeDescription(control); !description.empty()) {
    obj.SetString("description", description);
  }
}

String FormMCPSchema::ToolParamTitleAttribute(HTMLFormControlElement& control) {
  return control.FastGetAttribute(html_names::kToolparamtitleAttr);
}

String FormMCPSchema::ToolParamDescriptionAttribute(
    HTMLFormControlElement& control) {
  return control.FastGetAttribute(html_names::kToolparamdescriptionAttr);
}

String FormMCPSchema::ComputeDescription(HTMLFormControlElement& control) {
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
  if (String description =
          control.FastGetAttribute(html_names::kAriaDescriptionAttr);
      !description.empty()) {
    return description;
  }

  return g_null_atom;
}

String FormMCPSchema::LabelText(HTMLFormControlElement& control) {
  if (LiveNodeList* list = control.labels()) {
    StringBuilder builder;

    for (wtf_size_t i = 0; i < list->length(); ++i) {
      Node* label = list->item(i);
      if (i != 0) {
        builder.Append("; ");
      }
      builder.Append(label->textContent().StripWhiteSpace());
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
      String name = form_control->GetWebMCPParameterName();
      EnsureControlVector(name).push_back(form_control);
      ordered_names_.push_back(name);
      if (form_control->IsSuccessfulSubmitButton() && !submit_button_) {
        submit_button_ = form_control;
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

bool FormMCPSchema::IsText(HTMLFormControlElement& control) const {
  if (auto* input = DynamicTo<HTMLInputElement>(control)) {
    switch (input->FormControlType()) {
      case FormControlType::kInputText:
      case FormControlType::kInputEmail:
      case FormControlType::kInputSearch:
      case FormControlType::kInputTelephone:
      case FormControlType::kInputUrl:
      case FormControlType::kInputPassword:
        return true;
      default:
        break;
    }
  }
  return IsA<HTMLTextAreaElement>(control);
}

bool FormMCPSchema::IsDate(HTMLFormControlElement& control) const {
  auto* input = DynamicTo<HTMLInputElement>(control);
  return input && input->FormControlType() == FormControlType::kInputDate;
}

bool FormMCPSchema::IsTime(HTMLFormControlElement& control) const {
  auto* input = DynamicTo<HTMLInputElement>(control);
  return input && input->FormControlType() == FormControlType::kInputTime;
}

bool FormMCPSchema::IsNumber(HTMLFormControlElement& control) const {
  auto* input = DynamicTo<HTMLInputElement>(control);
  return input && input->FormControlType() == FormControlType::kInputNumber;
}

bool FormMCPSchema::IsSelect(HTMLFormControlElement& control) const {
  return IsA<HTMLSelectElement>(control);
}

bool FormMCPSchema::IsRange(HTMLFormControlElement& control) const {
  auto* input = DynamicTo<HTMLInputElement>(control);
  return input && input->FormControlType() == FormControlType::kInputRange;
}

bool FormMCPSchema::IsCheckbox(HTMLFormControlElement& control) const {
  auto* input = DynamicTo<HTMLInputElement>(control);
  return input && input->FormControlType() == FormControlType::kInputCheckbox;
}

bool FormMCPSchema::IsRadio(HTMLFormControlElement& control) const {
  auto* input = DynamicTo<HTMLInputElement>(control);
  return input && input->FormControlType() == FormControlType::kInputRadio;
}

bool FormMCPSchema::IsColor(HTMLFormControlElement& control) const {
  auto* input = DynamicTo<HTMLInputElement>(control);
  return input && input->FormControlType() == FormControlType::kInputColor;
}

bool FormMCPSchema::IsText(const ControlVector& controls_for_name) const {
  return controls_for_name.size() == 1u && IsText(*controls_for_name.front());
}

bool FormMCPSchema::IsDate(const ControlVector& controls_for_name) const {
  return controls_for_name.size() == 1u && IsDate(*controls_for_name.front());
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
  return controls_for_name.size() == 1u &&
         IsCheckbox(*controls_for_name.front());
}

bool FormMCPSchema::IsRadio(const ControlVector& controls_for_name) const {
  CHECK(!controls_for_name.empty());
  for (HTMLFormControlElement* control : controls_for_name) {
    if (!IsRadio(*control)) {
      return false;
    }
  }
  return true;
}

bool FormMCPSchema::IsColor(const ControlVector& controls_for_name) const {
  return controls_for_name.size() == 1u && IsColor(*controls_for_name.front());
}

}  // namespace blink
