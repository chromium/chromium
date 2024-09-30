// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BATCHED_PROXY_IPC_SENDER_H_
#define CONTENT_BROWSER_RENDERER_HOST_BATCHED_PROXY_IPC_SENDER_H_

#include <map>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/safe_ref.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/frame_replication_state.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/remote_frame.mojom.h"

namespace blink {
namespace mojom {
enum class TreeScopeType;
}  // namespace mojom
}  // namespace blink

namespace content {

class RenderFrameProxyHost;

// Used to batch create proxies for child frames within a `SiteInstanceGroup`.
//
// This is only used when proxies within a single `FrameTree` are created by
// `CreateProxiesForSiteInstance()`. There are 2 cases:
// (1) A subframe navigates to a new `SiteInstance`.
// (2) An opener chain created by a child/main frame navigation, in which case
// the navigating `FrameTree` will batch create proxies only for a child frame
// navigation. All other `FrameTree`s will create their own instance of
// `BatchedProxyIPCSender` to batch create proxies.
//
// `CreateProxiesForSiteInstance()` iterates through the `FrameTree` in breadth
// first order, and normally creates a proxy for a `FrameTreeNode` if needed.
// If this class is used for batch creation, a node with the necessary
// parameters for proxy creation will be added to
// `create_remote_children_params_` instead of creating the proxy
// immediately. After iterating the `FrameTree`, all child frames can be
// created with `CreateAllProxies()`. See doc in https://crbug.com/1393697 for
// more details.
//
// Note: When any frame in a frame tree navigates to a `SiteInstance` X,
// there are two cases: either (1) X is a new `SiteInstance`, in which case
// all `FrameTreeNode`s will need a new proxy, and hence we'll start creating
// proxies from the root; or (2) X is an existing `SiteInstance` that may
// already have some frames in it, in which case Chromium guarantees that all
// `FrameTreeNode`s on the page would either be actual frames in X or already
// have proxies for X. In either case, there shouldn't be a need to create
// additional proxies for just a part of a `FrameTree`, so this class can
// assume that new proxies will start to be created from the root.
class CONTENT_EXPORT BatchedProxyIPCSender {
 public:
  explicit BatchedProxyIPCSender(
      base::SafeRef<RenderFrameProxyHost> root_proxy);
  ~BatchedProxyIPCSender();

  // Creates a new node in `create_remote_children_params_` with all the
  // necessary parameters to create a proxy. The newly created node is in the
  // same relative position as the `FrameTreeNode` that `proxy_host` is for.
  void AddNewChildProxyCreationTask(
      base::SafeRef<RenderFrameProxyHost> proxy_host,
      const ::blink::RemoteFrameToken& token,
      const std::optional<::blink::FrameToken>& opener_frame_token,
      ::blink::mojom::TreeScopeType tree_scope_type,
      ::blink::mojom::FrameReplicationStatePtr replication_state,
      ::blink::mojom::FrameOwnerPropertiesPtr owner_properties,
      bool is_loading,
      const ::base::UnguessableToken& devtools_frame_token,
      ::blink::mojom::RemoteFrameInterfacesFromBrowserPtr
          remote_frame_interfaces,
      GlobalRoutingID parent_global_id);

  // Makes 1 IPC to the renderer to create all child frame proxies.
  void CreateAllProxies();

  // Checks if this `BatchedProxyIPCSender` will create a proxy for the
  // `RenderFrameProxyHost` corresponding to `global_id`.
  bool IsProxyCreationPending(GlobalRoutingID global_id);

 private:
  // The `RenderFrameProxyHost` for the proxy corresponding to the root of the
  // `FrameTree`. Used to make the IPC call to do the batch creation.
  base::SafeRef<RenderFrameProxyHost> root_proxy_host_;

  // The immediate children of the root node of a tree that is identical in
  // structure to the `FrameTree` this object is creating proxies for. The proxy
  // corresponding to the root node is a main frame proxy, and would have been
  // already created. Other than the root, all other nodes contain the
  // parameters needed to create the proxy for the corresponding node in the
  // FrameTree.
  std::vector<blink::mojom::CreateRemoteChildParamsPtr>
      create_remote_children_params_;

  // Maintain references to call `SetIsRenderFrameProxyCreated(true)` after
  // creating all proxies.
  std::vector<base::SafeRef<RenderFrameProxyHost>> proxy_hosts_;

  // Maps the `RenderFrameProxyHost`'s `GlobalRoutingID` to the node in
  // `create_remote_children_params_` with the params to create the
  // corresponding proxy.
  std::map<GlobalRoutingID,
           raw_ptr<blink::mojom::CreateRemoteChildParams, CtnExperimental>>
      proxy_to_child_params_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_BATCHED_PROXY_IPC_SENDER_H_
