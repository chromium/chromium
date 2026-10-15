// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script_tools/tool.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_tool_annotations.h"
#include "third_party/blink/renderer/core/frame/dom_window.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

Tool::Tool(DOMWindow* window,
           mojom::blink::ScriptToolPtr script_tool,
           const ScriptValue& input_schema)
    : window_(window), script_tool_(std::move(script_tool)) {
  CHECK(window_);
  CHECK(script_tool_);
  // The browser only accepts (and therefore only returns) tools that were
  // registered with a `tool_id`.
  CHECK(script_tool_->tool_id);
  CHECK(!script_tool_->tool_id->is_empty());
  CHECK_EQ(script_tool_->input_schema.IsNull(), input_schema.IsEmpty());
  if (!input_schema.IsEmpty()) {
    input_schema_.Reset(input_schema.GetIsolate(), input_schema.V8Value());
  }
}

Tool::~Tool() = default;

void Tool::Trace(Visitor* visitor) const {
  visitor->Trace(window_);
  visitor->Trace(input_schema_);
  ScriptWrappable::Trace(visitor);
}

const String& Tool::name() const {
  return script_tool_->name;
}

const String& Tool::description() const {
  return script_tool_->description;
}

const String& Tool::title() const {
  // A null title in mojo is exposed to script as "".
  return script_tool_->title;
}

DOMWindow* Tool::window() const {
  return window_.Get();
}

String Tool::origin() const {
  return script_tool_->origin->ToString();
}

ScriptObject Tool::inputSchema(ScriptState* script_state) const {
  v8::Isolate* isolate = script_state->GetIsolate();
  if (input_schema_.IsEmpty()) {
    return ScriptObject::CreateNull(isolate);
  }
  return ScriptObject(isolate, input_schema_.Get(isolate));
}

ScriptObject Tool::annotations(ScriptState* script_state) const {
  if (!script_tool_->annotations) {
    return ScriptObject::CreateNull(script_state->GetIsolate());
  }

  auto* annotations = ToolAnnotations::Create();
  annotations->setReadOnlyHint(script_tool_->annotations->read_only);
  annotations->setUntrustedContentHint(
      script_tool_->annotations->untrusted_content);
  annotations->setConsequentialHint(script_tool_->annotations->consequential);
  annotations->setDebugging(script_tool_->annotations->debugging);
  return ScriptObject::From(script_state, annotations);
}

}  // namespace blink
