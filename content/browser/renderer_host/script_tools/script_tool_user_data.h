// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_SCRIPT_TOOLS_SCRIPT_TOOL_USER_DATA_H_
#define CONTENT_BROWSER_RENDERER_HOST_SCRIPT_TOOLS_SCRIPT_TOOL_USER_DATA_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/page_user_data.h"
#include "content/public/browser/script_tool_continuation.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/content_extraction/script_tools.mojom.h"
#include "url/origin.h"

namespace content {

class RenderFrameHost;

// Tracks a WebMCP tool execution across a single continuation token handoff.
// `tool_invocation_id_`, `token_`, and `step_invocation_id_` are immutable and
// fixed at construction time.
class CONTENT_EXPORT ContinuedToolExecution {
 public:
  enum class State {
    // Minted by RequestContinuationToken(); waiting for Redeem().
    kRequested,
    // Redeem() has completed; ResumeScriptTool() is currently executing in
    // `redeeming_document_`.
    kExecuting,
  };

  // `redeeming_document` is the document that ran the resumed step, if the
  // token was redeemed.
  using CompletionCallback =
      base::OnceCallback<void(base::expected<std::string, std::string> result,
                              WeakDocumentPtr redeeming_document)>;

  ContinuedToolExecution(blink::ToolInvocationToken tool_invocation_id,
                         blink::ContinuationToken token,
                         url::Origin origin,
                         std::string tool_name,
                         std::string input_arguments,
                         base::TimeDelta timeout,
                         base::OnceClosure on_expired);
  ContinuedToolExecution(const ContinuedToolExecution&) = delete;
  ContinuedToolExecution& operator=(const ContinuedToolExecution&) = delete;
  ~ContinuedToolExecution();

  const blink::ToolInvocationToken& tool_invocation_id() const {
    return tool_invocation_id_;
  }
  const blink::ContinuationToken& token() const { return token_; }
  const blink::ToolInvocationToken& step_invocation_id() const {
    return step_invocation_id_;
  }
  const url::Origin& origin() const { return origin_; }
  const std::string& tool_name() const { return tool_name_; }
  const std::string& input_arguments() const { return input_arguments_; }
  State state() const { return state_; }
  // Set by `Redeem()`.
  const std::optional<blink::DocumentToken>& redeeming_document_token() const {
    return redeeming_document_token_;
  }
  WeakDocumentPtr redeeming_document() const { return redeeming_document_; }

  // Claims the token for `redeeming_document` if `state_` is `kRequested`.
  // Stops the expiration timer and returns true on success.
  bool Redeem(WeakDocumentPtr redeeming_document,
              blink::DocumentToken redeeming_document_token);

  // Transitions from `kRequested` to `kExecuting` when `ResumeScriptTool()` is
  // dispatched to `redeeming_document_`.
  void MarkExecuting();

  void SetCompletionCallback(CompletionCallback callback);
  bool HasCompletionCallback() const { return !completion_callback_.is_null(); }
  void CancelCompletionCallback();
  void Complete(base::expected<std::string, std::string> result);

