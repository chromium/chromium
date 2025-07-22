// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script_tools/automation_delegate.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_annotations_dict.h"

namespace blink {

namespace {

String ValidateAndStringifyObject(ScriptState* script_state,
                                  const ScriptObject& input) {
  v8::Local<v8::String> value;
  if (!v8::JSON::Stringify(script_state->GetContext(), input.V8Object())
           .ToLocal(&value)) {
    return String();
  }
  return ToBlinkString<String>(script_state->GetIsolate(), value,
                               kDoNotExternalize);
}

}  // namespace

AutomationDelegate::AutomationDelegate() = default;

void AutomationDelegate::ForEachScriptTool(
    base::FunctionRef<void(const mojom::blink::ScriptTool&)> func) const {
  for (const auto& tool : tool_map_) {
    func(*tool.value->script_tool);
  }
}

void AutomationDelegate::registerTool(ScriptState* script_state,
                                      ToolRegistrationParams* params,
                                      ExceptionState& exception_state) {
  if (tool_map_.find(params->name()) != tool_map_.end()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Duplicate tool name");
    return;
  }

  String input_schema;
  if (params->hasInputSchema()) {
    input_schema =
        ValidateAndStringifyObject(script_state, params->inputSchema());
    if (!input_schema) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "Invalid input schema");
      return;
    }
  }

  auto* tool_data = MakeGarbageCollected<ToolData>();

  auto script_tool = mojom::blink::ScriptTool::New();
  script_tool->name = params->name();
  script_tool->description = params->description();
  script_tool->input_schema = input_schema;

  if (params->hasAnnotations()) {
    script_tool->annotations = mojom::blink::ScriptToolAnnotations::New();
    script_tool->annotations->read_only = params->annotations()->readOnlyHint();
  }

  tool_data->script_tool = std::move(script_tool);
  tool_data->tool_function = params->execute();

  LOG(ERROR) << "hellp tool";
  tool_map_.insert(params->name(), std::move(tool_data));
}

void AutomationDelegate::unregisterTool(ScriptState* script_state,
                                        const String& tool_name,
                                        ExceptionState& exception_state) {
  auto it = tool_map_.find(tool_name);
  if (it == tool_map_.end()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid tool name");
    return;
  }

  tool_map_.erase(it);
}

void AutomationDelegate::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(tool_map_);
}

void AutomationDelegate::ToolData::Trace(Visitor* visitor) const {
  visitor->Trace(tool_function);
}

}  // namespace blink
