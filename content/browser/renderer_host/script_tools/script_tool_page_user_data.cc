// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/script_tools/script_tool_page_user_data.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/types/expected.h"
#include "content/browser/renderer_host/script_tools/script_tool_continuation_manager.h"
#include "content/browser/renderer_host/script_tools/script_tool_document_user_data.h"
#include "content/public/browser/render_frame_host.h"

namespace content {

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
  auto* target_data =
      ScriptToolDocumentUserData::GetForCurrentDocument(target_rfh);
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
    // data (thus `ScriptToolDocumentUserData`) has already been cleared. So
    // it's impossible to grab a remote to the renderer and send the IPC.
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

}  // namespace content
