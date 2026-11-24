// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/script_tools/script_tool_continuation_manager.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/uuid.h"
#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/script_tools/script_tool_document_user_data.h"
#include "content/browser/renderer_host/script_tools/script_tool_page_user_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/script_tool_continuation.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"

namespace content {

namespace {

// Receives the result of the continuation of a browser-initiated execution,
// and unregisters the execution from `manager_` when destroyed.
class ScriptToolExecutionRegistrationImpl
    : public ScriptToolExecutionRegistration {
 public:
  explicit ScriptToolExecutionRegistrationImpl(
      const blink::ToolInvocationToken& invocation_id)
      : invocation_id_(invocation_id) {}

  ~ScriptToolExecutionRegistrationImpl() override {
    if (manager_) {
      manager_->UnregisterExternalExecution(invocation_id_);
    }
  }

  void set_manager(base::WeakPtr<ScriptToolContinuationManager> manager) {
    manager_ = std::move(manager);
  }

  // Returns the callback that the continuation completes with.
  ContinuedToolExecution::CompletionCallback GetCompletionCallback() {
    return base::BindOnce(
        &ScriptToolExecutionRegistrationImpl::OnContinuationCompleted,
        weak_factory_.GetWeakPtr());
  }

  // ScriptToolExecutionRegistration:
  void WaitForContinuation(
      ScriptToolContinuationCompletedCallback callback) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    CHECK(!callback_);
    callback_ = std::move(callback);
  }

 private:
  void OnContinuationCompleted(base::expected<std::string, std::string> result,
                               WeakDocumentPtr redeeming_document) {
    if (!callback_) {
      return;
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ScriptToolExecutionRegistrationImpl::RunCallback,
                       weak_factory_.GetWeakPtr(), std::move(result),
                       std::move(redeeming_document)));
  }

  // `callback_` may destroy `this`.
  void RunCallback(base::expected<std::string, std::string> result,
                   WeakDocumentPtr redeeming_document) {
    std::move(callback_).Run(std::move(result), std::move(redeeming_document));
  }

  base::WeakPtr<ScriptToolContinuationManager> manager_;
  const blink::ToolInvocationToken invocation_id_;
  ScriptToolContinuationCompletedCallback callback_;
  base::WeakPtrFactory<ScriptToolExecutionRegistrationImpl> weak_factory_{this};
};

}  // namespace

ContinuedToolExecution::ContinuedToolExecution(
    blink::ToolInvocationToken tool_invocation_id,
    blink::ContinuationToken token,
    url::Origin origin,
    std::string tool_name,
    std::string input_arguments,
    base::TimeDelta timeout,
    base::OnceClosure on_expired)
    : tool_invocation_id_(tool_invocation_id),
      token_(std::move(token)),
      // Mint a new invocation ID for the resumed step.
      step_invocation_id_(blink::ToolInvocationToken()),
      origin_(std::move(origin)),
      tool_name_(std::move(tool_name)),
      input_arguments_(std::move(input_arguments)) {
  expiration_timer_.Start(FROM_HERE, timeout, std::move(on_expired));
}

ContinuedToolExecution::~ContinuedToolExecution() = default;

bool ContinuedToolExecution::Redeem(
    WeakDocumentPtr redeeming_document,
    blink::DocumentToken redeeming_document_token) {
  if (state_ != State::kRequested) {
    return false;
  }
  redeeming_document_ = std::move(redeeming_document);
  redeeming_document_token_ = redeeming_document_token;
  expiration_timer_.Stop();
  return true;
}

void ContinuedToolExecution::MarkExecuting() {
  CHECK_EQ(state_, State::kRequested);
  CHECK(redeeming_document_token_.has_value());
  state_ = State::kExecuting;
}

void ContinuedToolExecution::SetCompletionCallback(
    CompletionCallback callback) {
  completion_callback_ = std::move(callback);
}

void ContinuedToolExecution::CancelCompletionCallback() {
  completion_callback_.Reset();
}

