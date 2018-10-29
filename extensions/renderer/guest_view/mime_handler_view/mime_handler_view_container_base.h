// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_CONTAINER_BASE_H_
#define EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_CONTAINER_BASE_H_

#include <vector>

#include "base/macros.h"
#include "content/public/common/transferrable_url_loader.mojom.h"
#include "extensions/common/api/mime_handler.mojom.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "third_party/blink/public/web/web_associated_url_loader_client.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "v8/include/v8.h"

namespace blink {
class WebAssociatedURLLoader;
class WebRemoteFrame;
}  // namespace blink

namespace content {
class RenderFrame;
class URLLoaderThrottle;
struct WebPluginInfo;
}  // namespace content

namespace IPC {
class Message;
}

namespace extensions {
// A base class for MimeHandlerViewContainer which provides a way of reusing the
// common logic between the BrowserPlugin-based and frame-based container.
class MimeHandlerViewContainerBase : public blink::WebAssociatedURLLoaderClient,
                                     public mime_handler::BeforeUnloadControl {
 public:
  MimeHandlerViewContainerBase(content::RenderFrame* embedder_render_frame,
                               const content::WebPluginInfo& info,
                               const std::string& mime_type,
                               const GURL& original_url);

  ~MimeHandlerViewContainerBase() override;

  static std::vector<MimeHandlerViewContainerBase*> FromRenderFrame(
      content::RenderFrame* render_frame);

  static bool TryHandleMessage(const IPC::Message& message);

  // If the URL matches the same URL that this object has created and it hasn't
  // added a throttle yet, it will return a new one for the purpose of
  // intercepting it.
  std::unique_ptr<content::URLLoaderThrottle> MaybeCreatePluginThrottle(
      const GURL& url);

  // Post a JavaScript message to the guest.
  void PostJavaScriptMessage(v8::Isolate* isolate,
                             v8::Local<v8::Value> message);

  // Post |message| to the guest.
  void PostMessageFromValue(const base::Value& message);

  bool OnHandleMessage(const IPC::Message& message);

  // WebAssociatedURLLoaderClient overrides.
  void DidReceiveData(const char* data, int data_length) override;
  void DidFinishLoading() override;

 protected:
  virtual void CreateMimeHandlerViewGuestIfNecessary();
  virtual void OnRetryCreatingMimeHandlerViewGuest(int32_t element_instance_id);
  virtual void OnDestroyFrameContainer(int32_t element_instance_id);
  virtual blink::WebRemoteFrame* GetGuestProxyFrame() const = 0;
  virtual int32_t GetInstanceId() const = 0;
  virtual gfx::Size GetElementSize() const = 0;

  void OnMimeHandlerViewGuestOnLoadCompleted(int32_t element_instance_id);
  void SendResourceRequest();
  void EmbedderRenderFrameWillBeGone();
  v8::Local<v8::Object> GetScriptableObject(v8::Isolate* isolate);

  bool guest_created() const { return guest_created_; }

  // Used when network service is enabled:
  bool waiting_to_create_throttle_ = false;

  // The URL of the extension to navigate to.
  std::string view_id_;

  // Whether the plugin is embedded or not.
  bool is_embedded_;

  // The original URL of the plugin.
  const GURL original_url_;

  // Only valid for the cross-process-frame-based implementation. This holds the
  // routing ID of the frame or proxy whose corresponding WebFrame is the
  // ContentFrame() of the plugin element.
  int32_t plugin_frame_routing_id_ = MSG_ROUTING_NONE;

 private:
  class PluginResourceThrottle;

  // Returns the frame which is embedding the corresponding plugin element.
  content::RenderFrame* GetEmbedderRenderFrame() const;

  // Called for embedded plugins when network service is enabled. This is called
  // by the URLLoaderThrottle which intercepts the resource load, which is then
  // sent to the browser to be handed off to the plugin.
  void SetEmbeddedLoader(
      content::mojom::TransferrableURLLoaderPtr transferrable_url_loader);

  // Path of the plugin.
  const std::string plugin_path_;

  // The MIME type of the plugin.
  const std::string mime_type_;

  // Used when network service is enabled:
  content::mojom::TransferrableURLLoaderPtr transferrable_url_loader_;

  // Used when network service is disabled:
  // A URL loader to load the |original_url_| when the plugin is embedded. In
  // the embedded case, no URL request is made automatically.
  std::unique_ptr<blink::WebAssociatedURLLoader> loader_;

  // The scriptable object that backs the plugin.
  v8::Global<v8::Object> scriptable_object_;

  // Pending postMessage messages that need to be sent to the guest. These are
  // queued while the guest is loading and once it is fully loaded they are
  // delivered so that messages aren't lost.
  std::vector<v8::Global<v8::Value>> pending_messages_;

  // True if a guest process has been requested.
  bool guest_created_ = false;

  // True if the guest page has fully loaded and its JavaScript onload function
  // has been called.
  bool guest_loaded_ = false;

  // The routing ID of the frame which contains the plugin element.
  const int32_t embedder_render_frame_routing_id_;

  mojo::Binding<mime_handler::BeforeUnloadControl>
      before_unload_control_binding_;

  base::WeakPtrFactory<MimeHandlerViewContainerBase> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MimeHandlerViewContainerBase);
};

}  // namespace extensions
#endif  // EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_CONTAINER_BASE_H_
