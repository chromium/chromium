// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_DELEGATE_H_

#include "base/callback_forward.h"
#include "base/process/kill.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace content {

class GuestHost;
class RenderFrameHost;
class RenderWidgetHost;
class SiteInstance;

// Objects implement this interface to get notified about changes in the guest
// WebContents and to provide necessary functionality.
class CONTENT_EXPORT BrowserPluginGuestDelegate {
 public:
  virtual ~BrowserPluginGuestDelegate() {}

  // Notification that the embedder will begin attachment. This is called
  // prior to resuming resource loads. |element_instance_id| uniquely identifies
  // the element that will serve as a container for the guest.
  // Once the content embedder has completed setting up state for attachment, it
  // must call the |completion_callback| to complete attachment.
  virtual void WillAttach(content::WebContents* embedder_web_contents,
                          int element_instance_id,
                          bool is_full_page_plugin,
                          base::OnceClosure completion_callback) {}

  virtual WebContents* CreateNewGuestWindow(
      const WebContents::CreateParams& create_params);

  // Notification that the embedder has completed attachment. The
  // |guest_proxy_routing_id| is the routing ID for the RenderView in the
  // embedder that will serve as a contentWindow proxy for the guest.
  virtual void DidAttach(int guest_proxy_routing_id) {}

  // Notification that the guest has detached from its container.
  virtual void DidDetach() {}

  // Notification that a valid |url| was dropped over the guest.
  virtual void DidDropLink(const GURL& url) {}

  // Notification that the BrowserPlugin has resized.
  virtual void ElementSizeChanged(const gfx::Size& size) {}

  // Returns the WebContents that currently owns this guest.
  virtual WebContents* GetOwnerWebContents();

  // Asks the delegate if the given guest can lock the pointer.
  // Invoking the |callback| synchronously is OK.
  virtual void RequestPointerLockPermission(
      bool user_gesture,
      bool last_unlocked_by_target,
      const base::Callback<void(bool)>& callback) {}

  // Provides the delegate with an interface with which to communicate with the
  // content module.
  virtual void SetGuestHost(GuestHost* guest_host) {}

  // TODO(ekaramad): A short workaround to force some types of guests to use
  // a BrowserPlugin even when we are using cross process frames for guests. It
  // should be removed after resolving https://crbug.com/642826).
  virtual bool CanUseCrossProcessFrames();

  // Returns the RenderWidgetHost corresponding to the owner frame.
  virtual RenderWidgetHost* GetOwnerRenderWidgetHost();

  // The site instance of the owner frame.
  virtual SiteInstance* GetOwnerSiteInstance();

  // Returns true if the corresponding guest is allowed to be embedded inside an
  // <iframe> which is cross process.
  virtual bool CanBeEmbeddedInsideCrossProcessFrames();

  // Returns the embedder frame for this guest.
  virtual RenderFrameHost* GetEmbedderFrame();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_DELEGATE_H_