 private:
  const blink::ToolInvocationToken tool_invocation_id_;
  const blink::ContinuationToken token_;
  // Identifies the resumed execution step in `redeeming_document_`, for
  // `ResumeScriptTool()` and `CancelScriptTool()`. Distinct from
  // `tool_invocation_id_`, which identifies the original tool execution.
  const blink::ToolInvocationToken step_invocation_id_;
  const url::Origin origin_;
  const std::string tool_name_;
  const std::string input_arguments_;
  WeakDocumentPtr redeeming_document_;
  std::optional<blink::DocumentToken> redeeming_document_token_;
  State state_ = State::kRequested;
  base::OneShotTimer expiration_timer_;
  CompletionCallback completion_callback_;
};

// Manages active `ContinuedToolExecution` instances across a `WebContents`, so
// that a continuation token outlives the document that minted it. The token can
// be redeemed by a same-origin document in the primary page, for example after
// the tool's frame navigates, or in another frame. The tool execution's caller
// must outlive the continuation to receive its result.
class CONTENT_EXPORT ScriptToolContinuationManager
    : public WebContentsUserData<ScriptToolContinuationManager> {
 public:
  static ScriptToolContinuationManager* GetForRenderFrameHost(
      RenderFrameHost& rfh);
  static ScriptToolContinuationManager* GetOrCreateForRenderFrameHost(
      RenderFrameHost& rfh);

  ~ScriptToolContinuationManager() override;

  void RegisterExternalExecution(
      RenderFrameHost& target_rfh,
      const blink::ToolInvocationToken& invocation_id,
      std::string tool_name,
      std::string input_arguments,
      ScriptToolContinuationTokenCreatedCallback on_token_created,
      ContinuedToolExecution::CompletionCallback on_completed);
  // Unregisters a browser-initiated execution. If its target document already
  // requested a continuation token, cancels the continuation (see
  // CancelContinuation()).
  void UnregisterExternalExecution(
      const blink::ToolInvocationToken& invocation_id);

  std::optional<blink::ContinuationToken> RequestContinuationToken(
      RenderFrameHost& rfh,
      const blink::ToolInvocationToken& invocation_id);

  // Redeems `token` for `rfh` and replies to the renderer via `reply`. On
  // acceptance, `reply` runs *before* `ResumeScriptTool()` is sent, so that the
  // renderer resolves the `resumeTool()` Promise (and drains its microtasks)
  // before the resumed callback is invoked. Both interfaces share one ordered
  // channel.
  void RedeemContinuationToken(
      RenderFrameHost& rfh,
      const blink::ContinuationToken& token,
      blink::mojom::ModelContextHost::RedeemContinuationTokenCallback reply);

  std::optional<blink::ContinuationToken> GetContinuationToken(
      const blink::ToolInvocationToken& tool_invocation_id) const;

  // Returns true if `invocation_id` identifies a registered browser-initiated
  // execution, or an execution with a continuation.
  bool IsInvocationIdInUse(
      const blink::ToolInvocationToken& invocation_id) const;

  void WaitForContinuation(const blink::ContinuationToken& token,
                           ContinuedToolExecution::CompletionCallback callback);

  void CancelContinuation(const blink::ContinuationToken& token);

  void OnDocumentDestroyed(
      const blink::DocumentToken& destroyed_document_token);

  base::WeakPtr<ScriptToolContinuationManager> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  friend class WebContentsUserData<ScriptToolContinuationManager>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  explicit ScriptToolContinuationManager(WebContents* web_contents);

  ContinuedToolExecution* FindByToken(const blink::ContinuationToken& token);

  void StartResumedExecution(
      const blink::ToolInvocationToken& tool_invocation_id);
  void OnResumedToolCompleted(
      const blink::ToolInvocationToken& tool_invocation_id,
      const std::optional<std::string>& result,
      std::optional<blink::mojom::ScriptToolErrorCode> error_code);
  void OnTokenExpired(const blink::ToolInvocationToken& tool_invocation_id);
  void FailAndRemove(const blink::ToolInvocationToken& tool_invocation_id,
                     const std::string& error_message);

  struct ExternalExecution {
    ExternalExecution(
        blink::DocumentToken tool_owner_document_token,
        std::string tool_name,
        std::string input_arguments,
        ScriptToolContinuationTokenCreatedCallback on_token_created,
        ContinuedToolExecution::CompletionCallback on_completed);
    ExternalExecution(ExternalExecution&&);
    ExternalExecution& operator=(ExternalExecution&&);
    ~ExternalExecution();

    // The document running the tool's initial step, which is the only one
    // that may request a continuation token for it.
    blink::DocumentToken tool_owner_document_token;
    std::string tool_name;
    std::string input_arguments;
    ScriptToolContinuationTokenCreatedCallback on_token_created;
    // Becomes the completion callback of the continuation, if the target
    // document requests a continuation token.
    ContinuedToolExecution::CompletionCallback on_completed;
  };

  // Both maps are keyed by `tool_invocation_id`.
  //
  // An entry in `external_executions_` is removed when:
  // - its registration is destroyed (UnregisterExternalExecution()), i.e. when
  //   the caller's tool execution finishes, fails, or is cancelled,
  // - its tool owner document is destroyed (OnDocumentDestroyed()),
  // - its tool owner document requests a continuation token, which moves its
  //   contents into a `ContinuedToolExecution` in `continuations_`, or
  // - `this` is destroyed with its WebContents.
  std::map<blink::ToolInvocationToken, ExternalExecution> external_executions_;
  std::map<blink::ToolInvocationToken, std::unique_ptr<ContinuedToolExecution>>
      continuations_;

  base::WeakPtrFactory<ScriptToolContinuationManager> weak_factory_{this};
};

// Associated with the Page, managing pending tool executions within a given
// frame tree. This must be `Page`-scoped instead of document-scoped, and
// therefore differs from `ScriptToolUserData`, since tool executions are
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
  // `blink.mojom.ModelContextHost` (i.e., `ScriptToolUserData`), this method
  // is called, and the callback to resolve/reject the Promise in the renderer
  // is added to `pending_script_tool_executions_`.
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

// `ScriptToolUserData` tracks registered tools and handles communication with
// `blink::ModelContext` It is created on-demand when the renderer first
// accesses `document.modelContext`. If it does not exist for a frame, it means
// that frame has not used the API yet and does not need to receive broadcasts
// like `toolchange`.
class CONTENT_EXPORT ScriptToolUserData
    : public blink::mojom::ModelContextHost,
      public DocumentUserData<ScriptToolUserData> {
 public:
  explicit ScriptToolUserData(RenderFrameHost* rfh);
  ~ScriptToolUserData() override;

  static void Bind(
      RenderFrameHost* rfh,
      mojo::PendingAssociatedReceiver<blink::mojom::ModelContextHost> receiver);

  // blink::mojom::ModelContextHost implementation:
  void BindModelContext(
      mojo::PendingAssociatedRemote<blink::mojom::ModelContext> model_context)
      override;
  void RegisterScriptTool(blink::mojom::ScriptToolPtr tool,
                          RegisterScriptToolCallback callback) override;
  void UnregisterScriptTool(const blink::ScriptToolToken& tool_id) override;
  void GetScriptTools(const std::vector<url::Origin>& from_origins,
                      GetScriptToolsCallback callback) override;
  void ExecuteRemoteScriptTool(
      const blink::ToolInvocationToken& invocation_id,
      const blink::ScriptToolToken& tool_id,
      const blink::FrameToken& tool_owner_frame_token,
      const url::Origin& expected_target_origin,
      const std::string& input_arguments,
      ExecuteRemoteScriptToolCallback callback) override;
  void CancelRemoteScriptTool(
      const blink::ToolInvocationToken& invocation_id) override;
  void SetScriptToolsReady(bool ready) override;
  void RequestContinuationToken(
      const blink::ToolInvocationToken& invocation_id,
      RequestContinuationTokenCallback callback) override;
  void RedeemContinuationToken(
      const blink::ContinuationToken& token,
      RedeemContinuationTokenCallback callback) override;

  std::vector<blink::mojom::ScriptToolPtr>& script_tools() {
    return script_tools_;
  }

  bool script_tools_waiting() const { return script_tools_waiting_; }

 private:
  friend class DocumentUserData<ScriptToolUserData>;
  friend class ScriptToolPageUserData;
  friend class ScriptToolContinuationManager;

  DOCUMENT_USER_DATA_KEY_DECL();

  // Iterates over all documents in the frame tree and notifies them that the
  // list of available tools has changed, provided their origin matches one in
  // `exposed_origins` for the tool.
  void NotifyToolChange(const std::vector<url::Origin>& exposed_origins);

  mojo::AssociatedReceiver<blink::mojom::ModelContextHost> receiver_{this};
  mojo::AssociatedRemote<blink::mojom::ModelContext> model_context_remote_;

  std::vector<blink::mojom::ScriptToolPtr> script_tools_;

  // Whether the document is waiting for script tools to be ready
  // via unsettled Promises passed to modelContext.waitForToolsUntil().
  bool script_tools_waiting_ = false;

  base::WeakPtrFactory<ScriptToolUserData> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_SCRIPT_TOOLS_SCRIPT_TOOL_USER_DATA_H_
