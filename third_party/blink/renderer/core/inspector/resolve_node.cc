// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/resolve_node.h"

#include "third_party/blink/renderer/bindings/core/v8/binding_security.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"
#include "third_party/blink/renderer/core/inspector/v8_inspector_string.h"

namespace blink {

v8::Local<v8::Value> NodeV8Value(v8::Local<v8::Context> context, Node* node) {
  v8::Isolate* isolate = context->GetIsolate();
  if (!node ||
      !BindingSecurity::ShouldAllowAccessTo(CurrentDOMWindow(isolate), node)) {
    return v8::Null(isolate);
  }
  return ToV8Traits<Node>::ToV8(ScriptState::From(isolate, context), node);
}

std::unique_ptr<v8_inspector::protocol::Runtime::API::RemoteObject> ResolveNode(
    v8_inspector::V8InspectorSession* v8_session,
    Node* node,
    const String& object_group,
    protocol::Maybe<int> v8_execution_context_id) {
  if (!node)
    return nullptr;

  Document* document =
      node->IsDocumentNode() ? &node->GetDocument() : node->ownerDocument();
  LocalFrame* frame = document ? document->GetFrame() : nullptr;
  if (!frame)
    return nullptr;

  v8::Isolate* isolate = document->GetAgent().isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context;
  if (v8_execution_context_id.has_value()) {
    if (!MainThreadDebugger::Instance(isolate)
             ->GetV8Inspector()
             ->contextById(v8_execution_context_id.value())
             .ToLocal(&context)) {
      return nullptr;
    }
  } else {
    ScriptState* script_state = ToScriptStateForMainWorld(frame);
    if (!script_state)
      return nullptr;
    context = script_state->GetContext();
  }
  v8::Context::Scope scope(context);
  return v8_session->wrapObject(context, NodeV8Value(context, node),
                                ToV8InspectorStringView(object_group),
                                false /* generatePreview */);
}

std::unique_ptr<v8_inspector::protocol::Runtime::API::RemoteObject>
NullRemoteObject(v8_inspector::V8InspectorSession* v8_session,
                 LocalFrame* frame,
                 const String& object_group) {
  ScriptState* script_state = ToScriptStateForMainWorld(frame);
  if (!script_state)
    return nullptr;

  ScriptState::Scope scope(script_state);
  return v8_session->wrapObject(
      script_state->GetContext(),
      NodeV8Value(script_state->GetContext(), nullptr),
      ToV8InspectorStringView(object_group), false /* generatePreview */);
}

}  // namespace blink
