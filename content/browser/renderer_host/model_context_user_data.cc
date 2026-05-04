// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/model_context_user_data.h"

#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
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
// relevant documents.
ModelContextUserData::~ModelContextUserData() = default;

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

  std::vector<url::Origin> exposed_origins = (*it)->exposed_origins;

  script_tools_.erase(it);

  NotifyToolChange(exposed_origins);
}

void ModelContextUserData::GetScriptTools(GetScriptToolsCallback callback) {
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
    for (const auto& t : local_tools) {
      if (IsScriptToolVisibleToOrigin(rfh->GetLastCommittedOrigin(),
                                      t->exposed_origins, caller_origin)) {
        all_tools.push_back(t.Clone());
      }
    }
    return RenderFrameHost::FrameIterationAction::kContinue;
  });

  std::move(callback).Run(std::move(all_tools));
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

}  // namespace content
