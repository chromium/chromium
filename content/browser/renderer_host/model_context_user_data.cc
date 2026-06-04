// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/model_context_user_data.h"

#include <algorithm>

#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/page_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_host_manager.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"

namespace content {

namespace {

bool IsScriptToolVisibleToOrigin(
    const url::Origin& tool_owner_origin,
    const std::vector<url::Origin>& exposed_origins,
    const url::Origin& target_origin) {
  if (target_origin.IsSameOriginWith(tool_owner_origin)) {
    return true;
  }
  for (const auto& allowed_origin : exposed_origins) {
    if (target_origin.IsSameOriginWith(allowed_origin)) {
      return true;
    }
  }
  return false;
}

bool IsScriptToolRequestedByOrigin(
    const url::Origin& tool_owner_origin,
    const url::Origin& caller_origin,
    const std::vector<url::Origin>& from_origins) {
  if (tool_owner_origin.IsSameOriginWith(caller_origin)) {
    return true;
  }
  return std::ranges::contains(from_origins, tool_owner_origin);
}

bool IsWebMCPEnabled(RenderFrameHost& rfh) {
  // In the renderer, the WebMCP feature is implied by WebMCPTesting (in
  // runtime_enabled_features.json5). Since this implication does not propagate
  // automatically to the browser's base::FeatureList, we must explicitly check
  // both features here to prevent renderer termination (bad IPC message).
  return (base::FeatureList::IsEnabled(blink::features::kWebMCP) ||
          base::FeatureList::IsEnabled(blink::features::kWebMCPTesting)) &&
         rfh.IsFeatureEnabled(network::mojom::PermissionsPolicyFeature::kTools);
}

}  // namespace

DOCUMENT_USER_DATA_KEY_IMPL(ModelContextUserData);

ModelContextUserData::ModelContextUserData(RenderFrameHost* rfh)
    : DocumentUserData<ModelContextUserData>(rfh) {}

// TODO(https://crbug.com/508285989): In the destructor, implement the implicit
// unregistering of all `script_tools_` and invoking `NotifyToolChange()` on any
// relevant documents. Right now, the destructor only supports the cancelling of
// pending tool execution in the calling renderer process, when the tool host's
// document is destroyed.
ModelContextUserData::~ModelContextUserData() {
  auto& page = render_frame_host().GetPage();
  auto* page_data = ModelContextPageUserData::GetForPage(page);
  if (page_data) {
    auto& rfh_impl = static_cast<RenderFrameHostImpl&>(render_frame_host());
    page_data->CancelPendingScriptToolExecutionsForDocument(
        rfh_impl.GetDocumentToken());
  }
}

// static
void ModelContextUserData::Bind(
    RenderFrameHost* rfh,
    mojo::PendingReceiver<blink::mojom::ModelContextHost> receiver) {
  auto* user_data = ModelContextUserData::GetForCurrentDocument(rfh);
  if (user_data) {
    bad_message::ReceivedBadMessage(rfh->GetProcess(),
                                    bad_message::RFHI_WEBMCP_DUPLICATE_BIND);
    return;
  }
  user_data = ModelContextUserData::GetOrCreateForCurrentDocument(rfh);
  user_data->receiver_.Bind(std::move(receiver));
}

void ModelContextUserData::BindModelContext(
    mojo::PendingRemote<blink::mojom::ModelContext> model_context) {
  if (model_context_remote_.is_bound()) {
    bad_message::ReceivedBadMessage(
        render_frame_host().GetProcess(),
        bad_message::RFHI_WEBMCP_DUPLICATE_SET_RECEIVER);
    return;
  }
  model_context_remote_.Bind(std::move(model_context));
}

void ModelContextUserData::RegisterScriptTool(
    blink::mojom::ScriptToolPtr tool) {
  if (!IsWebMCPEnabled(render_frame_host())) {
    bad_message::ReceivedBadMessage(render_frame_host().GetProcess(),
                                    bad_message::RFHI_WEBMCP_NOT_ENABLED);
    return;
  }

  // Kill the renderer if it tries to register a duplicate tool name, because
  // the renderer should prevent this.
  auto it = std::find_if(script_tools_.begin(), script_tools_.end(),
                         [&](const blink::mojom::ScriptToolPtr& t) {
                           return t->name == tool->name;
                         });
  if (it != script_tools_.end()) {
    bad_message::ReceivedBadMessage(
        render_frame_host().GetProcess(),
        bad_message::RFHI_WEBMCP_REGISTER_DUPLICATE_TOOL_NAME);
    return;
  }

  for (const auto& origin : tool->exposed_origins) {
    if (origin.scheme() != url::kHttpsScheme) {
      bad_message::ReceivedBadMessage(
          render_frame_host().GetProcess(),
          bad_message::RFHI_WEBMCP_EXPOSED_NON_HTTPS_ORIGIN);
      return;
    }
  }

  // TOOD(https://crbug.com/509568047): Stop passing in a frame token and origin
  // during tool registration. These values are obvious from context, and the
  // renderer shouldn't need to pass them in and have the browser verify them.
  // Instead, the browser should be setting them for the first time here, with
  // no input from the renderer.
  if (tool->tool_owner_frame_token != render_frame_host().GetFrameToken() ||
      tool->origin != render_frame_host().GetLastCommittedOrigin()) {
    bad_message::ReceivedBadMessage(
        render_frame_host().GetProcess(),
        bad_message::RFHI_WEBMCP_INVALID_TOOL_OWNER);
    return;
  }

  std::vector<url::Origin> exposed_origins = tool->exposed_origins;
  script_tools_.push_back(std::move(tool));
  NotifyToolChange(exposed_origins);
}

void ModelContextUserData::UnregisterScriptTool(const std::string& name) {
  if (!IsWebMCPEnabled(render_frame_host())) {
    bad_message::ReceivedBadMessage(render_frame_host().GetProcess(),
                                    bad_message::RFHI_WEBMCP_NOT_ENABLED);
    return;
  }

  auto it = std::find_if(
      script_tools_.begin(), script_tools_.end(),
      [&](const blink::mojom::ScriptToolPtr& t) { return t->name == name; });
  if (it == script_tools_.end()) {
    bad_message::ReceivedBadMessage(render_frame_host().GetProcess(),
                                    bad_message::RFHI_WEBMCP_UNKNOWN_TOOL_NAME);
    return;
  }

  // Cancel all pending executions of the tool. For now this only include
  // notifying the caller that tool execution has failed, but see the
  // documentation above the
  // `CancelPendingScriptToolExecutionsDueToUnregistration()` declaration about
  // notifying the tool itself.
  auto& page = render_frame_host().GetPage();
  auto* page_data = ModelContextPageUserData::GetForPage(page);
  if (page_data) {
    auto& rfh_impl = static_cast<RenderFrameHostImpl&>(render_frame_host());
    page_data->CancelPendingScriptToolExecutionsDueToUnregistration(
        rfh_impl.GetDocumentToken(), name);
  }

  std::vector<url::Origin> exposed_origins = (*it)->exposed_origins;

  script_tools_.erase(it);

  NotifyToolChange(exposed_origins);
}

void ModelContextUserData::GetScriptTools(
    const std::vector<url::Origin>& from_origins,
    GetScriptToolsCallback callback) {
  if (!IsWebMCPEnabled(render_frame_host())) {
    bad_message::ReceivedBadMessage(render_frame_host().GetProcess(),
                                    bad_message::RFHI_WEBMCP_NOT_ENABLED);
    std::move(callback).Run({});
    return;
  }

  const url::Origin& caller_origin =
      render_frame_host().GetLastCommittedOrigin();

  std::vector<blink::mojom::ScriptToolPtr> all_tools;
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

    auto* data = ModelContextUserData::GetForCurrentDocument(rfh);
    if (!data) {
      return RenderFrameHost::FrameIterationAction::kContinue;
    }

    const auto& local_tools = data->script_tools();
    const url::Origin& tool_owner_origin = rfh->GetLastCommittedOrigin();
    for (const auto& t : local_tools) {
      if (IsScriptToolVisibleToOrigin(tool_owner_origin, t->exposed_origins,
                                      caller_origin) &&
          IsScriptToolRequestedByOrigin(tool_owner_origin, caller_origin,
                                        from_origins)) {
        blink::mojom::ScriptToolPtr cloned_tool = t.Clone();
        // Find the frame (it could be local or remote) that the caller can use
        // to reference the `Window` hosting the tool.
        //
        // `token` will never be `nullopt`.
        blink::FrameToken token =
            *static_cast<RenderFrameHostImpl*>(rfh)
                 ->frame_tree_node()
                 ->render_manager()
                 ->GetFrameTokenForSiteInstanceGroup(site_instance_group);

        cloned_tool->tool_owner_frame_token = token;
        all_tools.push_back(std::move(cloned_tool));
      }
    }
    return RenderFrameHost::FrameIterationAction::kContinue;
  });

  std::move(callback).Run(std::move(all_tools));
}

