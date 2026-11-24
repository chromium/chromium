// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_SCRIPT_TOOLS_SCRIPT_TOOL_CONTINUATION_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_SCRIPT_TOOLS_SCRIPT_TOOL_CONTINUATION_MANAGER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "content/common/content_export.h"
#include "content/public/browser/script_tool_continuation.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents_user_data.h"
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
  static bool IsWebMCPContinuationsEnabled(RenderFrameHost& rfh);

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

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_SCRIPT_TOOLS_SCRIPT_TOOL_CONTINUATION_MANAGER_H_
