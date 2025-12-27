// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_MODEL_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_MODEL_CONTEXT_H_

#include "base/functional/callback.h"
#include "third_party/blink/public/mojom/content_extraction/script_tools.mojom-blink.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_model_context.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_provide_context_params.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_tool_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_tool_registration_params.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/allow_discouraged_type.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"

namespace blink {

class CORE_EXPORT ModelContext : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit ModelContext(scoped_refptr<base::SingleThreadTaskRunner>);

  void ForEachScriptTool(
      base::FunctionRef<void(const mojom::blink::ScriptTool&)>) const;

  void registerTool(ScriptState* state,
                    ToolRegistrationParams* params,
                    ExceptionState& exception_state);
  void unregisterTool(const String& name, ExceptionState& exception_state);

  void provideContext(ScriptState* state,
                      ProvideContextParams* params,
                      ExceptionState& exception_state);
  void clearContext();

  void ExecuteTool(const String& name,
                   const String& input_arguments,
                   WebDocument::ScriptToolExecutedCallback tool_executed_cb);

  void SetToolsChangedCallback(std::optional<base::RepeatingClosure> cb) {
    tools_changed_closure_ = std::move(cb);
  }

  void Trace(Visitor*) const override;

 private:
  class ToolFunctionFinishedCallback;

  class ToolData : public GarbageCollected<ToolData> {
   public:
    void Trace(Visitor* visitor) const;

    mojo::StructPtr<mojom::blink::ScriptTool> script_tool;
    Member<V8ToolFunction> tool_function;
  };

  bool RegisterTool(ScriptState* script_state,
                    ToolRegistrationParams* params,
                    ExceptionState& exception_state);

  void OnToolExecuted(uint32_t execution_id, std::optional<String> result);

  void OnToolsChanged();

  HeapHashMap<String, Member<ToolData>> tool_map_;

  uint32_t next_execution_id_ = 0;
  HashMap<uint32_t, WebDocument::ScriptToolExecutedCallback>
      pending_executions_;

  std::optional<base::RepeatingClosure> tools_changed_closure_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_MODEL_CONTEXT_H_
