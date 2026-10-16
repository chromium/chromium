// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_MODEL_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_MODEL_CONTEXT_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/types/pass_key.h"
#include "base/unguessable_token.h"
#include "third_party/blink/public/mojom/content_extraction/script_tools.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_model_context.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/dom/scoped_abort_state.h"
#include "third_party/blink/renderer/core/script_tools/script_tool_types.h"
#include "third_party/blink/renderer/platform/allow_discouraged_type.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/base_hash_traits.h"

namespace blink {
class AbortController;
class Document;
class Element;
class ExecuteToolOptions;
class SourceLocation;
class ModelContextGetToolOptions;
class ModelContextRegisterToolOptions;
class ModelContextTool;
class ModelContextToolInvocation;
class Tool;
class V8ToolExecuteCallback;

class DeclarativeWebMCPTool : public GarbageCollectedMixin {
 public:
  // Executes the associated tool and invokes `done_callback` with the result
  // when the execution is finished. The callback is invoked with a null string
  // if the execution resulted in a navigation, or an error if the execution
  // failed.
  virtual void ExecuteTool(
      const base::UnguessableToken& invocation_id,
      String input_arguments,
      base::OnceCallback<void(base::expected<String, ScriptToolError>)>
          done_callback) = 0;

  virtual void CancelTool() = 0;

  virtual String ToolName() const = 0;

  // The unique, non-web-exposed identifier of this tool. It is fixed for the
  // lifetime of the object, and is used to register and unregister the tool.
  virtual const base::UnguessableToken& ToolId() const = 0;

  virtual String ToolDescription() const = 0;

  virtual String ToolTitle() const = 0;

  // Returns the input json-schema associated with the tool.
  virtual String ComputeInputSchema() = 0;

  // The <form> backing this declarative tool.
  virtual Element* FormElement() const = 0;
};

class CORE_EXPORT ToolData : public GarbageCollected<ToolData> {
 public:
  // Creates a JS-backed tool.
  ToolData(base::PassKey<ModelContext>,
           mojo::StructPtr<mojom::blink::ScriptTool> script_tool,
           V8ToolExecuteCallback* v8_tool_function,
           SourceLocation* source_location,
           AbortSignal::AlgorithmHandle* abort_handle)
      : script_tool_(std::move(script_tool)),
        v8_tool_function_(v8_tool_function),
        source_location_(source_location),
        abort_algorithm_handle_(abort_handle) {}

  // Creates a declarative (<form>-backed) tool.
  ToolData(base::PassKey<ModelContext>,
           mojo::StructPtr<mojom::blink::ScriptTool> script_tool,
           DeclarativeWebMCPTool* declarative_tool)
      : script_tool_(std::move(script_tool)),
        declarative_tool_(declarative_tool) {}

  const String& Name() const;

  // The stable, non-web-exposed identifier minted for this tool at
  // registration time. This is the key that both `ModelContext::tool_map_` and
  // the browser process use to look the tool up.
  const base::UnguessableToken& Id() const {
    return script_tool_->tool_id.value();
  }

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

  V8ToolExecuteCallback* GetV8ToolExecuteCallback() const {
    return v8_tool_function_;
  }
  DeclarativeWebMCPTool* DeclarativeTool() const { return declarative_tool_; }

  bool RefreshDeclarativeInputSchema();

