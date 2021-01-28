// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRERENDER_PRERENDER_HOST_REGISTRY_H_
#define CONTENT_BROWSER_PRERENDER_PRERENDER_HOST_REGISTRY_H_

#include <map>

#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/prerender/prerender.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class BrowserContext;
class FrameTreeNode;
class PrerenderHost;

// Prerender2:
// PrerenderHostRegistry manages running prerender hosts and provides the host
// to navigation code for activating prerendered contents. This is created and
// owned by StoragePartitionImpl.
class CONTENT_EXPORT PrerenderHostRegistry {
 public:
  explicit PrerenderHostRegistry(BrowserContext& browser_context);
  ~PrerenderHostRegistry();

  PrerenderHostRegistry(const PrerenderHostRegistry&) = delete;
  PrerenderHostRegistry& operator=(const PrerenderHostRegistry&) = delete;
  PrerenderHostRegistry(PrerenderHostRegistry&&) = delete;
  PrerenderHostRegistry& operator=(PrerenderHostRegistry&&) = delete;

  // Creates and starts a host. Returns the root frame tree node id of the
  // prerendered page, which can be used as the id of the host.
  int CreateAndStartHost(blink::mojom::PrerenderAttributesPtr attributes,
                         const url::Origin& initiator_origin);

  // Destroys the host registered for `frame_tree_node_id`.
  // TODO(https://crbug.com/1169594): Distinguish two paths that cancel
  // prerendering. A prerender can be canceled due to the following reasons:
  // 1. Initiator was no longer interested. Since one prerender may have several
  // initiators, PrerenderHostRegistry should not destroy a PrerenderHost
  // instance if one of the initiators is still alive.
  // 2. Prerendering page did something undesirable. The same behavior always
  // happens regardless of which caller calls it. So PrerenderHostRegistry
  // should destroy the PrerenderHost.
  void AbandonHost(int frame_tree_node_id);

  // Selects the host to activate for a navigation for the given FrameTreeNode.
  // Returns nullptr if it's not found or not ready for activation yet.
  std::unique_ptr<PrerenderHost> FindHostToActivate(
      const GURL& navigation_url,
      FrameTreeNode& frame_tree_node);

  // Returns a prerender host for `prerendering_url`. Returns nullptr if the URL
  // doesn't match any prerender host.
  PrerenderHost* FindHostByUrlForTesting(const GURL& prerendering_url);

 private:
  // This outlives `this` because PrerenderHostRegistry is owned by
  // StoragePartitionImpl, which in turn is owned by BrowserContext.
  BrowserContext& browser_context_;

  // TODO(https://crbug.com/1132746): Expire prerendered contents if they are
  // not used for a while.
  std::map<int, std::unique_ptr<PrerenderHost>>
      prerender_host_by_frame_tree_node_id_;
  std::map<GURL, int> frame_tree_node_id_by_url_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRERENDER_PRERENDER_HOST_REGISTRY_H_
