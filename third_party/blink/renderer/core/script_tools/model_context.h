// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_MODEL_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_MODEL_CONTEXT_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/types/pass_key.h"
#include "third_party/blink/public/mojom/content_extraction/script_tools.mojom-blink.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_model_context.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_provide_context_params.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_tool_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_tool_registration_params.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/platform/allow_discouraged_type.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

class AbortSignal;
class Element;
class SourceLocation;

class DeclarativeWebMCPTool : public GarbageCollectedMixin {
 public:
  // Executes the associated tool and invokes `done_callback` with the result
  // when the execution is finished. The callback is invoked with a null string
  // if the execution resulted in a navigation, or an error if the execution
  // failed.
  virtual void ExecuteTool(
      String input_arguments,
      base::OnceCallback<
          void(base::expected<String, WebDocument::ScriptToolError>)>
          done_callback) = 0;

  // Returns the input json-schema associated with the tool.
  virtual String ComputeInputSchema() = 0;

  // The <form> backing this declarative tool.
  virtual Element* FormElement() const = 0;
};

class CORE_EXPORT ModelContext : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ModelContext(Document& document, scoped_refptr<base::SingleThreadTaskRunner>);

  void ForEachScriptTool(
      base::FunctionRef<void(const mojom::blink::ScriptTool&)>) const;

  void registerTool(ScriptState* state,
                    ToolRegistrationParams* params,
                    ExceptionState& exception_state);
  void unregisterTool(const String& name, ExceptionState& exception_state);

  void SetScriptToolDeclaration(
      const String& name,
      WebDocument::ScriptToolDeclaration* tool_declaration) const;

  void provideContext(ScriptState* state,
                      ProvideContextParams* params,
                      ExceptionState& exception_state);
  void clearContext();

  using ScriptToolExecutedCallback = base::OnceCallback<void(
      base::expected<WebString, WebDocument::ScriptToolError>)>;

  // TODO: crbug.com/479291237 - remove public/web dependency
  std::optional<uint32_t> ExecuteTool(
      const String& name,
      const String& input_arguments,
      AbortSignal* signal,
      ScriptToolExecutedCallback tool_executed_cb);
  using CrossDocumentScriptToolResultCallback =
      base::OnceCallback<void(String)>;
  void GetCrossDocumentScriptToolResult(
      CrossDocumentScriptToolResultCallback result_callback);

  void CancelTool(uint32_t execution_id);

  void SetToolsChangedCallback(std::optional<base::RepeatingClosure> cb) {
    tools_changed_closure_ = std::move(cb);
  }

  void RegisterDeclarativeTool(String name,
                               String description,
                               DeclarativeWebMCPTool* tool);
  void PauseExecution();
  void DidFinishParsing();

  class CORE_EXPORT ToolData : public GarbageCollected<ToolData> {
   public:
    // Creates a JS-backed tool.
    ToolData(base::PassKey<ModelContext>,
             mojo::StructPtr<mojom::blink::ScriptTool> script_tool,
             V8ToolFunction* v8_tool_function,
             SourceLocation* source_location)
        : script_tool_(std::move(script_tool)),
          v8_tool_function_(v8_tool_function),
          source_location_(source_location) {}

    // Creates a declarative (<form>-backed) tool.
    ToolData(base::PassKey<ModelContext>,
             mojo::StructPtr<mojom::blink::ScriptTool> script_tool,
             DeclarativeWebMCPTool* declarative_tool)
        : script_tool_(std::move(script_tool)),
          declarative_tool_(declarative_tool) {}

    const String& Name() const;

    const mojom::blink::ScriptTool& ScriptTool() const { return *script_tool_; }

    // If this is a JS-provided tool, returns the source location
    // of the call to registerTool(). Otherwise, returns nullptr.
    SourceLocation* GetSourceLocation() const;

    // If this is a declarative tool, returns the <form> element
    // that provided this tool. Otherwise, returns nullptr.
    Element* BackingFormElement() const;

    void Trace(Visitor* visitor) const;

   private:
    friend class ModelContext;

    V8ToolFunction* GetV8ToolFunction() const { return v8_tool_function_; }
    DeclarativeWebMCPTool* DeclarativeTool() const { return declarative_tool_; }

    void RefreshDeclarativeInputSchema();

    mojo::StructPtr<mojom::blink::ScriptTool> script_tool_;
    // A JS-provided MCP tool:
    Member<V8ToolFunction> v8_tool_function_;
    // Used for declarative (form-based) MCP tools only:
    Member<DeclarativeWebMCPTool> declarative_tool_;
    // For JS-provided MCP tools, the location of the registerTool() call.
    Member<SourceLocation> source_location_;
  };

  // Returns registered tools, sorted by CodeUnitCompareLessThan().
  HeapVector<Member<const ToolData>> ListTools() const;

  ExecutionContext* GetExecutionContext() const;

  void Trace(Visitor*) const override;

 private:
  class ToolFunctionFinishedCallback;

  std::optional<uint32_t> ExecuteV8Tool(
      V8ToolFunction* tool_function,
      const String& name,
      const String& input_arguments,
      AbortSignal* signal,
      ScriptToolExecutedCallback tool_executed_cb);
  void ExecuteDeclarativeTool(DeclarativeWebMCPTool* tool,
                              const String& input_arguments,
                              ScriptToolExecutedCallback tool_executed_cb);

  bool RegisterTool(ScriptState* script_state,
                    ToolRegistrationParams* params,
                    ExceptionState& exception_state);

  void OnToolExecuted(uint32_t execution_id, std::optional<String> result);

  void OnToolsChanged();

  HeapHashMap<String, Member<ToolData>> tool_map_;

  uint32_t next_execution_id_ = 0;
  struct PendingExecution {
    String tool_name;
    ScriptToolExecutedCallback callback;
  };
  HashMap<uint32_t, PendingExecution> pending_executions_;

  Vector<CrossDocumentScriptToolResultCallback>
      cross_document_result_callbacks_;

  std::optional<base::RepeatingClosure> tools_changed_closure_;
  Member<Document> document_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  HeapMojoRemote<mojom::blink::ScriptToolHost> script_tool_host_remote_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_MODEL_CONTEXT_H_
