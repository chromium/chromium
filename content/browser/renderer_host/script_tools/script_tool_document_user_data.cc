// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/script_tools/script_tool_document_user_data.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_host_manager.h"
#include "content/browser/renderer_host/script_tools/script_tool_continuation_manager.h"
#include "content/browser/renderer_host/script_tools/script_tool_page_user_data.h"
#include "content/browser/site_instance_impl.h"
#include "mojo/public/cpp/bindings/message.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
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

}  // namespace

DOCUMENT_USER_DATA_KEY_IMPL(ScriptToolDocumentUserData);

// static
bool ScriptToolDocumentUserData::IsWebMCPEnabled(RenderFrameHost& rfh) {
  return base::FeatureList::IsEnabled(blink::features::kWebMCP) &&
         rfh.IsFeatureEnabled(network::mojom::PermissionsPolicyFeature::kTools);
}

ScriptToolDocumentUserData::ScriptToolDocumentUserData(RenderFrameHost* rfh)
    : DocumentUserData<ScriptToolDocumentUserData>(rfh) {}

// TODO(https://crbug.com/508285989): In the destructor, implement the implicit
// unregistering of all `script_tools_` and invoking `NotifyToolChange()` on any
// relevant documents. Right now, the destructor only supports the cancelling of
// pending tool execution in the calling renderer process, when the tool host's
// document is destroyed.
ScriptToolDocumentUserData::~ScriptToolDocumentUserData() {
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
void ScriptToolDocumentUserData::Bind(
    RenderFrameHost* rfh,
    mojo::PendingAssociatedReceiver<blink::mojom::ModelContextHost> receiver) {
  auto* user_data = ScriptToolDocumentUserData::GetForCurrentDocument(rfh);
  if (user_data) {
    // TODO(crbug.com/573662751): Migrate to mojo::ReportBadMessage.
    bad_message::ReceivedBadMessage(rfh->GetProcess(),
                                    bad_message::RFHI_WEBMCP_DUPLICATE_BIND);
    return;
  }
  user_data = ScriptToolDocumentUserData::GetOrCreateForCurrentDocument(rfh);
  user_data->receiver_.Bind(std::move(receiver));
}

void ScriptToolDocumentUserData::BindModelContext(
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

void ScriptToolDocumentUserData::RegisterScriptTool(
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

void ScriptToolDocumentUserData::UnregisterScriptTool(
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

void ScriptToolDocumentUserData::GetScriptTools(
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

    auto* data = ScriptToolDocumentUserData::GetForCurrentDocument(rfh);
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

void ScriptToolDocumentUserData::ExecuteRemoteScriptTool(
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

  auto* target_data =
      ScriptToolDocumentUserData::GetForCurrentDocument(target_rfh);
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

void ScriptToolDocumentUserData::CancelRemoteScriptTool(
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

void ScriptToolDocumentUserData::SetScriptToolsReady(bool ready) {
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

void ScriptToolDocumentUserData::RequestContinuationToken(
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

void ScriptToolDocumentUserData::RedeemContinuationToken(
    const blink::ContinuationToken& token,
    RedeemContinuationTokenCallback callback) {
  // Check this before looking up `continuation_manager`, which doesn't exist
  // until a document requests a continuation token.
  if (!ScriptToolContinuationManager::IsWebMCPContinuationsEnabled(
          render_frame_host())) {
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

void ScriptToolDocumentUserData::NotifyToolChange(
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
      auto* data = ScriptToolDocumentUserData::GetForCurrentDocument(frame);
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

}  // namespace content
