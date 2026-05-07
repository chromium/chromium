// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MODEL_CONTEXT_USER_DATA_H_
#define CONTENT_BROWSER_RENDERER_HOST_MODEL_CONTEXT_USER_DATA_H_

#include <map>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/page_user_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/content_extraction/script_tools.mojom.h"
#include "url/origin.h"

namespace content {

class RenderFrameHost;

// Associated with the Page, managing pending tool executions within a given
// frame tree. This must be `Page`-scoped instead of document-scoped, and
// therefore differs from `ModelContextUserData`, since tool executions are
// fundamentally cross-document, and it's easiest to have a centralized place to
// manage them.
//
// For example, if iframe A invokes a tool in iframe B, and iframe B navigates
// away during tool execution, the destructor needs to traverse the frame tree
// to locate the caller of the tool and run its pending callback, to signal to
// the caller's script that the tool execution has cancelled. The same must
// happen in the inverse, when the caller's script aborts its execution. To
// simplify the "finding" of tool caller and callee, we centralize all pending
// execution data here.
class CONTENT_EXPORT ModelContextPageUserData
    : public PageUserData<ModelContextPageUserData> {
 public:
  struct PendingScriptToolExecution {
    PendingScriptToolExecution();
    PendingScriptToolExecution(PendingScriptToolExecution&&);
    PendingScriptToolExecution& operator=(PendingScriptToolExecution&&);
    ~PendingScriptToolExecution();

    blink::DocumentToken caller_token;
    blink::DocumentToken target_token;
    std::string tool_name;
    blink::mojom::ModelContextHost::ExecuteRemoteScriptToolCallback callback;
  };

  explicit ModelContextPageUserData(Page& page);
  ~ModelContextPageUserData() override;

  // When the `ExecuteRemoteScriptTool()` IPC is called on
  // `blink.mojom.ModelContextHost` (i.e., `ModelContextUserData`), this method
  // is called, and the callback to resolve/reject the Promise in the renderer
  // is added to `pending_script_tool_executions_`.
  void AddPendingScriptToolExecution(
      const base::UnguessableToken& invocation_id,
      PendingScriptToolExecution execution);
  // When the browser process calls the `ExecuteScriptTool()` IPC on
  // `blink.mojom.ModelContext` (i.e., `blink::ModelContext`), the callback
  // signifying tool completion in the renderer calls this method. This method
  // in turn takes the tool response, and forwards it back to the invoking
  // renderer process, by running the response callback that was added in
  // `AddPendingScriptToolExecution()` above.
  void CompletePendingScriptToolExecution(
      const base::UnguessableToken& invocation_id,
      const std::optional<std::string>& result,
      bool success);
  // The below "cancellation" methods ensure that a failure to execute a tool to
  // completion is propagated back to the calling renderer under certain
  // circumstances.
  //
  // `CancelPendingScriptToolExecutionsForDocument()` removes all outstanding
  // `PendingScriptToolExecution` structs associated with `document_token`,
  // because the document is being destroyed. This means it serves two purposes:
  //   1. It signals to the callers of all still-running tools in
  //      `document_token` that tool execution was cancelled because the tool
  //      owner is going away (by invoking the tool execution response
  //      callback); and
  //   2. It simply cleans up all pending execution data for tools *called by*
  //      `document_token`, when tool callers gets destroyed.
  //
  // `CancelPendingScriptToolExecutionsDueToUnregistration()` is
  // rather obviously more narrowly scoped, and is used to notify the caller of
  // a still-running tool has been unregistered mid-execution.
  //
  // Note that there is no mechanism yet to signal from tool caller to tool
  // executor that the caller has cancelled their invocation. See
  // http://b/481899636 for this.
  void CancelPendingScriptToolExecutionsForDocument(
      const blink::DocumentToken& document_token);
  void CancelPendingScriptToolExecutionsDueToUnregistration(
      const blink::DocumentToken& target_document_token,
      const std::string& tool_name);

  base::WeakPtr<ModelContextPageUserData> GetWeakPtr();

 private:
  friend class PageUserData<ModelContextPageUserData>;
  PAGE_USER_DATA_KEY_DECL();

  std::map<base::UnguessableToken, PendingScriptToolExecution>
      pending_script_tool_executions_;

  base::WeakPtrFactory<ModelContextPageUserData> weak_factory_{this};
};

// `ModelContextUserData` tracks registered tools and handles communication with
// `blink::ModelContext` It is created on-demand when the renderer first
// accesses `navigator.modelContext`. If it does not exist for a frame, it means
// that frame has not used the API yet and does not need to receive broadcasts
// like `toolchange`.
class CONTENT_EXPORT ModelContextUserData
    : public blink::mojom::ModelContextHost,
      public DocumentUserData<ModelContextUserData> {
 public:
  explicit ModelContextUserData(RenderFrameHost* rfh);
  ~ModelContextUserData() override;

  static void Bind(
      RenderFrameHost* rfh,
      mojo::PendingReceiver<blink::mojom::ModelContextHost> receiver);

  // blink::mojom::ModelContextHost implementation:
  void BindModelContext(
      mojo::PendingRemote<blink::mojom::ModelContext> model_context) override;
  void RegisterScriptTool(blink::mojom::ScriptToolPtr tool) override;
  void UnregisterScriptTool(const std::string& name) override;
  void GetScriptTools(GetScriptToolsCallback callback) override;
  void ExecuteRemoteScriptTool(
      const blink::FrameToken& tool_owner_frame_token,
      const url::Origin& expected_target_origin,
      const std::string& name,
      const std::string& input_arguments,
      ExecuteRemoteScriptToolCallback callback) override;

  std::vector<blink::mojom::ScriptToolPtr>& script_tools() {
    return script_tools_;
  }

 private:
  friend class DocumentUserData<ModelContextUserData>;

  DOCUMENT_USER_DATA_KEY_DECL();

  // Iterates over all documents in the frame tree and notifies them that the
  // list of available tools has changed, provided their origin matches one in
  // `exposed_origins` for the tool.
  void NotifyToolChange(const std::vector<url::Origin>& exposed_origins);

  mojo::Receiver<blink::mojom::ModelContextHost> receiver_{this};
  mojo::Remote<blink::mojom::ModelContext> model_context_remote_;

  std::vector<blink::mojom::ScriptToolPtr> script_tools_;

  base::WeakPtrFactory<ModelContextUserData> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MODEL_CONTEXT_USER_DATA_H_