  mojo::StructPtr<mojom::blink::ScriptTool> script_tool_;
  // A JS-provided MCP tool:
  Member<V8ToolExecuteCallback> v8_tool_function_;
  // Used for declarative (form-based) MCP tools only:
  Member<DeclarativeWebMCPTool> declarative_tool_;
  // For JS-provided MCP tools, the location of the registerTool() call.
  Member<SourceLocation> source_location_;
  // Keeps the abort algorithm handle alive as long as the tool is registered.
  Member<AbortSignal::AlgorithmHandle> abort_algorithm_handle_ = nullptr;
};

class CORE_EXPORT ModelContext : public EventTarget,
                                 public mojom::blink::ModelContext {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ModelContext(Document& document);

  // Returns true if declarative (<form>-based) WebMCP is enabled in `context`.
  // Declarative WebMCP is gated by its own runtime feature (which is also
  // enabled by the WebMCP origin trial), but it builds on top of the
  // imperative WebMCP infrastructure, so this also requires `WebMCP` to be
  // enabled (via flag or origin trial). This ensures the kWebMCP kill switch
  // disables both.
  static bool IsDeclarativeWebMCPEnabled(const ExecutionContext* context);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(toolchange, kToolchange)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(toolactivated, kToolactivated)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(toolcancel, kToolcancel)

  const AtomicString& InterfaceName() const override;

  void ForEachScriptTool(
      base::FunctionRef<void(const mojom::blink::ScriptTool&)>) const;

  ScriptPromise<IDLUndefined> registerTool(
      ScriptState* state,
      ModelContextTool* tool,
      ModelContextRegisterToolOptions* options);
  ScriptPromise<IDLSequence<Tool>> getTools(
      ScriptState* script_state,
      const ModelContextGetToolOptions* options = nullptr);
  ScriptPromise<IDLNullable<IDLString>> executeTool(ScriptState* script_state,
                                                    Tool* tool) {
    return executeTool(script_state, tool, ScriptObject());
  }
  // `input_object` is empty when the argument was omitted or explicitly
  // `undefined`, in which case we manually treat it as an empty object, by
  // constructing a new one in its place.
  ScriptPromise<IDLNullable<IDLString>> executeTool(
      ScriptState* script_state,
      Tool* tool,
      ScriptObject input_object,
      const ExecuteToolOptions* options = nullptr);
  ScriptPromise<IDLUndefined> resumeTool(ScriptState* script_state,
                                         const String& token,
                                         V8ToolExecuteCallback* callback);
  ScriptPromise<IDLString> RequestContinuationToken(
      base::PassKey<ModelContextToolInvocation>,
      ScriptState* script_state,
      const base::UnguessableToken& invocation_id,
      bool& did_request_token);

  std::optional<ScriptToolDeclaration> GetScriptToolDeclaration(
      const String& name) const;

  bool ExecuteTool(base::UnguessableToken invocation_id,
                   const base::UnguessableToken& tool_id,
                   const String& input_arguments,
                   ScriptToolExecutedCallback tool_executed_cb);
  // Like `ExecuteTool()`, but embedders that identify tools by name. If no tool
  // is currently registered under `name`, this reports the failure in terms of
  // `name`.
  //
  // TODO(https://crbug.com/536063275): Remove this, along with
  // `FindToolDataByName()`, once all embedders always identify tools by ID.
  // Two embedders still execute tools by name:
  //   - DevTools: `WebMCP.invokeTool` accepts a `toolName`, and must keep doing
  //     so through a deprecation window so that existing CDP clients can
  //     migrate to `toolId`.
  //   - Actor: it executes by ID when a prior observation pinned one, but falls
  //     back to the name when the model acts without one.
  bool ExecuteToolByName(base::UnguessableToken invocation_id,
                         const String& name,
                         const String& input_arguments,
                         ScriptToolExecutedCallback tool_executed_cb);
  using CrossDocumentScriptToolResultCallback =
      base::OnceCallback<void(String)>;
  void GetCrossDocumentScriptToolResult(
      const base::UnguessableToken& invocation_id,
      CrossDocumentScriptToolResultCallback result_callback);

  bool CancelTool(base::UnguessableToken invocation_id);

  // Returns whether `tool` was registered. Registration is rejected if, e.g.,
  // the `tools` permissions policy is disabled, or `tool`'s name is invalid or
  // already taken. The caller must not unregister a rejected tool.
  [[nodiscard]] bool RegisterDeclarativeTool(DeclarativeWebMCPTool* tool);
  // Unregisters the tool identified by `tool_id`, which must be registered.
  // Used for both imperative tools (when their `AbortSignal` aborts) and
  // declarative tools (when their form stops being a tool).
  void UnregisterTool(const base::UnguessableToken& tool_id);
  void PauseExecution();

  // mojom::blink::ModelContext implementation:
  void NotifyToolChange() override;
  void ExecuteScriptTool(const base::UnguessableToken& invocation_id,
                         const base::UnguessableToken& tool_id,
                         const String& input_arguments,
                         ExecuteScriptToolCallback callback) override;
  void CancelScriptTool(const base::UnguessableToken& invocation_id) override;

  void DidFinishParsing();

  // Returns registered tools, sorted by CodeUnitCompareLessThan().
  HeapVector<Member<const ToolData>> ListTools() const;

  // Returns the tool registered under `name`, or null if there is none. Tools
  // are keyed by `tool_id`, and `name` is only unique among the tools that are
  // registered at any given moment, so this is a linear scan. The number of
  // tools a document registers is small.
  //
  // TODO(https://crbug.com/536063275): Remove this once all embedders always
  // identify tools by ID. See `ExecuteToolByName()`.
  ToolData* FindToolDataByName(const String& name) const;

  ExecutionContext* GetExecutionContext() const override;

  void OnGetScriptToolsCompleted(
      ScriptPromiseResolver<IDLSequence<Tool>>* resolver,
      Vector<mojom::blink::ExposedScriptToolPtr> tools);

  void OnExecuteScriptToolCompleted(
      ScriptPromiseResolver<IDLNullable<IDLString>>* resolver,
      std::unique_ptr<ScopedAbortState> abort_state,
      const String& result,
      std::optional<mojom::blink::ExecuteRemoteScriptToolError> error);

  void Trace(Visitor*) const override;

 private:
  class ToolFunctionFinishedCallback;
  class ToolUnregisterAbortAlgorithm;

  void ExecuteDeclarativeTool(DeclarativeWebMCPTool* tool,
                              base::UnguessableToken invocation_id,
                              const base::UnguessableToken& tool_id,
                              const String& input_arguments,
                              AbortController* abort_controller,
                              ScriptToolExecutedCallback tool_executed_cb);
  bool ExecuteV8Tool(V8ToolExecuteCallback* tool_function,
                     base::UnguessableToken invocation_id,
                     const base::UnguessableToken& tool_id,
                     const String& name,
                     const String& input_arguments,
                     AbortController* abort_controller,
                     ScriptToolExecutedCallback tool_executed_cb);

  void OnToolFailed(ScriptToolExecutedCallback callback,
                    const base::UnguessableToken& invocation_id,
                    ScriptToolError&& error);

  void OnToolExecuted(
      const base::UnguessableToken& invocation_id,
      base::expected<String, std::pair<ScriptValue, ScriptState*>> result);

  void MaybeRecordToolCount();

  HeapHashMap<base::UnguessableToken, Member<ToolData>> tool_map_;

  struct PendingExecution {
    DISALLOW_NEW();

   public:
    void Trace(Visitor* visitor) const;
    String tool_name;
    // The tool being executed. Used to look the tool back up in `tool_map_`,
    // which `tool_name` cannot safely do, since the name may since have been
    // taken by a different tool.
    base::UnguessableToken tool_id;
    ScriptToolExecutedCallback callback;
    // Created for every tool execution; never null.
    Member<AbortController> abort_controller;
  };
  HeapHashMap<base::UnguessableToken, PendingExecution> pending_executions_;

  Vector<CrossDocumentScriptToolResultCallback>
      cross_document_result_callbacks_;

  Member<Document> document_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  HeapMojoRemote<mojom::blink::ScriptToolHost> script_tool_host_remote_;
  HeapMojoRemote<mojom::blink::ModelContextHost> model_context_host_remote_;
  HeapMojoReceiver<mojom::blink::ModelContext, ModelContext>
      model_context_receiver_{this, nullptr};

  // true when there is a pending or completed task to record the number
  // of registered tools.
  bool will_record_tool_count_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_MODEL_CONTEXT_H_
