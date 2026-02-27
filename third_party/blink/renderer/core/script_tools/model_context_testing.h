// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_MODEL_CONTEXT_TESTING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_MODEL_CONTEXT_TESTING_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_execute_tool_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_model_context_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_registered_tool.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_tools_changed_callback.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"

namespace blink {
class ModelContext;
class RegisteredTool;

class CORE_EXPORT ModelContextTesting : public EventTarget {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit ModelContextTesting(ModelContext& model_context);

  HeapVector<Member<RegisteredTool>> listTools();
  ScriptPromise<IDLNullable<IDLString>> executeTool(ScriptState* state,
                                                    String tool_name,
                                                    String input_arguments,
                                                    const ExecuteToolOptions*);
  void registerToolsChangedCallback(V8ToolsChangedCallback* callback);
  ScriptPromise<IDLString> getCrossDocumentScriptToolResult(ScriptState* state);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(toolchange, kToolchange)

  // EventTarget:
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  void Trace(Visitor*) const override;

 private:
  void OnToolsChanged();

  Member<ModelContext> model_context_;
  Member<V8ToolsChangedCallback> tools_changed_callback_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_MODEL_CONTEXT_TESTING_H_
