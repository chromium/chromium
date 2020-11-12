// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRERENDER_PRERENDER_HOST_H_
#define CONTENT_BROWSER_PRERENDER_PRERENDER_HOST_H_

#include <memory>

#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/prerender/prerender.mojom.h"
#include "url/gurl.h"

namespace content {

class RenderFrameHostImpl;
class WebContents;

// Prerender2:
// PrerenderHost creates a new WebContents and starts prerendering with that.
// Then navigation code is expected to find this host from PrerenderHostRegistry
// and activate the prerendered WebContents upon navigation. This is created per
// request from a renderer process via PrerenderProcessor or will directly be
// created for browser-initiated prerendering (this code path is not implemented
// yet). This is owned by PrerenderHostRegistry.
//
// TODO(https://crbug.com/1132746): Stop creating a new WebContents and instead
// use the MPArch.
class CONTENT_EXPORT PrerenderHost final : public WebContentsObserver {
 public:
  PrerenderHost(blink::mojom::PrerenderAttributesPtr attributes,
                const GlobalFrameRoutingId& initiator_render_frame_host_id,
                const url::Origin& initiator_origin);
  ~PrerenderHost() override;

  PrerenderHost(const PrerenderHost&) = delete;
  PrerenderHost& operator=(const PrerenderHost&) = delete;
  PrerenderHost(PrerenderHost&&) = delete;
  PrerenderHost& operator=(PrerenderHost&&) = delete;

  void StartPrerendering();

  // WebContentsObserver implementation:
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

  // Activates the prerendered contents. Returns false when activation didn't
  // occur for some reason. This must be called after this host gets ready for
  // activation.
  bool ActivatePrerenderedContents(
      RenderFrameHostImpl& current_render_frame_host);

  bool is_ready_for_activation() const { return is_ready_for_activation_; }

 private:
  const blink::mojom::PrerenderAttributesPtr attributes_;
  const GlobalFrameRoutingId initiator_render_frame_host_id_;
  const url::Origin initiator_origin_;

  // WebContents for prerendering.
  // TODO(https://crbug.com/1132746): Stop owning a new WebContents and instead
  // use the new MPArch mechanism.
  std::unique_ptr<WebContents> prerendered_contents_;

  // Indicates if `prerendered_contents_` is ready for activation.
  bool is_ready_for_activation_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRERENDER_PRERENDER_HOST_H_
