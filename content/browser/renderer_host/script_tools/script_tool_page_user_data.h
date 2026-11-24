// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_SCRIPT_TOOLS_SCRIPT_TOOL_PAGE_USER_DATA_H_
#define CONTENT_BROWSER_RENDERER_HOST_SCRIPT_TOOLS_SCRIPT_TOOL_PAGE_USER_DATA_H_

#include <map>
#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/page_user_data.h"
#include "content/public/browser/weak_document_ptr.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/content_extraction/script_tools.mojom.h"

namespace content {

class ScriptToolContinuationManager;

// Associated with the Page, managing pending tool executions within a given
// frame tree. This must be `Page`-scoped instead of document-scoped, and
// therefore differs from `ScriptToolDocumentUserData`, since tool executions
// are fundamentally cross-document, and it's easiest to have a centralized
// place to manage them.
//
// For example, if iframe A invokes a tool in iframe B, and iframe B navigates
// away during tool execution, the destructor needs to traverse the frame tree
// to locate the caller of the tool and run its pending callback, to signal to
// the caller's script that the tool execution has cancelled. The same must
// happen in the inverse, when the caller's script aborts its execution. To
// simplify the "finding" of tool caller and callee, we centralize all pending
// execution data here.
class CONTENT_EXPORT ScriptToolPageUserData
    : public PageUserData<ScriptToolPageUserData> {
 public:
  struct PendingScriptToolExecution {
    PendingScriptToolExecution();
    PendingScriptToolExecution(PendingScriptToolExecution&&);
    PendingScriptToolExecution& operator=(PendingScriptToolExecution&&);
    ~PendingScriptToolExecution();

    blink::DocumentToken caller_token;
    blink::DocumentToken target_token;
    WeakDocumentPtr target_document;
    std::string tool_name;
    std::string input_arguments;
    blink::mojom::ModelContextHost::ExecuteRemoteScriptToolCallback callback;
    // Set to true as soon as the initial target document mints a continuation
    // token via `RequestContinuationToken()`. While true, `callback` is held
    // open waiting for the continuation chain to finish in subsequent
    // document(s), and unloading the initial `target_document` or settling its
    // initial `ExecuteScriptTool()` IPC does not complete or cancel the pending
    // execution.
    bool awaiting_continuation = false;
  };

  explicit ScriptToolPageUserData(Page& page);
  ~ScriptToolPageUserData() override;

  // Returns true if there is an outstanding tool execution for `invocation_id`.
  bool HasPendingScriptToolExecution(
      const blink::ToolInvocationToken& invocation_id) const;

  // When the `ExecuteRemoteScriptTool()` IPC is called on
  // `blink.mojom.ModelContextHost` (i.e., `ScriptToolDocumentUserData`), this
  // method is called, and the callback to resolve/reject the Promise in the
  // renderer is added to `pending_script_tool_executions_`.
  void AddPendingScriptToolExecution(
      const blink::ToolInvocationToken& invocation_id,
      PendingScriptToolExecution execution);
  const PendingScriptToolExecution* FindPendingScriptToolExecution(
      const blink::ToolInvocationToken& invocation_id) const;
  // Called by `continuation_manager` when the target of the pending execution
  // for `invocation_id` mints `token`. Marks the execution as awaiting the
  // continuation, and waits for the continuation's result.
  void OnContinuationTokenCreated(
      ScriptToolContinuationManager& continuation_manager,
      const blink::ToolInvocationToken& invocation_id,
      const blink::ContinuationToken& token);
  // When the browser process calls the `ExecuteScriptTool()` IPC on
  // `blink.mojom.ModelContext` (i.e., `blink::ModelContext`), the callback
  // signifying tool completion in the renderer calls this method. This method
  // in turn takes `result` and `error` (if tool execution failed), and
  // forwards them back to the invoking renderer process by running the
  // response callback that was added in `AddPendingScriptToolExecution()`
  // above.
  void CompletePendingScriptToolExecution(
      const blink::ToolInvocationToken& invocation_id,
      const std::optional<std::string>& result,
      std::optional<blink::mojom::ExecuteRemoteScriptToolError> error);
  // The below "cancellation" methods ensure that a failure to execute a tool to
  // completion is propagated back to the calling renderer under certain
  // circumstances.
  //
  // `CancelPendingScriptToolExecutionsForDocument()` removes all outstanding
  // `PendingScriptToolExecution` structs associated with `rfh`,
  // because the document is being destroyed. This means it serves two purposes:
  //   1. It signals to the callers of all still-running tools in
  //      `rfh` that tool execution was cancelled because the tool
  //      owner is going away (by invoking the tool execution response
  //      callback); and
  //   2. It simply cleans up all pending execution data for tools *called by*
  //      `rfh`, when tool callers gets destroyed.
  //
  void CancelPendingScriptToolExecution(
      const blink::ToolInvocationToken& invocation_id);
  void CancelPendingScriptToolExecutionsForDocument(
      const blink::DocumentToken& document_token);

  base::WeakPtr<ScriptToolPageUserData> GetWeakPtr();

 private:
  friend class PageUserData<ScriptToolPageUserData>;
  PAGE_USER_DATA_KEY_DECL();

  static void SendCancelScriptToolToTarget(
      WeakDocumentPtr target_document,
      const blink::ToolInvocationToken& invocation_id);

  std::map<blink::ToolInvocationToken, PendingScriptToolExecution>
      pending_script_tool_executions_;

  base::WeakPtrFactory<ScriptToolPageUserData> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_SCRIPT_TOOLS_SCRIPT_TOOL_PAGE_USER_DATA_H_
