// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRERENDER_PRERENDER_PROCESSOR_H_
#define CONTENT_BROWSER_PRERENDER_PRERENDER_PROCESSOR_H_

#include "base/scoped_observation.h"
#include "content/browser/prerender/prerender_host_registry.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/mojom/prerender/prerender.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class RenderFrameHostImpl;

// Prerender2:
// PrerenderProcessor implements blink::mojom::PrerenderProcessor and works as
// the browser-side entry point of prerendering. This is created per prerender
// request (see comments on the mojom interface for details) and owned by the
// initiator RenderFrameHostImpl's mojo::UniqueReceiverSet.
//
// When Start() is called from a renderer process, this asks
// PrerenderHostRegistry to create a PrerenderHost and start prerendering.
class CONTENT_EXPORT PrerenderProcessor final
    : public blink::mojom::PrerenderProcessor,
      public PrerenderHostRegistry::Observer {
 public:
  explicit PrerenderProcessor(RenderFrameHostImpl& initiator_render_frame_host);
  ~PrerenderProcessor() override;

  PrerenderProcessor(const PrerenderProcessor&) = delete;
  PrerenderProcessor& operator=(const PrerenderProcessor&) = delete;
  PrerenderProcessor(PrerenderProcessor&&) = delete;
  PrerenderProcessor& operator=(PrerenderProcessor&&) = delete;

  // blink::mojom::PrerenderProcessor implementation:
  void Start(blink::mojom::PrerenderAttributesPtr attributes) override;
  void Cancel() override;

  // PrerenderHostRegistry::Observer implementation:
  void OnRegistryDestroyed() override;

 private:
  void CancelPrerendering();

  // The initiator render frame host owns `this`, so the reference is safe.
  RenderFrameHostImpl& initiator_render_frame_host_;

  // The origin of the initiator render frame host. This could be different from
  // `initiator_render_frame_host_.GetLastCommittedOrigin()` because the frame
  // may navigate away before Start() is called from a renderer process.
  const url::Origin initiator_origin_;

  // The root frame tree node id of the prerendered page.
  int prerender_frame_tree_node_id_ = RenderFrameHost::kNoFrameTreeNodeId;

  enum class State { kInitial, kStarted, kCancelled };
  State state_ = State::kInitial;

  // `this` may outlive the registry. The registry is set on the constructor and
  // unset on OnRegistryDestroyed().
  PrerenderHostRegistry* registry_;
  base::ScopedObservation<PrerenderHostRegistry,
                          PrerenderHostRegistry::Observer>
      observation_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRERENDER_PRERENDER_PROCESSOR_H_