void ModelContextUserData::ExecuteRemoteScriptTool(
    const blink::FrameToken& tool_owner_frame_token,
    const url::Origin& expected_target_origin,
    const std::string& name,
    const std::string& input_arguments,
    ExecuteRemoteScriptToolCallback callback) {
  if (!IsWebMCPEnabled(render_frame_host())) {
    bad_message::ReceivedBadMessage(render_frame_host().GetProcess(),
                                    bad_message::RFHI_WEBMCP_NOT_ENABLED);
    std::move(callback).Run(std::nullopt, false);
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
    std::move(callback).Run(std::nullopt, false);
    return;
  }

  // Verify that `target_rfh`'s origin matches the origin that the tool invoker
  // expects the tool to run in. This is necessary because it is possible that
  // the `tool_owner_frame_token` points to a `RenderFrameProxyHost` whose frame
  // tree node's `current_frame_host()` has changed since tool registration.
  if (!target_rfh->GetLastCommittedOrigin().IsSameOriginWith(
          expected_target_origin)) {
    std::move(callback).Run(std::nullopt, false);
    return;
  }

  auto* target_data = ModelContextUserData::GetForCurrentDocument(target_rfh);
  // Don't kill the renderer, since legitimate script can target the wrong
  // document, including one that has never touched its `modelContext` accessor,
  // and therefore does not have `target_data`. However, if our API ever moves
  // to opaque "Tool" interface that internally encapsulates a tool host's frame
  // token instead of making JavaScript pass it in via a `Window` argument, then
  // we'd be able to terminate the renderer here, since it would not be possible
  // for a legitimate renderer to supply a frame token targeting a frame host
  // that exists, but does not have `target_data`.
  if (!target_data) {
    std::move(callback).Run(std::nullopt, false);
    return;
  }

  auto& tools = target_data->script_tools();
  auto it = std::find_if(
      tools.begin(), tools.end(),
      [&](const blink::mojom::ScriptToolPtr& t) { return t->name == name; });

  // Don't kill the renderer, since legitimate script can provide the name of a
  // tool that doesn't exist (including one that did exist, but got unregistered
  // by the time the call to invoke it here).
  if (it == tools.end()) {
    std::move(callback).Run(std::nullopt, false);
    return;
  }

  // Don't kill the renderer here, since legitimate script can target a tool in
  // another document that it might have been told about, but technically cannot
  // access.
  if (!IsScriptToolVisibleToOrigin(
          target_rfh->GetLastCommittedOrigin(), (*it)->exposed_origins,
          render_frame_host().GetLastCommittedOrigin())) {
    std::move(callback).Run(std::nullopt, false);
    return;
  }

  // At this point, it is safe to invoke the tool in the target renderer pointed
  // to by `target_data`.
  //
  // TODO(http://b/485810761): Right now `invocation_id` is only used to
  // identify pending execution requests in the browser process. Plumb this up
  // to the renderer for use by DevTools.
  base::UnguessableToken invocation_id = base::UnguessableToken::Create();

  ModelContextPageUserData* page_data =
      ModelContextPageUserData::GetOrCreateForPage(target_rfh->GetPage());
  ModelContextPageUserData::PendingScriptToolExecution execution;
  execution.caller_token =
      static_cast<RenderFrameHostImpl&>(render_frame_host()).GetDocumentToken();
  execution.target_token = target_rfh->GetDocumentToken();
  execution.tool_name = name;
  execution.callback = std::move(callback);
  page_data->AddPendingScriptToolExecution(invocation_id, std::move(execution));

  target_data->model_context_remote_->ExecuteScriptTool(
      name, input_arguments,
      base::BindOnce(
          [](base::WeakPtr<ModelContextPageUserData> page_data,
             base::UnguessableToken invocation_id,
             const std::optional<std::string>& result, bool success) {
            if (page_data) {
              page_data->CompletePendingScriptToolExecution(invocation_id,
                                                            result, success);
            }
          },
          page_data->GetWeakPtr(), invocation_id));
}

