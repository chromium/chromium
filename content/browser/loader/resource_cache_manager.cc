// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/resource_cache_manager.h"

#include "base/containers/contains.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/loader/resource_cache.mojom.h"

namespace content {

namespace {

// Calculates a resource cache key from a committed render frame host.
ResourceCacheKey CalculateResourceCacheKeyFromCommittedFrame(
    RenderFrameHostImpl& render_frame_host) {
  CHECK(!render_frame_host.HasPendingCommitForCrossDocumentNavigation());
  return ResourceCacheKey(render_frame_host.GetProcess()->GetProcessLock(),
                          render_frame_host.GetLastCommittedOrigin());
}

// Returns a RenderFrameHostImpl associated with `render_frame_host_id` if
// it can host or use a remote cache. Otherwise, return nullptr.
RenderFrameHostImpl* GetEligibleRenderFrameHostFromID(
    GlobalRenderFrameHostId render_frame_host_id) {
  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(render_frame_host_id);
  if (!render_frame_host || !render_frame_host->IsActive()) {
    return nullptr;
  }

  // TODO(https://crbug.com/141426): Check lifecycle state of
  // `render_frame_host` so that we don't treat the frame as an eligible
  // frame when the frame is in the BFCache.

  if (render_frame_host->HasPendingCommitForCrossDocumentNavigation()) {
    // TODO(https://crbug.com/141426): Consider handling pending navigation.
    // If there is a pending cross origin navigation, the frame isn't ready
    // to use or host a remote cache due to initialization ordering in blink.
    return nullptr;
  }

  return render_frame_host;
}

// Creates a mojo pending remote from `hosting_render_frame_host` and passes
// it to `target_render_frame_host`.
void PassResourceCacheRemote(RenderFrameHostImpl& hosting_render_frame_host,
                             RenderFrameHostImpl& target_render_frame_host) {
  mojo::PendingRemote<blink::mojom::ResourceCache> pending_remote;
  hosting_render_frame_host.GetRemoteInterfaces()->GetInterface(
      pending_remote.InitWithNewPipeAndPassReceiver());
  target_render_frame_host.SetResourceCacheRemote(std::move(pending_remote));
}

}  // namespace

ResourceCacheKey::ResourceCacheKey(const ProcessLock& process_lock,
                                   const url::Origin& origin)
    : process_lock(process_lock), origin(origin) {}

bool ResourceCacheKey::operator==(const ResourceCacheKey& rhs) const {
  return this->process_lock == rhs.process_lock && this->origin == rhs.origin;
}

bool ResourceCacheKey::operator<(const ResourceCacheKey& rhs) const {
  return this->process_lock < rhs.process_lock || this->origin < rhs.origin;
}

ResourceCacheManager::ResourceCacheEntry::ResourceCacheEntry(
    mojo::Remote<blink::mojom::ResourceCache> remote,
    GlobalRenderFrameHostId render_frame_host_id)
    : remote(std::move(remote)), render_frame_host_id(render_frame_host_id) {}

ResourceCacheManager::ResourceCacheEntry::~ResourceCacheEntry() = default;

ResourceCacheManager::ResourceCacheManager() = default;

ResourceCacheManager::~ResourceCacheManager() = default;

void ResourceCacheManager::MaybeInitializeResourceCacheRemoteOnCommitNavigation(
    mojo::PendingRemote<blink::mojom::ResourceCache>& pending_remote,
    NavigationRequest& navigation_request) {
  CHECK_EQ(navigation_request.state(),
           NavigationRequest::NavigationState::READY_TO_COMMIT);
  CHECK(!pending_remote.is_valid());
  if (!navigation_request.GetURL().SchemeIsHTTPOrHTTPS()) {
    return;
  }

  RenderFrameHostImpl* render_frame_host =
      navigation_request.GetRenderFrameHost();
  CHECK(render_frame_host);

  // At this point there could be a renderer that can host a ResourceCache but
  // doesn't host it yet. Find an existing ResourceCache to connect to the
  // committing RenderFrameHost. If none exists, try to create one.
  const ResourceCacheKey key(render_frame_host->GetProcess()->GetProcessLock(),
                             *navigation_request.GetOriginToCommit());

  RenderFrameHostImpl* hosting_render_frame_host =
      FindOrMaybeCreateResourceCache(key);
  if (hosting_render_frame_host) {
    hosting_render_frame_host->GetRemoteInterfaces()->GetInterface(
        pending_remote.InitWithNewPipeAndPassReceiver());

    // It's possible that the RenderFrameHost is already hosting a
    // ResourceCache, when doing a same-RenderFrameHost navigation.
    if (hosting_render_frame_host->GetGlobalId() ==
        render_frame_host->GetGlobalId()) {
      return;
    }
  }

  // The RenderFrameHost we're about to commit in doesn't host a ResourceCache,
  // so put it in non hosting hosts.
  // TODO(https://crbug.com/141426): When two renderers that have the same
  // process lock are about to commit at the same time, one may not get a
  // ResourceCache remote. Figure out how to ensure renderers will get a
  // ResourceCache remote eventually.
  auto non_hosting_it = non_hosting_frame_hosts_.find(key);
  if (non_hosting_it == non_hosting_frame_hosts_.end()) {
    std::set<GlobalRenderFrameHostId> hosts = {
        render_frame_host->GetGlobalId()};
    non_hosting_frame_hosts_.emplace(key, std::move(hosts));
  } else {
    CHECK(!base::Contains(non_hosting_it->second,
                          render_frame_host->GetGlobalId()));
    non_hosting_it->second.insert(render_frame_host->GetGlobalId());
  }
}

void ResourceCacheManager::RenderFrameHostDeleted(
    RenderFrameHostImpl& render_frame_host) {
  const ResourceCacheKey key =
      CalculateResourceCacheKeyFromCommittedFrame(render_frame_host);
  remote_caches_.erase(key);
  auto it = non_hosting_frame_hosts_.find(key);
  if (it != non_hosting_frame_hosts_.end()) {
    it->second.erase(render_frame_host.GetGlobalId());
  }
}

bool ResourceCacheManager::IsRenderFrameHostHostingRemoteCache(
    RenderFrameHostImpl& render_frame_host) {
  const ResourceCacheKey key =
      CalculateResourceCacheKeyFromCommittedFrame(render_frame_host);
  auto remote_cache_it = remote_caches_.find(key);
  if (remote_cache_it == remote_caches_.end()) {
    // There is no remote for the key. No renderer hosts a remote cache.
    return false;
  }

  return remote_cache_it->second.render_frame_host_id ==
         render_frame_host.GetGlobalId();
}

RenderFrameHostImpl* ResourceCacheManager::FindOrMaybeCreateResourceCache(
    const ResourceCacheKey& key) {
  auto remote_cache_it = remote_caches_.find(key);
  if (remote_cache_it != remote_caches_.end()) {
    return RenderFrameHostImpl::FromID(
        remote_cache_it->second.render_frame_host_id);
  }

  // Try to create a new ResourceCache.

  auto non_hosting_it = non_hosting_frame_hosts_.find(key);
  if (non_hosting_it == non_hosting_frame_hosts_.end()) {
    return nullptr;
  }

  // Find an eligible RenderFrameHost to host a new ResourceCache.
  RenderFrameHostImpl* new_hosting_render_frame = nullptr;
  auto render_frame_host_it = non_hosting_it->second.begin();
  while (render_frame_host_it != non_hosting_it->second.end()) {
    RenderFrameHostImpl* render_frame_host =
        GetEligibleRenderFrameHostFromID(*render_frame_host_it);
    if (!render_frame_host) {
      ++render_frame_host_it;
      continue;
    }

    // Create a ResourceCache on the first eligible RenderFrameHost.
    if (!new_hosting_render_frame) {
      CHECK_EQ(key.process_lock,
               render_frame_host->GetProcess()->GetProcessLock());
      CHECK(!render_frame_host->HasPendingCommitForCrossDocumentNavigation());

      mojo::Remote<blink::mojom::ResourceCache> remote =
          CreateResourceCacheRemote(*render_frame_host, key);
      auto [it, inserted] = remote_caches_.try_emplace(
          std::move(key), std::move(remote), render_frame_host->GetGlobalId());
      CHECK(inserted);

      render_frame_host_it = non_hosting_it->second.erase(render_frame_host_it);
      new_hosting_render_frame = render_frame_host;
      continue;
    }

    // The first eligible RenderFrameHost was selected to host a new
    // ResourceCache. We will connect the newly created one to the remaining
    // renderers.
    PassResourceCacheRemote(*new_hosting_render_frame, *render_frame_host);

    ++render_frame_host_it;
  }

  return new_hosting_render_frame;
}

mojo::Remote<blink::mojom::ResourceCache>
ResourceCacheManager::CreateResourceCacheRemote(
    RenderFrameHostImpl& render_frame_host,
    const ResourceCacheKey& key) {
  mojo::PendingRemote<blink::mojom::ResourceCache> pending_remote;
  render_frame_host.GetRemoteInterfaces()->GetInterface(
      pending_remote.InitWithNewPipeAndPassReceiver());
  mojo::Remote<blink::mojom::ResourceCache> remote(std::move(pending_remote));
  remote.set_disconnect_handler(base::BindOnce(
      &ResourceCacheManager::OnRemoteCacheDisconnected,
      weak_ptr_factory_.GetWeakPtr(), key, render_frame_host.GetGlobalId()));
  return remote;
}

void ResourceCacheManager::OnRemoteCacheDisconnected(
    const ResourceCacheKey& key,
    GlobalRenderFrameHostId render_frame_host_id) {
  auto it = remote_caches_.find(key);
  if (it == remote_caches_.end()) {
    return;
  }

  auto* render_frame_host = RenderFrameHostImpl::FromID(render_frame_host_id);
  // `render_frame_host` could be nullptr when the frame is about to shutdown,
  // the browser is shutting down, or the renderer process was crashed.
  if (!render_frame_host || render_frame_host->is_render_frame_deleted()) {
    remote_caches_.erase(key);
    FindOrMaybeCreateResourceCache(key);
    return;
  }

  CHECK(!render_frame_host->GetProcess()->ShutdownRequested());

  // Same-origin-same-process navigation happened. The previous mojo remote was
  // disconnected but the frame can continue hosting the remote cache. Recreate
  // a new mojo connection.
  mojo::Remote<blink::mojom::ResourceCache> remote =
      CreateResourceCacheRemote(*render_frame_host, key);
  it->second.remote = std::move(remote);

  // Plumb the new remote to eligible non hosting frames.
  auto non_hosting_it = non_hosting_frame_hosts_.find(key);
  if (non_hosting_it == non_hosting_frame_hosts_.end()) {
    return;
  }
  for (auto& id : non_hosting_it->second) {
    RenderFrameHostImpl* eligible_non_hosting_frame_host =
        GetEligibleRenderFrameHostFromID(id);
    if (eligible_non_hosting_frame_host) {
      PassResourceCacheRemote(*render_frame_host,
                              *eligible_non_hosting_frame_host);
    }
  }
}

}  // namespace content
