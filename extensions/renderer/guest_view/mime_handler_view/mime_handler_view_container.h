// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_CONTAINER_H_
#define EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_CONTAINER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "components/guest_view/renderer/guest_view_container.h"
#include "extensions/common/api/mime_handler.mojom.h"
#include "extensions/common/guest_view/mime_handler_view_uma_types.h"
#include "extensions/common/mojom/guest_view.mojom.h"
#include "extensions/renderer/guest_view/mime_handler_view/post_message_support.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "third_party/blink/public/mojom/loader/transferrable_url_loader.mojom.h"
#include "third_party/blink/public/web/web_associated_url_loader_client.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "v8/include/v8.h"

namespace blink {
class URLLoaderThrottle;
class WebAssociatedURLLoader;
class WebFrame;
class WebLocalFrame;
}  // namespace blink

namespace content {
class RenderFrame;
struct WebPluginInfo;
}  // namespace content

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
class MimeHandlerViewContainer : public blink::WebAssociatedURLLoaderClient,
                                 public guest_view::GuestViewContainer,
                                 public mime_handler::BeforeUnloadControl,
                                 public PostMessageSupport::Delegate {
 public:
  MimeHandlerViewContainer(content::RenderFrame* render_frame,
                           const content::WebPluginInfo& info,
                           const std::string& mime_type,
                           const GURL& original_url);

  static mojom::GuestView* GuestView();

  // TODO(ekaramad): Remove this and make MimeHandlerViewContainerManager of
  // |render_frame| hold on to the list of MimeHandlerViewContainer.
  static std::vector<MimeHandlerViewContainer*> FromRenderFrame(
      content::RenderFrame* render_frame);

  // If the URL matches the same URL that this object has created and it hasn't
  // added a throttle yet, it will return a new one for the purpose of
  // intercepting it.
  std::unique_ptr<blink::URLLoaderThrottle> MaybeCreatePluginThrottle(
      const GURL& url);

  // GuestViewContainer implementation.
  bool OnMessage(const IPC::Message& message) override;
  void OnReady() override;

  // TODO(533069): Remove since BrowserPlugin has been removed.
  void PluginDidFinishLoading();
  void PluginDidReceiveData(const char* data, int data_length);
  void DidResizeElement(const gfx::Size& new_size);
  v8::Local<v8::Object> V8ScriptableObject(v8::Isolate*);

  // GuestViewContainer overrides.
  void OnRenderFrameDestroyed() override;

  // PostMessageSupport::Delegate overrides.
  blink::WebLocalFrame* GetSourceFrame() override;
  blink::WebFrame* GetTargetFrame() override;
  bool IsEmbedded() const override;
  bool IsResourceAccessibleBySource() const override;

  // WebAssociatedURLLoaderClient overrides.
  void DidReceiveData(const char* data, int data_length) override;
  void DidFinishLoading() override;

 protected:
  ~MimeHandlerViewContainer() override;

 private:
  class PluginResourceThrottle;

  // mime_handler::BeforeUnloadControl:
  void SetShowBeforeUnloadDialog(
      bool show_dialog,
      SetShowBeforeUnloadDialogCallback callback) override;

  void SendResourceRequest();
  void EmbedderRenderFrameWillBeGone();
  v8::Local<v8::Object> GetScriptableObjectInternal(v8::Isolate* isolate);
  void RecordInteraction(MimeHandlerViewUMATypes::Type uma_type);

  // Called for embedded plugins when network service is enabled. This is called
  // by the URLLoaderThrottle which intercepts the resource load, which is then
  // sent to the browser to be handed off to the plugin.
  void SetEmbeddedLoader(
      blink::mojom::TransferrableURLLoaderPtr transferrable_url_loader);

  void CreateMimeHandlerViewGuestIfNecessary();
  int32_t GetInstanceId() const;
  gfx::Size GetElementSize() const;

  // Message handlers.
  void OnGuestAttached(int element_instance_id,
                       int guest_proxy_routing_id);

  // Returns the frame which is embedding the corresponding plugin element.
  content::RenderFrame* GetEmbedderRenderFrame() const;

  bool guest_created() const { return guest_created_; }

  // Used when network service is enabled:
  bool waiting_to_create_throttle_ = false;

  // The URL of the extension to navigate to.
  std::string view_id_;

  // Whether the plugin is embedded or not.
  const bool is_embedded_;

  // The original URL of the plugin.
  const GURL original_url_;

  // Path of the plugin.
  const std::string plugin_path_;

  // The MIME type of the plugin.
  const std::string mime_type_;

  // Used when network service is enabled:
  blink::mojom::TransferrableURLLoaderPtr transferrable_url_loader_;

  // Used when network service is disabled:
  // A URL loader to load the |original_url_| when the plugin is embedded. In
  // the embedded case, no URL request is made automatically.
  std::unique_ptr<blink::WebAssociatedURLLoader> loader_;

  // True if a guest process has been requested.
  bool guest_created_ = false;

  // The routing ID of the frame which contains the plugin element.
  const int32_t embedder_render_frame_routing_id_;

  mojo::Receiver<mime_handler::BeforeUnloadControl>
      before_unload_control_receiver_{this};

  // The RenderView routing ID of the guest.
  int guest_proxy_routing_id_ = -1;

  // Determines whether the embedder can access |original_url_|. Used for UMA.
  const bool is_resource_accessible_to_embedder_;

  // The size of the element.
  base::Optional<gfx::Size> element_size_;

  base::WeakPtrFactory<MimeHandlerViewContainer> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MimeHandlerViewContainer);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_CONTAINER_H_