void ModelContextUserData::NotifyToolChange(
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

    if (IsScriptToolVisibleToOrigin(tool_owner_origin, exposed_origins,
                                    frame->GetLastCommittedOrigin())) {
      auto* data = ModelContextUserData::GetForCurrentDocument(frame);
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

PAGE_USER_DATA_KEY_IMPL(ModelContextPageUserData);

ModelContextPageUserData::PendingScriptToolExecution::
    PendingScriptToolExecution() = default;
ModelContextPageUserData::PendingScriptToolExecution::
    PendingScriptToolExecution(PendingScriptToolExecution&&) = default;
ModelContextPageUserData::PendingScriptToolExecution&
ModelContextPageUserData::PendingScriptToolExecution::operator=(
    PendingScriptToolExecution&&) = default;
ModelContextPageUserData::PendingScriptToolExecution::
    ~PendingScriptToolExecution() = default;

ModelContextPageUserData::ModelContextPageUserData(Page& page)
    : PageUserData<ModelContextPageUserData>(page) {}

ModelContextPageUserData::~ModelContextPageUserData() = default;

void ModelContextPageUserData::AddPendingScriptToolExecution(
    const base::UnguessableToken& invocation_id,
    PendingScriptToolExecution execution) {
  pending_script_tool_executions_.emplace(invocation_id, std::move(execution));
}

void ModelContextPageUserData::CompletePendingScriptToolExecution(
    const base::UnguessableToken& invocation_id,
    const std::optional<std::string>& result,
    bool success) {
  // We are here because a tool just returned a result, and we should forward
  // the result to the tool's invoker. But it's possible that no pending
  // execution exists for this tool anymore. This can happen if the invoker
  // document is destroyed before the response from the renderer hosting the
  // tool comes back.
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

  std::move(it->second.callback).Run(result, success);
  pending_script_tool_executions_.erase(it);
}

void ModelContextPageUserData::CancelPendingScriptToolExecutionsForDocument(
    const blink::DocumentToken& document_token) {
  for (auto it = pending_script_tool_executions_.begin();
       it != pending_script_tool_executions_.end();) {
    if (it->second.caller_token == document_token ||
        it->second.target_token == document_token) {
      std::move(it->second.callback).Run(std::nullopt, false);
      it = pending_script_tool_executions_.erase(it);
    } else {
      ++it;
    }
  }
}

void ModelContextPageUserData::
    CancelPendingScriptToolExecutionsDueToUnregistration(
        const blink::DocumentToken& target_document_token,
        const std::string& tool_name) {
  for (auto it = pending_script_tool_executions_.begin();
       it != pending_script_tool_executions_.end();) {
    if (it->second.target_token == target_document_token &&
        it->second.tool_name == tool_name) {
      std::move(it->second.callback).Run(std::nullopt, false);
      it = pending_script_tool_executions_.erase(it);
    } else {
      ++it;
    }
  }
}

base::WeakPtr<ModelContextPageUserData> ModelContextPageUserData::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace content