void ContinuedToolExecution::Complete(
    base::expected<std::string, std::string> result) {
  if (completion_callback_) {
    std::move(completion_callback_).Run(std::move(result), redeeming_document_);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ScriptToolContinuationManager);

ScriptToolContinuationManager::ExternalExecution::ExternalExecution(
    blink::DocumentToken tool_owner_document_token,
    std::string tool_name,
    std::string input_arguments,
    ScriptToolContinuationTokenCreatedCallback on_token_created,
    ContinuedToolExecution::CompletionCallback on_completed)
    : tool_owner_document_token(tool_owner_document_token),
      tool_name(std::move(tool_name)),
      input_arguments(std::move(input_arguments)),
      on_token_created(std::move(on_token_created)),
      on_completed(std::move(on_completed)) {}

ScriptToolContinuationManager::ExternalExecution::ExternalExecution(
    ExternalExecution&&) = default;
ScriptToolContinuationManager::ExternalExecution&
ScriptToolContinuationManager::ExternalExecution::operator=(
    ExternalExecution&&) = default;
ScriptToolContinuationManager::ExternalExecution::~ExternalExecution() =
    default;

// static
bool ScriptToolContinuationManager::IsWebMCPContinuationsEnabled(
    RenderFrameHost& rfh) {
  return ScriptToolDocumentUserData::IsWebMCPEnabled(rfh) &&
         base::FeatureList::IsEnabled(blink::features::kWebMCPContinuations);
}

// static
ScriptToolContinuationManager*
ScriptToolContinuationManager::GetForRenderFrameHost(RenderFrameHost& rfh) {
  WebContents* web_contents = WebContents::FromRenderFrameHost(&rfh);
  return web_contents ? FromWebContents(web_contents) : nullptr;
}

// static
ScriptToolContinuationManager*
ScriptToolContinuationManager::GetOrCreateForRenderFrameHost(
    RenderFrameHost& rfh) {
  WebContents* web_contents = WebContents::FromRenderFrameHost(&rfh);
  return web_contents ? GetOrCreateForWebContents(web_contents) : nullptr;
}

ScriptToolContinuationManager::ScriptToolContinuationManager(
    WebContents* web_contents)
    : WebContentsUserData<ScriptToolContinuationManager>(*web_contents) {}

ScriptToolContinuationManager::~ScriptToolContinuationManager() {
  // The completion callbacks run below may destroy registrations of
  // browser-initiated executions, which must not call back into `this`.
  weak_factory_.InvalidateWeakPtrs();
  auto continuations = std::move(continuations_);
  continuations_.clear();
  for (auto& [tool_invocation_id, continuation] : continuations) {
    if (continuation->HasCompletionCallback()) {
      continuation->Complete(
          base::unexpected("WebContents destroyed during tool continuation."));
    }
  }
}

void ScriptToolContinuationManager::RegisterExternalExecution(
    RenderFrameHost& target_rfh,
    const blink::ToolInvocationToken& invocation_id,
    std::string tool_name,
    std::string input_arguments,
    ScriptToolContinuationTokenCreatedCallback on_token_created,
    ContinuedToolExecution::CompletionCallback on_completed) {
  // Browser-initiated invocation IDs are unguessable tokens minted by the
  // browser.
  CHECK(!IsInvocationIdInUse(invocation_id));
  auto& rfh_impl = static_cast<RenderFrameHostImpl&>(target_rfh);
  external_executions_.emplace(
      invocation_id,
      ExternalExecution(rfh_impl.GetDocumentToken(), std::move(tool_name),
                        std::move(input_arguments), std::move(on_token_created),
                        std::move(on_completed)));
}

void ScriptToolContinuationManager::UnregisterExternalExecution(
    const blink::ToolInvocationToken& invocation_id) {
  external_executions_.erase(invocation_id);
  // If the target document requested a continuation token, the caller no
  // longer waits for the continuation.
  if (auto it = continuations_.find(invocation_id);
      it != continuations_.end()) {
    CancelContinuation(it->second->token());
  }
}

std::optional<blink::ContinuationToken>
ScriptToolContinuationManager::RequestContinuationToken(
    RenderFrameHost& rfh,
    const blink::ToolInvocationToken& invocation_id) {
  if (!IsWebMCPContinuationsEnabled(rfh)) {
    // TODO(crbug.com/573662751): Migrate to mojo::ReportBadMessage.
    bad_message::ReceivedBadMessage(
        rfh.GetProcess(), bad_message::RFHI_WEBMCP_INVALID_TOOL_CONTINUATION);
    return std::nullopt;
  }
  if (!rfh.IsActive()) {
    return std::nullopt;
  }
  // `rfh.GetMainFrame()->IsInPrimaryMainFrame()` is true for both the primary
  // main frame and all subframes of the primary page, and false for non-primary
  // MPArch frame trees (such as prerendered pages or fenced frames).
  if (!rfh.GetMainFrame()->IsInPrimaryMainFrame()) {
    return std::nullopt;
  }
  if (continuations_.contains(invocation_id)) {
    // TODO(crbug.com/573662751): Migrate to mojo::ReportBadMessage.
    bad_message::ReceivedBadMessage(
        rfh.GetProcess(), bad_message::RFHI_WEBMCP_INVALID_TOOL_CONTINUATION);
    return std::nullopt;
  }

  auto& rfh_impl = static_cast<RenderFrameHostImpl&>(rfh);
  const blink::DocumentToken document_token = rfh_impl.GetDocumentToken();

  std::string tool_name;
  std::string input_arguments;
  bool is_page_execution = false;
  ScriptToolContinuationTokenCreatedCallback on_token_created;
  ContinuedToolExecution::CompletionCallback on_completed;
  auto* page_data = ScriptToolPageUserData::GetForPage(rfh.GetPage());
  if (const auto* pending_execution =
          page_data ? page_data->FindPendingScriptToolExecution(invocation_id)
                    : nullptr) {
    if (pending_execution->target_token != document_token ||
        pending_execution->awaiting_continuation) {
      // TODO(crbug.com/573662751): Migrate to mojo::ReportBadMessage.
      bad_message::ReceivedBadMessage(
          rfh.GetProcess(), bad_message::RFHI_WEBMCP_INVALID_TOOL_CONTINUATION);
      return std::nullopt;
    }
    tool_name = pending_execution->tool_name;
    input_arguments = pending_execution->input_arguments;
    is_page_execution = true;
  } else if (auto ext_it = external_executions_.find(invocation_id);
             ext_it != external_executions_.end()) {
    if (ext_it->second.tool_owner_document_token != document_token) {
      // TODO(crbug.com/573662751): Migrate to mojo::ReportBadMessage.
      bad_message::ReceivedBadMessage(
          rfh.GetProcess(), bad_message::RFHI_WEBMCP_INVALID_TOOL_CONTINUATION);
      return std::nullopt;
    }
    tool_name = std::move(ext_it->second.tool_name);
    input_arguments = std::move(ext_it->second.input_arguments);
    on_token_created = std::move(ext_it->second.on_token_created);
    on_completed = std::move(ext_it->second.on_completed);
    external_executions_.erase(ext_it);
  } else {
    return std::nullopt;
  }

  blink::ContinuationToken new_token(base::Uuid::GenerateRandomV4());

  auto [continuation_it, inserted] = continuations_.emplace(
      invocation_id,
      std::make_unique<ContinuedToolExecution>(
          /*tool_invocation_id=*/invocation_id, new_token,
          rfh.GetLastCommittedOrigin(), std::move(tool_name),
          std::move(input_arguments),
          blink::features::kWebMCPContinuationTimeout.Get(),
          base::BindOnce(&ScriptToolContinuationManager::OnTokenExpired,
                         weak_factory_.GetWeakPtr(), invocation_id)));
  if (is_page_execution) {
    page_data->OnContinuationTokenCreated(*this, invocation_id, new_token);
  } else {
    continuation_it->second->SetCompletionCallback(std::move(on_completed));
    if (on_token_created) {
      std::move(on_token_created).Run();
    }
  }
  return new_token;
}

void ScriptToolContinuationManager::RedeemContinuationToken(
    RenderFrameHost& rfh,
    const blink::ContinuationToken& token,
    blink::mojom::ModelContextHost::RedeemContinuationTokenCallback reply) {
  if (!rfh.IsActive() || !rfh.GetMainFrame()->IsInPrimaryMainFrame()) {
    std::move(reply).Run(false);
    return;
  }
  ContinuedToolExecution* continuation = FindByToken(token);
  if (!continuation ||
      !rfh.GetLastCommittedOrigin().IsSameOriginWith(continuation->origin())) {
    std::move(reply).Run(false);
    return;
  }
  auto& rfh_impl = static_cast<RenderFrameHostImpl&>(rfh);
  if (!continuation->Redeem(rfh_impl.GetWeakDocumentPtr(),
                            rfh_impl.GetDocumentToken())) {
    std::move(reply).Run(false);
    return;
  }
  // Reply before sending `ResumeScriptTool()`; see the header comment.
  std::move(reply).Run(true);
  StartResumedExecution(continuation->tool_invocation_id());
}

std::optional<blink::ContinuationToken>
ScriptToolContinuationManager::GetContinuationToken(
    const blink::ToolInvocationToken& tool_invocation_id) const {
  auto it = continuations_.find(tool_invocation_id);
  if (it == continuations_.end()) {
    return std::nullopt;
  }
  return it->second->token();
}

bool ScriptToolContinuationManager::IsInvocationIdInUse(
    const blink::ToolInvocationToken& invocation_id) const {
  return external_executions_.contains(invocation_id) ||
         continuations_.contains(invocation_id);
}

void ScriptToolContinuationManager::WaitForContinuation(
    const blink::ContinuationToken& token,
    ContinuedToolExecution::CompletionCallback callback) {
  ContinuedToolExecution* continuation = FindByToken(token);
  if (!continuation) {
    std::move(callback).Run(
        base::unexpected("Continuation token not found or already failed."),
        WeakDocumentPtr());
    return;
  }
  continuation->SetCompletionCallback(std::move(callback));
}

void ScriptToolContinuationManager::CancelContinuation(
    const blink::ContinuationToken& token) {
  ContinuedToolExecution* continuation = FindByToken(token);
  if (!continuation) {
    return;
  }
  if (continuation->state() == ContinuedToolExecution::State::kExecuting) {
    if (RenderFrameHost* redeeming_rfh =
            continuation->redeeming_document().AsRenderFrameHostIfValid()) {
      if (auto* redeeming_data =
              ScriptToolDocumentUserData::GetForCurrentDocument(
                  redeeming_rfh)) {
        if (redeeming_data->model_context_remote_.is_bound()) {
          redeeming_data->model_context_remote_->CancelScriptTool(
              continuation->step_invocation_id());
        }
      }
    }
    // The caller cancelled the continuation, so don't run its completion
    // callback.
    continuation->CancelCompletionCallback();
    FailAndRemove(continuation->tool_invocation_id(),
                  "Continuation execution cancelled.");
    return;
  }
  // If the token has not yet been redeemed (`State::kRequested`), detach the
  // caller's completion callback without revoking the token so a recipient can
  // still redeem it.
  continuation->CancelCompletionCallback();
}

void ScriptToolContinuationManager::OnDocumentDestroyed(
    const blink::DocumentToken& destroyed_document_token) {
  std::erase_if(external_executions_, [&](const auto& entry) {
    return entry.second.tool_owner_document_token == destroyed_document_token;
  });
  std::vector<blink::ToolInvocationToken> to_fail;
  for (const auto& [tool_invocation_id, continuation] : continuations_) {
    const bool redeemer_unloaded_before_completing =
        continuation->state() == ContinuedToolExecution::State::kExecuting &&
        continuation->redeeming_document_token() == destroyed_document_token;
    if (redeemer_unloaded_before_completing) {
      to_fail.push_back(tool_invocation_id);
    }
  }
  for (const auto& tool_invocation_id : to_fail) {
    FailAndRemove(
        tool_invocation_id,
        "Document hosting continuation unloaded before completing tool.");
  }
}

ContinuedToolExecution* ScriptToolContinuationManager::FindByToken(
    const blink::ContinuationToken& token) {
  for (auto& [tool_invocation_id, continuation] : continuations_) {
    if (continuation->token() == token) {
      return continuation.get();
    }
  }
  return nullptr;
}

void ScriptToolContinuationManager::StartResumedExecution(
    const blink::ToolInvocationToken& tool_invocation_id) {
  auto it = continuations_.find(tool_invocation_id);
  if (it == continuations_.end()) {
    return;
  }
  ContinuedToolExecution* continuation = it->second.get();
  if (continuation->state() != ContinuedToolExecution::State::kRequested ||
      !continuation->redeeming_document_token().has_value()) {
    return;
  }

  RenderFrameHost* redeeming_rfh =
      continuation->redeeming_document().AsRenderFrameHostIfValid();
  if (!redeeming_rfh || !redeeming_rfh->IsActive()) {
    FailAndRemove(tool_invocation_id,
                  "Redeeming document is no longer active.");
    return;
  }
  auto* redeeming_data =
      ScriptToolDocumentUserData::GetForCurrentDocument(redeeming_rfh);
  if (!redeeming_data || !redeeming_data->model_context_remote_.is_bound()) {
    FailAndRemove(tool_invocation_id,
                  "Redeeming document has no bound ModelContext.");
    return;
  }

  continuation->MarkExecuting();
  redeeming_data->model_context_remote_->ResumeScriptTool(
      continuation->step_invocation_id(), continuation->token(),
      continuation->tool_name(), continuation->input_arguments(),
      base::BindOnce(&ScriptToolContinuationManager::OnResumedToolCompleted,
                     weak_factory_.GetWeakPtr(), tool_invocation_id));
}

void ScriptToolContinuationManager::OnResumedToolCompleted(
    const blink::ToolInvocationToken& tool_invocation_id,
    const std::optional<std::string>& result,
    std::optional<blink::mojom::ScriptToolErrorCode> error_code) {
  auto it = continuations_.find(tool_invocation_id);
  if (it == continuations_.end()) {
    return;
  }
  if (error_code.has_value()) {
    FailAndRemove(tool_invocation_id, "Resumed tool execution failed.");
    return;
  }
  std::unique_ptr<ContinuedToolExecution> continuation = std::move(it->second);
  continuations_.erase(it);
  continuation->Complete(base::ok(result.value_or(std::string())));
}

void ScriptToolContinuationManager::OnTokenExpired(
    const blink::ToolInvocationToken& tool_invocation_id) {
  FailAndRemove(tool_invocation_id, "Continuation token expired.");
}

void ScriptToolContinuationManager::FailAndRemove(
    const blink::ToolInvocationToken& tool_invocation_id,
    const std::string& error_message) {
  auto it = continuations_.find(tool_invocation_id);
  if (it == continuations_.end()) {
    return;
  }
  std::unique_ptr<ContinuedToolExecution> continuation = std::move(it->second);
  continuations_.erase(it);
  if (continuation->HasCompletionCallback()) {
    continuation->Complete(base::unexpected(error_message));
  }
}

std::unique_ptr<ScriptToolExecutionRegistration>
RegisterExternalScriptToolExecution(
    RenderFrameHost& target_rfh,
    const blink::ToolInvocationToken& invocation_id,
    std::string tool_name,
    std::string input_arguments,
    ScriptToolContinuationTokenCreatedCallback on_token_created) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // `RequestContinuationToken()` rejects requests from documents that aren't
  // active or aren't in the primary page.
  if (!target_rfh.IsActive() ||
      !target_rfh.GetMainFrame()->IsInPrimaryMainFrame()) {
    return nullptr;
  }
  auto registration =
      std::make_unique<ScriptToolExecutionRegistrationImpl>(invocation_id);
  if (auto* continuation_manager =
          ScriptToolContinuationManager::GetOrCreateForRenderFrameHost(
              target_rfh)) {
    continuation_manager->RegisterExternalExecution(
        target_rfh, invocation_id, std::move(tool_name),
        std::move(input_arguments), std::move(on_token_created),
        registration->GetCompletionCallback());
    registration->set_manager(continuation_manager->GetWeakPtr());
  }
  return registration;
}

}  // namespace content
