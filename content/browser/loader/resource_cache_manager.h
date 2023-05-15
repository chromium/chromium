// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_RESOURCE_CACHE_MANAGER_H_
#define CONTENT_BROWSER_LOADER_RESOURCE_CACHE_MANAGER_H_

#include <map>
#include <memory>
#include <set>

#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/process_lock.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/loader/resource_cache.mojom.h"
#include "url/origin.h"

namespace content {

class NavigationRequest;
class RenderFrameHostImpl;
class ResourceCacheHostObserver;

// Used as a key to group ResourceCache.
struct ResourceCacheKey {
  ResourceCacheKey(const ProcessLock& process_lock, const url::Origin& origin);

  bool operator==(const ResourceCacheKey& rhs) const;
  bool operator<(const ResourceCacheKey& rhs) const;

  const ProcessLock process_lock;
  const url::Origin origin;
};

// Manages ResourceCache. Refer to
// //third_party/blink/public/mojom/loader/resource_cache.mojom for what
// ResourceCache does.
// ResourceCacheManager maintains mojo connections to ResourceCache instances,
// which live in renderers, keyed with ProcessLock and origin. Upon navigation
// commit, it tries to find or create a mojo remote to ResourceCache when there
// is no active ResourceCache for the newly committing document for the given
// ProcessLock/origin. The ResourceCache will be created and hosted by another
// existing renderer, not the one that we're about to commit to. The remote
// might also be used for subsequent navigation commits to pass the (cloned)
// remote to another frame in a new renderer that has the same
// ProcessLock/origin. When a remote is disconnected, ResourceCacheManager tries
// to find a renderer to host a ResourceCache for the same ProcessLock/origin.
//
// ResourceCacheManager and ResourceCache exist only to record histograms.
// These shouldn't plumb resources among renderers.
// Exists per profile (i.e. owned by StoragePartition).
class CONTENT_EXPORT ResourceCacheManager {
 public:
  ResourceCacheManager();
  ~ResourceCacheManager();

  // Initializes `pending_remote` when an existing renderer has the same
  // process lock and the renderer has a ResourceCache with the same
  // Process lock/origin as the navigation. If the existing renderer
  // doesn't host a ResourceCache, creates a ResourceCache in an existing
  // renderer and initializes `pending_remote` with it. If no matching renderer
  // exists, `pending_remote` stays uninitialized.
  // `navigation_request` must be about to commit navigation.
  void MaybeInitializeResourceCacheRemoteOnCommitNavigation(
      mojo::PendingRemote<blink::mojom::ResourceCache>& pending_remote,
      NavigationRequest& navigation_request);

  // Called when a RenderFrameHostImpl became ineligible to host a
  // ResourceCache, e.g., the renderer is deleted or is stored in BFCache.
  void RenderFrameHostBecameIneligible(RenderFrameHostImpl& render_frame_host);

  // Called when lifecycle state of `render_frame_host` has changed.
  void RenderFrameHostStateChanged(RenderFrameHostImpl& render_frame_host,
                                   RenderFrameHost::LifecycleState old_state,
                                   RenderFrameHost::LifecycleState new_state);

  // Destroys a host observer.
  void DestroyHostObserver(ResourceCacheHostObserver* observer);

  // Returns true when a frame associated with `render_frame_host` is
  // hosting a ResourceCache.
  bool IsRenderFrameHostHostingRemoteCache(
      RenderFrameHostImpl& render_frame_host);

 private:
  struct ResourceCacheEntry {
    ResourceCacheEntry(mojo::Remote<blink::mojom::ResourceCache> remote,
                       GlobalRenderFrameHostId render_frame_host_id);
    ~ResourceCacheEntry();

    ResourceCacheEntry(const ResourceCacheEntry&) = delete;
    ResourceCacheEntry& operator=(const ResourceCacheEntry&) = delete;

    mojo::Remote<blink::mojom::ResourceCache> remote;
    GlobalRenderFrameHostId render_frame_host_id;
  };

  // Inserts `render_frame_host_id` to `non_hosting_frame_hosts_`.
  void InsertNonHostingRenderFrameId(
      const ResourceCacheKey& key,
      GlobalRenderFrameHostId render_frame_host_id);

  // Looks for an active ResourceCache. If not found, tries to create a new
  // ResourceCache in a renderer. Returns a RenderFrameHostImpl that hosts the
  // ResourceCache. Returns nullptr when no resource cache is/ found nor
  // created.
  RenderFrameHostImpl* FindOrMaybeCreateResourceCache(
      const ResourceCacheKey& key);

  // Makes `render_frame_host` host a ResourceCache.
  void HostResourceCache(RenderFrameHostImpl& render_frame_host,
                         const ResourceCacheKey& key);

  // Creates blink::mojom::ResourceCache interface from `render_frame_host`.
  mojo::Remote<blink::mojom::ResourceCache> CreateResourceCacheRemote(
      RenderFrameHostImpl& render_frame_host,
      const ResourceCacheKey& key);

  // Called when a mojo remote to a ResourceCache is disconnected, to pick
  // a new renderer to host a ResourceCache for the `key`.
  void OnRemoteCacheDisconnected(const ResourceCacheKey& key,
                                 GlobalRenderFrameHostId render_frame_host_id);

  // Contains active remote caches.
  std::map<ResourceCacheKey, ResourceCacheEntry> remote_caches_;

  // Keeps track of renderers that don't host ResourceCache. Used to select
  // a new host for a ResourceCache when a renderer that hosted the
  // ResourceCache is disconnected.
  std::map<ResourceCacheKey, std::set<GlobalRenderFrameHostId>>
      non_hosting_frame_hosts_;

  // A set of WebContentsObservers to keep track of lifecycle state changes of
  // RenderFrameHosts that host ResourceCaches. Used to handle lifecycle state
  // changes. If a RenderFrameHost becomes inactive (e.g. pending deletion,
  // BFCached), the host becomes ineligible to host a ResourceCache. If the
  // RenderFrameHost becomes active again, the RenderFrameHost becomes eligible
  // to use/host a ResourceCache.
  std::set<std::unique_ptr<ResourceCacheHostObserver>,
           base::UniquePtrComparator>
      host_observers_;

  base::WeakPtrFactory<ResourceCacheManager> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_RESOURCE_CACHE_MANAGER_H_
