// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_FORM_MCP_SCHEMA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_FORM_MCP_SCHEMA_H_

#include <optional>

#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class HTMLFormElement;
class HTMLFormControlElement;

// Represents the JSON Schema for some <form> element.
//
// This class can used to produce an input parameter schema
// for declarative WebMCP tools, as well as fill out the associated
// form based on an incoming JSON object.
class CORE_EXPORT FormMCPSchema {
  STACK_ALLOCATED();
  // Form controls may be named, but there is not a 1:1 relationship
  // between names and form controls; we keep track of all form controls
  // associated with a given name. (See name_to_controls_.)
  using ControlVector = GCedHeapVector<Member<HTMLFormControlElement>>;

 public:
  explicit FormMCPSchema(HTMLFormElement&);

  // Compute an input schema, suitable for DeclarativeWebMCPTool::
  // ComputeInputSchema().
  std::unique_ptr<JSONObject> ComputeJSON();

  // Fill out the form using the provided JSON object.
  //
  // Returns std::nullopt on success, or some error if the provided
  // object did not match the structure of the form. In the error case,
  // all form control states are left unchanged.
  std::optional<WebDocument::ScriptToolError> FillData(const JSONObject&);

  // The first successful submit button (IsSuccessfulSubmitButton())
  // found within the form.
  HTMLFormControlElement* SubmitButton() const { return submit_button_; }

 private:
  std::unique_ptr<JSONObject> ComputeParameterSchema(const String& name,
                                                     bool& required);
  std::unique_ptr<JSONObject> ComputeTextParameterSchema(
      const ControlVector& controls_for_name,
      bool& required);
  std::unique_ptr<JSONObject> ComputeDateParameterSchema(
      const ControlVector& controls_for_name,
      bool& required);
  std::unique_ptr<JSONObject> ComputeTimeParameterSchema(
      const ControlVector& controls_for_name,
      bool& required);
  std::unique_ptr<JSONObject> ComputeNumberParameterSchema(
      const ControlVector& controls_for_name,
      bool& required);
  std::unique_ptr<JSONObject> ComputeSelectParameterSchema(
      const ControlVector& controls_for_name,
      bool& required);
  std::unique_ptr<JSONObject> ComputeRangeParameterSchema(
      const ControlVector& controls_for_name,
      bool& required);
  std::unique_ptr<JSONObject> ComputeCheckboxParameterSchema(
      const ControlVector& controls_for_name,
      bool& required);
  std::unique_ptr<JSONObject> ComputeRadioParameterSchema(const ControlVector&,
                                                          bool& required);
  std::unique_ptr<JSONObject> ComputeColorParameterSchema(
      const ControlVector& controls_for_name,
      bool& required);

  // Compute an array representing the values (as HTMLInputElement::Value()
  // of the specified controls, suitable for assignment to a 'oneOf' field.
  // The argument to 'required' will be set if at least one of the controls
  // are required.
  std::unique_ptr<JSONArray> ComputeOneOfArray(
      const ControlVector& controls_for_name,
      bool& required);

  bool ValidateParameterData(const String& name, const JSONValue&);
  bool ValidateTextData(const ControlVector& controls_for_name,
                        const JSONValue&);
  bool ValidateNumberData(const ControlVector& controls_for_name,
                          const JSONValue&);
  bool ValidateCheckboxData(const ControlVector& controls_for_name,
                            const JSONValue&);
  bool ValidateRadioData(const ControlVector& controls_for_name,
                         const JSONValue&);
  bool ValidateSelectData(const ControlVector& controls_for_name,
                          const JSONValue&);

  void FillParameterData(const String& name, const JSONValue&);
  void FillTextData(const ControlVector& controls_for_name, const JSONValue&);
  void FillNumberData(const ControlVector& controls_for_name, const JSONValue&);
  void FillCheckboxData(const ControlVector& controls_for_name,
                        const JSONValue&);
  void FillRadioData(const ControlVector& controls_for_name, const JSONValue&);
  void FillSelectData(const ControlVector& controls_for_name, const JSONValue&);

  void AddTitle(HTMLFormControlElement&, JSONObject&);
  void AddDescription(HTMLFormControlElement&, JSONObject&);

  // It's not clear yet where to host the toolparamtitle/description
  // attributes for <input type=radio>, or other parameters that are
  // associated with more than one element. For now, we use the attributes
  // set on the first control within a group.
  //
  // Note that unlike AddTitle()/AddDescription(), this does not try
  // to find "fallback" values (from <label>, etc) when tool-* attributes
  // are missing.
  //
  // See also: https://github.com/webmachinelearning/webmcp/issues/71
  void AddTitleAndDescriptionFromToolAttributesOnly(HTMLFormControlElement&,
                                                    JSONObject&);

  String ToolParamTitleAttribute(HTMLFormControlElement&);
  String ToolParamDescriptionAttribute(HTMLFormControlElement&);
  String ComputeDescription(HTMLFormControlElement&);
  String LabelText(HTMLFormControlElement&);

  void ProcessForm(HTMLFormElement&);
  ControlVector& EnsureControlVector(const String& name);

  bool IsText(HTMLFormControlElement&) const;
  bool IsDate(HTMLFormControlElement&) const;
  bool IsTime(HTMLFormControlElement&) const;
  bool IsNumber(HTMLFormControlElement&) const;
  bool IsSelect(HTMLFormControlElement&) const;
  bool IsRange(HTMLFormControlElement&) const;
  bool IsCheckbox(HTMLFormControlElement&) const;
  bool IsRadio(HTMLFormControlElement&) const;
  bool IsColor(HTMLFormControlElement&) const;

  bool IsText(const ControlVector& controls_for_name) const;
  bool IsDate(const ControlVector& controls_for_name) const;
  bool IsTime(const ControlVector& controls_for_name) const;
  bool IsNumber(const ControlVector& controls_for_name) const;
  bool IsSelect(const ControlVector& controls_for_name) const;
  bool IsRange(const ControlVector& controls_for_name) const;
  bool IsCheckbox(const ControlVector& controls_for_name) const;
  bool IsRadio(const ControlVector& controls_for_name) const;
  bool IsColor(const ControlVector& controls_for_name) const;

  // Maps a WebMCP parameter name (HTMLFormControlElement::
  // GetWebMCPParameterName()) to a list of form controls.
  HeapHashMap<String, Member<ControlVector>> name_to_controls_;
  // Keeps track of the order of entries into name_to_controls_;
  // we want to produce the JSON object entries in that same order.
  HeapVector<String> ordered_names_;
  HTMLFormControlElement* submit_button_ = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_FORM_MCP_SCHEMA_H_
