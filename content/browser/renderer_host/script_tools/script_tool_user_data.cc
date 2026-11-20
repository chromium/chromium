// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/script_tools/script_tool_user_data.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/page_impl.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_host_manager.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/script_tool_continuation.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/message.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/script_tools/script_tool_utils.h"
#include "url/origin.h"

namespace content {

namespace {

bool IsScriptToolExposedToOrigin(
    const url::Origin& tool_owner_origin,
    const std::vector<url::Origin>& exposed_origins,
    const url::Origin& accessing_origin) {
  if (accessing_origin.IsSameOriginWith(tool_owner_origin)) {
    return true;
  }
  return std::ranges::contains(exposed_origins, accessing_origin);
}

bool IsScriptToolOwnerRequestedByOrigin(
    const url::Origin& tool_owner_origin,
    const url::Origin& caller_origin,
    const std::vector<url::Origin>& from_origins) {
  if (caller_origin.IsSameOriginWith(tool_owner_origin)) {
    return true;
  }
  return std::ranges::contains(from_origins, tool_owner_origin);
}

bool IsWebMCPEnabled(RenderFrameHost& rfh) {
  return base::FeatureList::IsEnabled(blink::features::kWebMCP) &&
         rfh.IsFeatureEnabled(network::mojom::PermissionsPolicyFeature::kTools);
}

blink::mojom::ExecuteRemoteScriptToolError ToExecuteRemoteScriptToolError(
    blink::mojom::ScriptToolErrorCode code) {
  switch (code) {
    case blink::mojom::ScriptToolErrorCode::kInvalidToolName:
      return blink::mojom::ExecuteRemoteScriptToolError::kNotFoundError;
    case blink::mojom::ScriptToolErrorCode::kInvalidInputArguments:
      return blink::mojom::ExecuteRemoteScriptToolError::kDataError;
    case blink::mojom::ScriptToolErrorCode::kMissingRequiredSubmitButton:
    case blink::mojom::ScriptToolErrorCode::kToolInvocationFailed:
      return blink::mojom::ExecuteRemoteScriptToolError::kOperationError;
    case blink::mojom::ScriptToolErrorCode::kToolCancelled:
      return blink::mojom::ExecuteRemoteScriptToolError::kAbortError;
  }
}

bool IsWebMCPContinuationsEnabled(RenderFrameHost& rfh) {
  return IsWebMCPEnabled(rfh) &&
         base::FeatureList::IsEnabled(blink::features::kWebMCPContinuations);
}

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
              ScriptToolUserData::GetForCurrentDocument(redeeming_rfh)) {
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
      ScriptToolUserData::GetForCurrentDocument(redeeming_rfh);
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

DOCUMENT_USER_DATA_KEY_IMPL(ScriptToolUserData);

ScriptToolUserData::ScriptToolUserData(RenderFrameHost* rfh)
    : DocumentUserData<ScriptToolUserData>(rfh) {}

// TODO(https://crbug.com/508285989): In the destructor, implement the implicit
// unregistering of all `script_tools_` and invoking `NotifyToolChange()` on any
// relevant documents. Right now, the destructor only supports the cancelling of
// pending tool execution in the calling renderer process, when the tool host's
// document is destroyed.
ScriptToolUserData::~ScriptToolUserData() {
  auto& rfh_impl = static_cast<RenderFrameHostImpl&>(render_frame_host());
  if (auto* continuation_manager =
          ScriptToolContinuationManager::GetForRenderFrameHost(
              render_frame_host())) {
    continuation_manager->OnDocumentDestroyed(rfh_impl.GetDocumentToken());
  }
  auto& page = render_frame_host().GetPage();
  auto* page_data = ScriptToolPageUserData::GetForPage(page);
  if (page_data) {
    page_data->CancelPendingScriptToolExecutionsForDocument(
        rfh_impl.GetDocumentToken());
  }
}

// static
void ScriptToolUserData::Bind(
    RenderFrameHost* rfh,
    mojo::PendingAssociatedReceiver<blink::mojom::ModelContextHost> receiver) {
  auto* user_data = ScriptToolUserData::GetForCurrentDocument(rfh);
  if (user_data) {
    // TODO(crbug.com/573662751): Migrate to mojo::ReportBadMessage.
    bad_message::ReceivedBadMessage(rfh->GetProcess(),
                                    bad_message::RFHI_WEBMCP_DUPLICATE_BIND);
    return;
  }
  user_data = ScriptToolUserData::GetOrCreateForCurrentDocument(rfh);
  user_data->receiver_.Bind(std::move(receiver));
}

void ScriptToolUserData::BindModelContext(
    mojo::PendingAssociatedRemote<blink::mojom::ModelContext> model_context) {
  if (model_context_remote_.is_bound()) {
    // TODO(crbug.com/573662751): Migrate to mojo::ReportBadMessage.
    bad_message::ReceivedBadMessage(
        render_frame_host().GetProcess(),
        bad_message::RFHI_WEBMCP_DUPLICATE_SET_RECEIVER);
    return;
  }
  model_context_remote_.Bind(std::move(model_context));
}

void ScriptToolUserData::RegisterScriptTool(
    blink::mojom::ScriptToolPtr tool,
    RegisterScriptToolCallback callback) {
  if (!IsWebMCPEnabled(render_frame_host())) {
    // TODO(crbug.com/573662751): Migrate to mojo::ReportBadMessage.
    bad_message::ReceivedBadMessage(render_frame_host().GetProcess(),
                                    bad_message::RFHI_WEBMCP_NOT_ENABLED);
    std::move(callback).Run();
    return;
  }

  // Kill the renderer if it tries to register a tool with an invalid name,
  // because the renderer should prevent this.
  if (!blink::IsValidScriptToolName(tool->name)) {
    // TODO(crbug.com/573662751): Migrate to mojo::ReportBadMessage.
    bad_message::ReceivedBadMessage(render_frame_host().GetProcess(),
                                    bad_message::RFHI_WEBMCP_INVALID_TOOL_NAME);
    std::move(callback).Run();
    return;
  }

  // Kill the renderer if it tries to register a tool without a `tool_id`, or
  // with a `tool_id` that is invalid or already registered in `this`. The
  // renderer mints this ID, but the browser verifies that it is valid and
  // unique.
  if (!tool->tool_id || !blink::IsValidScriptToolToken(*tool->tool_id) ||
      std::ranges::any_of(script_tools_,
                          [&](const blink::mojom::ScriptToolPtr& t) {
                            return t->tool_id == tool->tool_id;
                          })) {
    // TODO(crbug.com/573662751): Migrate to mojo::ReportBadMessage.
    bad_message::ReceivedBadMessage(
        render_frame_host().GetProcess(),
        bad_message::RFHI_WEBMCP_REGISTER_INVALID_TOOL_ID);
    std::move(callback).Run();
    return;
  }

  // Kill the renderer if it tries to register a duplicate tool name (*even* if
  // `tool_id` is unique), because the renderer should prevent this. Tools must
  // be unique in both ID and name within `this`.
  if (std::ranges::any_of(script_tools_,
                          [&](const blink::mojom::ScriptToolPtr& t) {
                            return t->name == tool->name;
                          })) {
    // TODO(crbug.com/573662751): Migrate to mojo::ReportBadMessage.
    bad_message::ReceivedBadMessage(
        render_frame_host().GetProcess(),
        bad_message::RFHI_WEBMCP_REGISTER_DUPLICATE_TOOL_NAME);
    std::move(callback).Run();
    return;
  }

  for (const auto& origin : tool->exposed_origins) {
    if (!network::IsOriginPotentiallyTrustworthy(origin)) {
      // TODO(crbug.com/573662751): Migrate to mojo::ReportBadMessage.
      bad_message::ReceivedBadMessage(
          render_frame_host().GetProcess(),
          bad_message::RFHI_WEBMCP_EXPOSED_UNTRUSTWORTHY_ORIGIN);
      std::move(callback).Run();
      return;
    }
  }

  std::vector<url::Origin> exposed_origins = tool->exposed_origins;
  script_tools_.push_back(std::move(tool));
  NotifyToolChange(exposed_origins);
  std::move(callback).Run();
}

void ScriptToolUserData::UnregisterScriptTool(
    const blink::ScriptToolToken& tool_id) {
  if (!IsWebMCPEnabled(render_frame_host())) {
    // TODO(crbug.com/573662751): Migrate to mojo::ReportBadMessage.
    bad_message::ReceivedBadMessage(render_frame_host().GetProcess(),
                                    bad_message::RFHI_WEBMCP_NOT_ENABLED);
    return;
  }

  auto it = std::find_if(script_tools_.begin(), script_tools_.end(),
                         [&](const blink::mojom::ScriptToolPtr& t) {
                           return t->tool_id == tool_id;
                         });
  if (it == script_tools_.end()) {
    // TODO(crbug.com/573662751): Migrate to mojo::ReportBadMessage.
    bad_message::ReceivedBadMessage(
        render_frame_host().GetProcess(),
        bad_message::RFHI_WEBMCP_UNREGISTER_INVALID_TOOL_ID);
    return;
  }

  std::vector<url::Origin> exposed_origins = (*it)->exposed_origins;

  script_tools_.erase(it);

  NotifyToolChange(exposed_origins);
}

void ScriptToolUserData::GetScriptTools(
    const std::vector<url::Origin>& from_origins,
    GetScriptToolsCallback callback) {
  if (!IsWebMCPEnabled(render_frame_host())) {
    // TODO(crbug.com/573662751): Migrate to mojo::ReportBadMessage.
    bad_message::ReceivedBadMessage(render_frame_host().GetProcess(),
                                    bad_message::RFHI_WEBMCP_NOT_ENABLED);
    std::move(callback).Run({});
    return;
  }

  const url::Origin& caller_origin =
      render_frame_host().GetLastCommittedOrigin();

  std::vector<blink::mojom::ExposedScriptToolPtr> all_tools;
  RenderFrameHost* main_frame = render_frame_host().GetMainFrame();
  SiteInstanceGroup* site_instance_group =
      static_cast<SiteInstanceImpl*>(render_frame_host().GetSiteInstance())
          ->group();
  main_frame->ForEachRenderFrameHostWithAction([&](RenderFrameHost* rfh) {
    if (rfh->GetMainFrame() != main_frame) {
      return RenderFrameHost::FrameIterationAction::kSkipChildren;
    }

    if (!rfh->IsFeatureEnabled(
            network::mojom::PermissionsPolicyFeature::kTools)) {
      return RenderFrameHost::FrameIterationAction::kContinue;
    }

    const url::Origin& tool_owner_origin = rfh->GetLastCommittedOrigin();
    if (!IsScriptToolOwnerRequestedByOrigin(tool_owner_origin, caller_origin,
                                            from_origins)) {
      return RenderFrameHost::FrameIterationAction::kContinue;
    }

    auto* data = ScriptToolUserData::GetForCurrentDocument(rfh);
    if (!data) {
      return RenderFrameHost::FrameIterationAction::kContinue;
    }

    const auto& local_tools = data->script_tools();
    for (const auto& t : local_tools) {
      if (!IsScriptToolExposedToOrigin(tool_owner_origin, t->exposed_origins,
                                       /*accessing_origin=*/caller_origin)) {
        continue;
      }

      blink::mojom::ScriptToolPtr exposed_tool = t.Clone();
      // Find the frame (it could be local or remote) that the caller can use
      // to reference the `Window` hosting the tool.
      //
      // `token` will never be `nullopt`.
      blink::FrameToken token =
          *static_cast<RenderFrameHostImpl*>(rfh)
               ->frame_tree_node()
               ->render_manager()
               ->GetFrameTokenForSiteInstanceGroup(site_instance_group);

      // Clear `exposed_origins` to prevent leaking the full list of
      // authorized origins to the calling renderer process.
      exposed_tool->exposed_origins.clear();

      all_tools.push_back(blink::mojom::ExposedScriptTool::New(
          std::move(exposed_tool), token, tool_owner_origin));
    }
    return RenderFrameHost::FrameIterationAction::kContinue;
  });

  std::ranges::sort(all_tools, [](const blink::mojom::ExposedScriptToolPtr& a,
                                  const blink::mojom::ExposedScriptToolPtr& b) {
    return a->tool->name < b->tool->name;
  });

  std::move(callback).Run(std::move(all_tools));
}

void ScriptToolUserData::ExecuteRemoteScriptTool(
    const blink::ToolInvocationToken& invocation_id,
    const blink::ScriptToolToken& tool_id,
    const blink::FrameToken& tool_owner_frame_token,
    const url::Origin& expected_target_origin,
    const std::string& input_arguments,
    ExecuteRemoteScriptToolCallback callback) {
  if (!IsWebMCPEnabled(render_frame_host())) {
    // TODO(crbug.com/573662751): Migrate to mojo::ReportBadMessage.
    bad_message::ReceivedBadMessage(render_frame_host().GetProcess(),
                                    bad_message::RFHI_WEBMCP_NOT_ENABLED);
    std::move(callback).Run(
        std::nullopt,
        blink::mojom::ExecuteRemoteScriptToolError::kInvalidStateError);
    return;
  }

  auto* existing_page_data =
      ScriptToolPageUserData::GetForPage(render_frame_host().GetPage());
  if (!blink::IsValidScriptToolToken(invocation_id) ||
      (existing_page_data &&
       existing_page_data->HasPendingScriptToolExecution(invocation_id))) {
    // TODO(crbug.com/573662751): Migrate to mojo::ReportBadMessage.
    bad_message::ReceivedBadMessage(
        render_frame_host().GetProcess(),
        bad_message::RFHI_WEBMCP_INVALID_INVOCATION_ID);
    std::move(callback).Run(
        std::nullopt,
        blink::mojom::ExecuteRemoteScriptToolError::kInvalidStateError);
    return;
  }

  // Renderers mint a new `invocation_id` for each tool execution, so reusing
  // one is a bad message. Check for an existing continuation explicitly: a
  // continuation can outlive its tool execution (e.g. an unredeemed token stays
  // redeemable after the caller cancels). Otherwise, the target's
  // `RequestContinuationToken()` would find that continuation and kill the
  // target's renderer instead of the caller's. Also check for a registered
  // browser-initiated execution, which `RequestContinuationToken()` would
  // otherwise ignore in favor of this execution.
  if (auto* continuation_manager =
          ScriptToolContinuationManager::GetForRenderFrameHost(
              render_frame_host());
      continuation_manager &&
      continuation_manager->IsInvocationIdInUse(invocation_id)) {
    // TODO(crbug.com/573662751): Migrate to mojo::ReportBadMessage.
    bad_message::ReceivedBadMessage(
        render_frame_host().GetProcess(),
        bad_message::RFHI_WEBMCP_INVALID_TOOL_CONTINUATION);
    std::move(callback).Run(
        std::nullopt,
        blink::mojom::ExecuteRemoteScriptToolError::kInvalidStateError);
    return;
  }

  RenderFrameHostImpl* target_rfh = nullptr;

  if (tool_owner_frame_token.Is<blink::LocalFrameToken>()) {
    target_rfh = static_cast<RenderFrameHostImpl*>(
        RenderFrameHost::FromFrameToken(GlobalRenderFrameHostToken(
            render_frame_host().GetProcess()->GetDeprecatedID(),
            tool_owner_frame_token.GetAs<blink::LocalFrameToken>())));
  } else {
    target_rfh =
        static_cast<RenderFrameHostImpl*>(RenderFrameHost::FromPlaceholderToken(
            render_frame_host().GetProcess()->GetDeprecatedID(),
            tool_owner_frame_token.GetAs<blink::RemoteFrameToken>()));
  }

  // Don't kill the renderer, since legitimate script can target a document that
  // no longer exists, or simply target the *wrong* document. It can also target
  // a document in another frame tree, but at the moment we do not support
  // cross-frame-tree, same-browsing-context-group tool execution, hence the
  // main-frame check below.
  RenderFrameHost* main_frame = render_frame_host().GetMainFrame();
  if (!target_rfh || main_frame != target_rfh->GetMainFrame()) {
    std::move(callback).Run(
        std::nullopt,
        blink::mojom::ExecuteRemoteScriptToolError::kInvalidStateError);
    return;
  }

  // Verify that `target_rfh`'s origin matches the origin that the tool invoker
  // expects the tool to run in. This is necessary because it is possible that
  // the `tool_owner_frame_token` points to a `RenderFrameProxyHost` whose frame
  // tree node's `current_frame_host()` has changed since tool registration.
  if (!target_rfh->GetLastCommittedOrigin().IsSameOriginWith(
          expected_target_origin)) {
    std::move(callback).Run(
        std::nullopt,
        blink::mojom::ExecuteRemoteScriptToolError::kNotFoundError);
    return;
  }

  auto* target_data = ScriptToolUserData::GetForCurrentDocument(target_rfh);
  // Don't kill the renderer, since legitimate script can target the wrong
  // document, including one that has never touched its `modelContext` accessor,
  // and therefore does not have `target_data`. However, if our API ever moves
  // to opaque "Tool" interface that internally encapsulates a tool host's frame
  // token instead of making JavaScript pass it in via a `Window` argument, then
  // we'd be able to terminate the renderer here, since it would not be possible
  // for a legitimate renderer to supply a frame token targeting a frame host
  // that exists, but does not have `target_data`.
  if (!target_data) {
    std::move(callback).Run(
        std::nullopt,
        blink::mojom::ExecuteRemoteScriptToolError::kNotFoundError);
    return;
  }

  auto& tools = target_data->script_tools();
  auto it = std::find_if(tools.begin(), tools.end(),
                         [&](const blink::mojom::ScriptToolPtr& t) {
                           return t->tool_id == tool_id;
                         });

  // Don't kill the renderer, since legitimate script can provide the id of a
  // tool that doesn't exist (including one that did exist, but got unregistered
  // by the time the call to invoke it here).
  if (it == tools.end()) {
    std::move(callback).Run(
        std::nullopt,
        blink::mojom::ExecuteRemoteScriptToolError::kNotFoundError);
    return;
  }

  // Don't kill the renderer here, since legitimate script can target a tool in
  // another document that it might have been told about, but technically cannot
  // access.
  if (!IsScriptToolExposedToOrigin(
          /*tool_owner_origin=*/target_rfh->GetLastCommittedOrigin(),
          (*it)->exposed_origins,
          /*accessing_origin=*/render_frame_host().GetLastCommittedOrigin())) {
    std::move(callback).Run(
        std::nullopt,
        blink::mojom::ExecuteRemoteScriptToolError::kNotFoundError);
    return;
  }

  // At this point, it is safe to invoke the tool in the target renderer pointed
  // to by `target_data`.
  ScriptToolPageUserData* page_data =
      ScriptToolPageUserData::GetOrCreateForPage(target_rfh->GetPage());
  ScriptToolPageUserData::PendingScriptToolExecution execution;
  execution.caller_token =
      static_cast<RenderFrameHostImpl&>(render_frame_host()).GetDocumentToken();
  execution.target_token = target_rfh->GetDocumentToken();
  execution.target_document = target_rfh->GetWeakDocumentPtr();
  execution.tool_name = (*it)->name;
  execution.input_arguments = input_arguments;
  execution.callback = std::move(callback);
  page_data->AddPendingScriptToolExecution(invocation_id, std::move(execution));

  target_data->model_context_remote_->ExecuteScriptTool(
      invocation_id, tool_id, input_arguments,
      base::BindOnce(
          [](base::WeakPtr<ScriptToolPageUserData> page_data,
             blink::ToolInvocationToken invocation_id,
             const std::optional<std::string>& result,
             std::optional<blink::mojom::ScriptToolErrorCode> error_code) {
            if (page_data) {
              auto error =
                  error_code ? std::optional(
                                   ToExecuteRemoteScriptToolError(*error_code))
                             : std::nullopt;
              page_data->CompletePendingScriptToolExecution(invocation_id,
                                                            result, error);
            }
          },
          page_data->GetWeakPtr(), invocation_id));
}

void ScriptToolUserData::CancelRemoteScriptTool(
    const blink::ToolInvocationToken& invocation_id) {
  if (!IsWebMCPEnabled(render_frame_host())) {
    // TODO(crbug.com/573662751): Migrate to mojo::ReportBadMessage.
    bad_message::ReceivedBadMessage(render_frame_host().GetProcess(),
                                    bad_message::RFHI_WEBMCP_NOT_ENABLED);
    return;
  }
  if (!blink::IsValidScriptToolToken(invocation_id)) {
    // TODO(crbug.com/573662751): Migrate to mojo::ReportBadMessage.
    bad_message::ReceivedBadMessage(
        render_frame_host().GetProcess(),
        bad_message::RFHI_WEBMCP_INVALID_INVOCATION_ID);
    return;
  }
  if (ScriptToolPageUserData* page_data =
          ScriptToolPageUserData::GetForPage(render_frame_host().GetPage())) {
    page_data->CancelPendingScriptToolExecution(invocation_id);
  }
}

void ScriptToolUserData::SetScriptToolsReady(bool ready) {
  if (!IsWebMCPEnabled(render_frame_host()) ||
      !base::FeatureList::IsEnabled(
          blink::features::kWebMCPWaitForToolsUntil)) {
    mojo::ReportBadMessage("WebMCPWaitForToolsUntil is not enabled.");
    return;
  }
  auto& rfh = static_cast<RenderFrameHostImpl&>(render_frame_host());
  const auto previous_state = rfh.GetScriptToolsReadyState();
  script_tools_waiting_ = !ready;
  if (previous_state != rfh.GetScriptToolsReadyState()) {
    rfh.delegate()->ScriptToolsReadyStateChanged(&rfh);
  }
}

void ScriptToolUserData::RequestContinuationToken(
    const blink::ToolInvocationToken& invocation_id,
    RequestContinuationTokenCallback callback) {
  auto* continuation_manager =
      ScriptToolContinuationManager::GetOrCreateForRenderFrameHost(
          render_frame_host());
  if (!continuation_manager) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  std::move(callback).Run(continuation_manager->RequestContinuationToken(
      render_frame_host(), invocation_id));
}

void ScriptToolUserData::RedeemContinuationToken(
    const blink::ContinuationToken& token,
    RedeemContinuationTokenCallback callback) {
  // Check this before looking up `continuation_manager`, which doesn't exist
  // until a document requests a continuation token.
  if (!IsWebMCPContinuationsEnabled(render_frame_host())) {
    // TODO(crbug.com/573662751): Migrate to mojo::ReportBadMessage.
    bad_message::ReceivedBadMessage(
        render_frame_host().GetProcess(),
        bad_message::RFHI_WEBMCP_INVALID_TOOL_CONTINUATION);
    std::move(callback).Run(false);
    return;
  }
  auto* continuation_manager =
      ScriptToolContinuationManager::GetForRenderFrameHost(render_frame_host());
  if (!continuation_manager) {
    std::move(callback).Run(false);
    return;
  }
  continuation_manager->RedeemContinuationToken(render_frame_host(), token,
                                                std::move(callback));
}

void ScriptToolUserData::NotifyToolChange(
    const std::vector<url::Origin>& exposed_origins) {
  RenderFrameHost& rfh = render_frame_host();
  url::Origin tool_owner_origin = rfh.GetLastCommittedOrigin();

  RenderFrameHost* main_frame = rfh.GetMainFrame();
  main_frame->ForEachRenderFrameHostWithAction([&](RenderFrameHost* frame) {
    if (frame->GetMainFrame() != main_frame) {
      return RenderFrameHost::FrameIterationAction::kSkipChildren;
    }

    if (!frame->IsFeatureEnabled(
            network::mojom::PermissionsPolicyFeature::kTools)) {
      return RenderFrameHost::FrameIterationAction::kContinue;
    }

    if (IsScriptToolExposedToOrigin(
            tool_owner_origin, exposed_origins,
            /*accessing_origin=*/frame->GetLastCommittedOrigin())) {
      auto* data = ScriptToolUserData::GetForCurrentDocument(frame);
      if (!data) {
        return RenderFrameHost::FrameIterationAction::kContinue;
      }

      if (data->model_context_remote_.is_bound()) {
        data->model_context_remote_->NotifyToolChange();
      }
    }
    return RenderFrameHost::FrameIterationAction::kContinue;
  });
}

PAGE_USER_DATA_KEY_IMPL(ScriptToolPageUserData);

ScriptToolPageUserData::PendingScriptToolExecution::
    PendingScriptToolExecution() = default;
ScriptToolPageUserData::PendingScriptToolExecution::PendingScriptToolExecution(
    PendingScriptToolExecution&&) = default;
ScriptToolPageUserData::PendingScriptToolExecution&
ScriptToolPageUserData::PendingScriptToolExecution::operator=(
    PendingScriptToolExecution&&) = default;
ScriptToolPageUserData::PendingScriptToolExecution::
    ~PendingScriptToolExecution() = default;

ScriptToolPageUserData::ScriptToolPageUserData(Page& page)
    : PageUserData<ScriptToolPageUserData>(page) {}

ScriptToolPageUserData::~ScriptToolPageUserData() = default;

bool ScriptToolPageUserData::HasPendingScriptToolExecution(
    const blink::ToolInvocationToken& invocation_id) const {
  return pending_script_tool_executions_.contains(invocation_id);
}

void ScriptToolPageUserData::AddPendingScriptToolExecution(
    const blink::ToolInvocationToken& invocation_id,
    PendingScriptToolExecution execution) {
  pending_script_tool_executions_.emplace(invocation_id, std::move(execution));
}

const ScriptToolPageUserData::PendingScriptToolExecution*
ScriptToolPageUserData::FindPendingScriptToolExecution(
    const blink::ToolInvocationToken& invocation_id) const {
  auto it = pending_script_tool_executions_.find(invocation_id);
  return it != pending_script_tool_executions_.end() ? &it->second : nullptr;
}

void ScriptToolPageUserData::OnContinuationTokenCreated(
    ScriptToolContinuationManager& continuation_manager,
    const blink::ToolInvocationToken& invocation_id,
    const blink::ContinuationToken& token) {
  auto it = pending_script_tool_executions_.find(invocation_id);
  // Only called by `RequestContinuationToken()`, which just found this pending
  // execution.
  CHECK(it != pending_script_tool_executions_.end());
  it->second.awaiting_continuation = true;
  continuation_manager.WaitForContinuation(
      token,
      base::BindOnce(
          [](base::WeakPtr<ScriptToolPageUserData> self,
             blink::ToolInvocationToken invocation_id,
             base::expected<std::string, std::string> cont_result,
             WeakDocumentPtr redeeming_document) {
            if (!self) {
              return;
            }
            auto pending_it =
                self->pending_script_tool_executions_.find(invocation_id);
            if (pending_it == self->pending_script_tool_executions_.end()) {
              return;
            }
            auto callback = std::move(pending_it->second.callback);
            self->pending_script_tool_executions_.erase(pending_it);
            if (cont_result.has_value()) {
              std::move(callback).Run(*cont_result, std::nullopt);
            } else {
              std::move(callback).Run(
                  std::nullopt,
                  blink::mojom::ExecuteRemoteScriptToolError::kOperationError);
            }
          },
          GetWeakPtr(), invocation_id));
}

void ScriptToolPageUserData::CompletePendingScriptToolExecution(
    const blink::ToolInvocationToken& invocation_id,
    const std::optional<std::string>& result,
    std::optional<blink::mojom::ExecuteRemoteScriptToolError> error) {
  // We are here because a tool just returned a result, and we should forward
  // the result to the tool's invoker. But it's possible that no pending
  // execution exists for this tool anymore. This can happen if the invoker
  // document is destroyed before the response from the renderer hosting the
  // tool comes back, or if a continuation step already completed.
  //
  // Conversely, if the document *hosting* a tool gets destroyed before it
  // responds with the result of the tool the associated pending execution is
  // *also* removed from `this`, but the mojo receiver is terminated before this
  // `CompletePendingScriptToolExecution()` callback is ever run, so we'll never
  // end up here.
  auto it = pending_script_tool_executions_.find(invocation_id);
  if (it == pending_script_tool_executions_.end()) {
    return;
  }

  // Once the initial step has minted a continuation token, the continuation
  // chain determines the final outcome of the tool execution regardless of how
  // the initial step's `execute()` Promise settled.
  if (it->second.awaiting_continuation) {
    return;
  }

  std::move(it->second.callback).Run(result, error);
  pending_script_tool_executions_.erase(it);
}

void ScriptToolPageUserData::SendCancelScriptToolToTarget(
    WeakDocumentPtr target_document,
    const blink::ToolInvocationToken& invocation_id) {
  RenderFrameHost* target_rfh = target_document.AsRenderFrameHostIfValid();
  if (!target_rfh) {
    return;
  }
  auto* target_data = ScriptToolUserData::GetForCurrentDocument(target_rfh);
  if (!target_data || !target_data->model_context_remote_.is_bound()) {
    return;
  }
  target_data->model_context_remote_->CancelScriptTool(invocation_id);
}

void ScriptToolPageUserData::CancelPendingScriptToolExecution(
    const blink::ToolInvocationToken& invocation_id) {
  // We are here because a caller document aborted its execution signal for a
  // tool. However, it is possible that no pending execution exists for this
  // `invocation_id` anymore. This can happen in legitimate, non-compromised
  // renderer races:
  //   1. The tool execution completed and sent its response IPC
  //      (`CompletePendingScriptToolExecution()`) before this cancellation
  //      IPC arrived.
  //   2. The document hosting the tool was destroyed
  //      (`CancelPendingScriptToolExecutionsForDocument()`) before this
  //      cancellation IPC arrived.
  auto it = pending_script_tool_executions_.find(invocation_id);
  if (it == pending_script_tool_executions_.end()) {
    return;
  }
  PendingScriptToolExecution execution = std::move(it->second);
  pending_script_tool_executions_.erase(it);

  // If the initial tool step has already settled, `target_document` ignores
  // this, because `invocation_id` is no longer pending there.
  SendCancelScriptToolToTarget(execution.target_document, invocation_id);
  // If the tool created a continuation token, cancel any actively executing
  // resumed continuation step while leaving any unredeemed token redeemable.
  if (auto* continuation_manager =
          ScriptToolContinuationManager::GetForRenderFrameHost(
              page().GetMainDocument())) {
    if (std::optional<blink::ContinuationToken> token =
            continuation_manager->GetContinuationToken(invocation_id)) {
      continuation_manager->CancelContinuation(*token);
    }
  }
  if (execution.callback) {
    std::move(execution.callback)
        .Run(std::nullopt,
             blink::mojom::ExecuteRemoteScriptToolError::kAbortError);
  }
}

void ScriptToolPageUserData::CancelPendingScriptToolExecutionsForDocument(
    const blink::DocumentToken& document_token) {
  ScriptToolContinuationManager* continuation_manager =
      ScriptToolContinuationManager::GetForRenderFrameHost(
          page().GetMainDocument());

  for (auto it = pending_script_tool_executions_.begin();
       it != pending_script_tool_executions_.end();) {
    const bool is_caller = it->second.caller_token == document_token;
    const bool is_target = it->second.target_token == document_token;
    if (!is_caller && !is_target) {
      ++it;
      continue;
    }

    const blink::ToolInvocationToken invocation_id = it->first;
    const std::optional<blink::ContinuationToken> continuation_token =
        continuation_manager
            ? continuation_manager->GetContinuationToken(invocation_id)
            : std::nullopt;

    // If the target document is unloading after minting a continuation token
    // (and the caller document is still alive), allow the continuation chain
    // to proceed across the target's navigation or unload.
    if (is_target && !is_caller && it->second.awaiting_continuation) {
      ++it;
      continue;
    }

    PendingScriptToolExecution execution = std::move(it->second);
    it = pending_script_tool_executions_.erase(it);

    // If the now-destructing document represented by `document_token` owns the
    // tool, then it's too late to send a cancellation IPC to it. During
    // document destruction, document weak pointers like `target_document` below
    // are still valid, but they point to a `RenderFrameHostImpl` whose user
    // data (thus `ScriptToolUserData`) has already been cleared. So it's
    // impossible to grab a remote to the renderer and send the IPC.
    if (!is_target) {
      SendCancelScriptToolToTarget(execution.target_document, invocation_id);
    }
    if (continuation_manager && continuation_token.has_value()) {
      continuation_manager->CancelContinuation(*continuation_token);
    }
    if (execution.callback) {
      std::move(execution.callback)
          .Run(std::nullopt,
               blink::mojom::ExecuteRemoteScriptToolError::kAbortError);
    }
  }
}

base::WeakPtr<ScriptToolPageUserData> ScriptToolPageUserData::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
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
