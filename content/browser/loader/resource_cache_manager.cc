// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/resource_cache_manager.h"

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents_observer.h"
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
    GlobalRenderFrameHostId render_frame_host_id,
    bool check_pending_navigation = true) {
  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(render_frame_host_id);
  if (!render_frame_host || !render_frame_host->IsActive() ||
      !render_frame_host->IsRenderFrameLive() ||
      render_frame_host->is_render_frame_deleted()) {
    return nullptr;
  }

  if (check_pending_navigation &&
      render_frame_host->HasPendingCommitForCrossDocumentNavigation()) {
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
  CHECK_NE(hosting_render_frame_host.GetGlobalId(),
           target_render_frame_host.GetGlobalId());
  mojo::PendingRemote<blink::mojom::ResourceCache> pending_remote;
  hosting_render_frame_host.GetRemoteInterfaces()->GetInterface(
      pending_remote.InitWithNewPipeAndPassReceiver());
  target_render_frame_host.SetResourceCacheRemote(std::move(pending_remote));
}

}  // namespace

// A WebContentsObserver implementation to track state changes in a
// RenderFrameHost that hosts or has hosted a ResourceCache. Used to manage
// lifecycle changes of RenderFrameHost.
class ResourceCacheHostObserver : public WebContentsObserver {
 public:
  ResourceCacheHostObserver(ResourceCacheManager* manager,
                            WebContentsImpl* web_contents)
      : WebContentsObserver(web_contents), manager(manager) {
    CHECK(manager);
  }

  // WebContentsObserver implementations:
  void RenderFrameHostStateChanged(
      RenderFrameHost* render_frame_host,
      RenderFrameHost::LifecycleState old_state,
      RenderFrameHost::LifecycleState new_state) override {
    manager->RenderFrameHostStateChanged(
        *static_cast<RenderFrameHostImpl*>(render_frame_host), old_state,
        new_state);
  }

  void WebContentsDestroyed() override {
    manager->DestroyHostObserver(this);
    // `manager` deleted `this`.
  }

 private:
  // `manager` owns `this`.
  const raw_ptr<ResourceCacheManager> manager;
};

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
  InsertNonHostingRenderFrameId(key, render_frame_host->GetGlobalId());
}

void ResourceCacheManager::RenderFrameHostBecameIneligible(
    RenderFrameHostImpl& render_frame_host) {
  CHECK(!GetEligibleRenderFrameHostFromID(render_frame_host.GetGlobalId()));

  const ResourceCacheKey key =
      CalculateResourceCacheKeyFromCommittedFrame(render_frame_host);
  auto non_hosting_it = non_hosting_frame_hosts_.find(key);
  if (non_hosting_it != non_hosting_frame_hosts_.end()) {
    non_hosting_it->second.erase(render_frame_host.GetGlobalId());
  }

  auto remote_cache_it = remote_caches_.find(key);
  if (remote_cache_it != remote_caches_.end() &&
      remote_cache_it->second.render_frame_host_id ==
          render_frame_host.GetGlobalId()) {
    remote_caches_.erase(key);
    FindOrMaybeCreateResourceCache(key);
  }
}

void ResourceCacheManager::RenderFrameHostStateChanged(
    RenderFrameHostImpl& render_frame_host,
    RenderFrameHost::LifecycleState old_state,
    RenderFrameHost::LifecycleState new_state) {
  if (new_state == RenderFrameHost::LifecycleState::kPendingDeletion ||
      new_state == RenderFrameHost::LifecycleState::kInBackForwardCache) {
    // * When a RenderFrameHost is pending deletion, it's no longer eligible to
    //   host a ResourceCache.
    // * When a LocalFrame in a renderer process was stored in the BFCache, the
    //   frame got frozen. The frame unbound all ResourceCache receivers when it
    //   got frozen. To reflect that, make the corresponding RenderFrameHost
    //   ineligible to host ResourceCache.
    RenderFrameHostBecameIneligible(render_frame_host);
    return;
  }

  if (old_state == RenderFrameHost::LifecycleState::kInBackForwardCache &&
      new_state == RenderFrameHost::LifecycleState::kActive) {
    // This RenderFrameHost was restored from the BFCache. Treat the
    // RenderFrameHost as eligible to host/use ResourceCache.
    const ResourceCacheKey key =
        CalculateResourceCacheKeyFromCommittedFrame(render_frame_host);
    auto remote_cache_it = remote_caches_.find(key);
    if (remote_cache_it == remote_caches_.end()) {
      HostResourceCache(render_frame_host, key);
    } else {
      InsertNonHostingRenderFrameId(key, render_frame_host.GetGlobalId());
      auto* hosting_render_frame = GetEligibleRenderFrameHostFromID(
          remote_cache_it->second.render_frame_host_id);
      CHECK(hosting_render_frame);
      PassResourceCacheRemote(*hosting_render_frame, render_frame_host);
    }
    return;
  }

  // TODO(https://crbug.com/141426): Consider handling prerenders.
}

void ResourceCacheManager::DestroyHostObserver(
    ResourceCacheHostObserver* observer) {
  auto it = host_observers_.find(observer);
  CHECK(it != host_observers_.end());
  host_observers_.erase(it);
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

void ResourceCacheManager::InsertNonHostingRenderFrameId(
    const ResourceCacheKey& key,
    GlobalRenderFrameHostId render_frame_host_id) {
  auto non_hosting_it = non_hosting_frame_hosts_.find(key);
  if (non_hosting_it == non_hosting_frame_hosts_.end()) {
    std::set<GlobalRenderFrameHostId> hosts = {render_frame_host_id};
    non_hosting_frame_hosts_.emplace(key, std::move(hosts));
  } else {
    CHECK(!base::Contains(non_hosting_it->second, render_frame_host_id));
    non_hosting_it->second.insert(render_frame_host_id);
  }
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
      HostResourceCache(*render_frame_host, key);

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

void ResourceCacheManager::HostResourceCache(
    RenderFrameHostImpl& render_frame_host,
    const ResourceCacheKey& key) {
  CHECK_EQ(key.process_lock, render_frame_host.GetProcess()->GetProcessLock());
  CHECK(!render_frame_host.HasPendingCommitForCrossDocumentNavigation());

  mojo::Remote<blink::mojom::ResourceCache> remote =
      CreateResourceCacheRemote(render_frame_host, key);
  auto [it, inserted] = remote_caches_.try_emplace(
      std::move(key), std::move(remote), render_frame_host.GetGlobalId());
  CHECK(inserted);
  auto* web_contents = static_cast<WebContentsImpl*>(
      WebContentsImpl::FromRenderFrameHostID(render_frame_host.GetGlobalId()));
  auto observer_it = base::ranges::find_if(
      host_observers_,
      [&web_contents](
          const std::unique_ptr<ResourceCacheHostObserver>& observer) {
        return web_contents == observer->web_contents();
      });
  if (observer_it == host_observers_.end()) {
    host_observers_.insert(
        std::make_unique<ResourceCacheHostObserver>(this, web_contents));
  }
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

  // `render_frame_host` could be nullptr when the frame is about to shutdown,
  // the browser is shutting down, or the renderer process was crashed.
  // Also bypass pending navigation check because `render_frame_host` could
  // have a pending navigation when same-origin-same-process navigation
  // happened.
  auto* render_frame_host = GetEligibleRenderFrameHostFromID(
      render_frame_host_id, /*check_pending_navigation=*/false);
  if (!render_frame_host) {
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
