// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/batched_proxy_ipc_sender.h"

#include "base/memory/safe_ref.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/frame_replication_state.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/remote_frame.mojom.h"

namespace content {
BatchedProxyIPCSender::BatchedProxyIPCSender(
    base::SafeRef<RenderFrameProxyHost> root_proxy)
    : root_proxy_host_(root_proxy) {
  DCHECK(!root_proxy->frame_tree_node()->parent());
}

BatchedProxyIPCSender::~BatchedProxyIPCSender() {}

void BatchedProxyIPCSender::AddNewChildProxyCreationTask(
    base::SafeRef<RenderFrameProxyHost> proxy_host,
    const ::blink::RemoteFrameToken& token,
    const std::optional<::blink::FrameToken>& opener_frame_token,
    ::blink::mojom::TreeScopeType tree_scope_type,
    ::blink::mojom::FrameReplicationStatePtr replication_state,
    ::blink::mojom::FrameOwnerPropertiesPtr owner_properties,
    bool is_loading,
    const ::base::UnguessableToken& devtools_frame_token,
    ::blink::mojom::RemoteFrameInterfacesFromBrowserPtr remote_frame_interfaces,
    GlobalRoutingID parent_global_id) {
  blink::mojom::CreateRemoteChildParamsPtr new_proxy_params =
      blink::mojom::CreateRemoteChildParams::New();
  new_proxy_params->token = token;
  new_proxy_params->opener_frame_token = opener_frame_token;
  new_proxy_params->tree_scope_type = tree_scope_type;
  new_proxy_params->replication_state = std::move(replication_state);
  new_proxy_params->owner_properties = std::move(owner_properties);
  new_proxy_params->is_loading = is_loading;
  new_proxy_params->devtools_frame_token = devtools_frame_token;
  new_proxy_params->frame_interfaces = std::move(remote_frame_interfaces);

  std::vector<blink::mojom::CreateRemoteChildParamsPtr>& child_params =
      parent_global_id == root_proxy_host_->GetGlobalID()
          ? create_remote_children_params_
          : proxy_to_child_params_.at(parent_global_id)->child_params;

  child_params.emplace_back(std::move(new_proxy_params));
  proxy_to_child_params_[proxy_host->GetGlobalID()] = child_params.back().get();

  proxy_hosts_.push_back(proxy_host);
}

void BatchedProxyIPCSender::CreateAllProxies() {
  if (create_remote_children_params_.empty()) {
    return;
  }

  root_proxy_host_->GetAssociatedRemoteFrame()->CreateRemoteChildren(
      std::move(create_remote_children_params_));

  for (const auto& proxy_host : proxy_hosts_) {
    proxy_host->SetRenderFrameProxyCreated(true);
  }
}

bool BatchedProxyIPCSender::IsProxyCreationPending(GlobalRoutingID global_id) {
  return proxy_to_child_params_.find(global_id) != proxy_to_child_params_.end();
}

}  // namespace content
