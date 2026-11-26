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

Tool::Tool(DOMWindow* source,
           scoped_refptr<const SecurityOrigin> origin,
           mojom::blink::ScriptToolPtr script_tool,
           const ScriptValue& input_schema,
           const ScriptValue& output_schema)
    : source_(source),
      origin_(std::move(origin)),
      script_tool_(std::move(script_tool)) {
  CHECK(source_);
  CHECK(origin_);
  CHECK(script_tool_);
  // The browser only accepts (and therefore only returns) tools that were
  // registered with a `tool_id`.
  CHECK(script_tool_->tool_id);
  const mojom::blink::NaturalLanguageProperties* natural_language_properties =
      script_tool_->natural_language_properties.get();
  CHECK_EQ(!natural_language_properties ||
               natural_language_properties->input_schema.IsNull(),
           input_schema.IsEmpty());
  if (!input_schema.IsEmpty()) {
    input_schema_.Reset(input_schema.GetIsolate(), input_schema.V8Value());
  }
  CHECK_EQ(!natural_language_properties ||
               natural_language_properties->output_schema.IsNull(),
           output_schema.IsEmpty());
  if (!output_schema.IsEmpty()) {
    output_schema_.Reset(output_schema.GetIsolate(), output_schema.V8Value());
  }
}

Tool::~Tool() = default;

void Tool::Trace(Visitor* visitor) const {
  visitor->Trace(source_);
  visitor->Trace(input_schema_);
  visitor->Trace(output_schema_);
  ScriptWrappable::Trace(visitor);
}

const String& Tool::name() const {
  return script_tool_->name;
}

const String& Tool::description() const {
  // A tool without natural language properties is exposed to script with an
  // empty description.
  if (!script_tool_->natural_language_properties) {
    return g_empty_string;
  }
  return script_tool_->natural_language_properties->description;
}

const String& Tool::title() const {
  // A null title in mojo is exposed to script as "".
  if (!script_tool_->natural_language_properties) {
    return g_empty_string;
  }
  return script_tool_->natural_language_properties->title;
}

DOMWindow* Tool::source() const {
  return source_.Get();
}

String Tool::origin() const {
  return origin_->ToString();
}

ScriptObject Tool::inputSchema(ScriptState* script_state) const {
  v8::Isolate* isolate = script_state->GetIsolate();
  if (input_schema_.IsEmpty()) {
    return ScriptObject::CreateNull(isolate);
  }
  return ScriptObject(isolate, input_schema_.Get(isolate));
}

ScriptObject Tool::outputSchema(ScriptState* script_state) const {
  v8::Isolate* isolate = script_state->GetIsolate();
  if (output_schema_.IsEmpty()) {
    return ScriptObject::CreateNull(isolate);
  }
  return ScriptObject(isolate, output_schema_.Get(isolate));
}

ScriptObject Tool::annotations(ScriptState* script_state) const {
  const mojom::blink::NaturalLanguageProperties* natural_language_properties =
      script_tool_->natural_language_properties.get();
  if (!natural_language_properties ||
      !natural_language_properties->annotations) {
    return ScriptObject::CreateNull(script_state->GetIsolate());
  }

  const mojom::blink::ScriptToolAnnotations& script_tool_annotations =
      *natural_language_properties->annotations;
  auto* annotations = ToolAnnotations::Create();
  annotations->setReadOnlyHint(script_tool_annotations.read_only);
  annotations->setUntrustedContentHint(
      script_tool_annotations.untrusted_content);
  annotations->setConsequentialHint(script_tool_annotations.consequential);
  annotations->setDebugging(script_tool_annotations.debugging);
  return ScriptObject::From(script_state, annotations);
}

}  // namespace blink
