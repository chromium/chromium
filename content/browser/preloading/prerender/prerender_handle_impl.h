// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_HANDLE_IMPL_H_
#define CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_HANDLE_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "content/browser/preloading/prerender/prerender_host.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/prerender_handle.h"

class GURL;

namespace content {

class PrerenderHostRegistry;

class PrerenderHandleImpl final : public PrerenderHandle,
                                  public PrerenderHost::Observer {
 public:
  PrerenderHandleImpl(
      base::WeakPtr<PrerenderHostRegistry> prerender_host_registry,
      FrameTreeNodeId frame_tree_node_id,
      const GURL& url);
  ~PrerenderHandleImpl() override;

  // PrerenderHandle:
  const GURL& GetInitialPrerenderingUrl() const override;
  base::WeakPtr<PrerenderHandle> GetWeakPtr() override;
  void SetPreloadingAttemptFailureReason(
      PreloadingFailureReason reason) override;
  void SetActivationCallback(base::OnceClosure activation_callback) override;

  // PrerenderHost::Observer:
  void OnActivated() override;

  FrameTreeNodeId frame_tree_node_id_for_testing() const {
    return frame_tree_node_id_;
  }

 private:
  PrerenderHost* GetPrerenderHost();

  base::WeakPtr<PrerenderHostRegistry> prerender_host_registry_;
  // `frame_tree_node_id_` is the root FrameTreeNode id of the prerendered
  // page.
  const FrameTreeNodeId frame_tree_node_id_;

  const GURL prerendering_url_;

  bool was_activated_ = false;
  base::OnceClosure activation_callback_;

  base::WeakPtrFactory<PrerenderHandle> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_HANDLE_IMPL_H_
