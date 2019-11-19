// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_CONTAINER_H_
#define EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_CONTAINER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/optional.h"
#include "components/guest_view/renderer/guest_view_container.h"
#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_container_base.h"
#include "url/gurl.h"
#include "v8/include/v8.h"

namespace blink {
class WebFrame;
class WebLocalFrame;
}  // namespace blink

namespace extensions {

// A container for loading up an extension inside a BrowserPlugin to handle a
// MIME type. A request for the URL of the data to load inside the container is
// made and a url is sent back in response which points to the URL which the
// container should be navigated to. There are two cases for making this URL
// request, the case where the plugin is embedded and the case where it is top
// level:
// 1) In the top level case a URL request for the data to load has already been
//    made by the renderer on behalf of the plugin. The |DidReceiveData| and
//    |DidFinishLoading| callbacks (from BrowserPluginDelegate) will be called
//    when data is received and when it has finished being received,
//    respectively.
// 2) In the embedded case, no URL request is automatically made by the
//    renderer. We make a URL request for the data inside the container using
//    a WebAssociatedURLLoader. In this case, the |didReceiveData| and
//    |didFinishLoading| (from WebAssociatedURLLoaderClient) when data is
//    received and when it has finished being received.
class MimeHandlerViewContainer : public guest_view::GuestViewContainer,
                                 public MimeHandlerViewContainerBase {
 public:
  MimeHandlerViewContainer(content::RenderFrame* render_frame,
                           const content::WebPluginInfo& info,
                           const std::string& mime_type,
                           const GURL& original_url);

  // GuestViewContainer implementation.
  bool OnMessage(const IPC::Message& message) override;
  void OnReady() override;

  // BrowserPluginDelegate implementation.
  void PluginDidFinishLoading() override;
  void PluginDidReceiveData(const char* data, int data_length) override;
  void DidResizeElement(const gfx::Size& new_size) override;
  v8::Local<v8::Object> V8ScriptableObject(v8::Isolate*) override;

  // GuestViewContainer overrides.
  void OnRenderFrameDestroyed() override;
  // PostMessageSupport::Delegate overrides.
  blink::WebLocalFrame* GetSourceFrame() override;
  blink::WebFrame* GetTargetFrame() override;
  bool IsEmbedded() const override;
  bool IsResourceAccessibleBySource() const override;

 protected:
  ~MimeHandlerViewContainer() override;

 private:
  // MimeHandlerViewContainerBase override.
  void CreateMimeHandlerViewGuestIfNecessary() final;
  int32_t GetInstanceId() const final;
  gfx::Size GetElementSize() const final;

  // Message handlers.
  void OnCreateMimeHandlerViewGuestACK(int element_instance_id);
  void OnGuestAttached(int element_instance_id,
                       int guest_proxy_routing_id);
  void OnMimeHandlerViewGuestOnLoadCompleted(int32_t element_instance_id);

  // The RenderView routing ID of the guest.
  int guest_proxy_routing_id_;
  // TODO(ekaramad): This is intentionally here instead of
  // MimeHandlerViewContainerBase because MimeHandlerViewFrameContainer will
  // soon be refactored, and no longer a subclass of  MHVCB. This means MHVCB
  // and MimeHandlerViewContainer should soon merge back into a MHVC class.
  // Determines whether the embedder can access |original_url_|. Used for UMA.
  bool is_resource_accessible_to_embedder_;

  // The size of the element.
  base::Optional<gfx::Size> element_size_;

  DISALLOW_COPY_AND_ASSIGN(MimeHandlerViewContainer);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_CONTAINER_H_
