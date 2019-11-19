// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_CONTAINER_BASE_H_
#define EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_CONTAINER_BASE_H_

#include <vector>

#include "base/macros.h"
#include "content/public/common/transferrable_url_loader.mojom.h"
#include "extensions/common/api/mime_handler.mojom.h"
#include "extensions/common/guest_view/mime_handler_view_uma_types.h"
#include "extensions/common/mojom/guest_view.mojom.h"
#include "extensions/renderer/guest_view/mime_handler_view/post_message_support.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "third_party/blink/public/web/web_associated_url_loader_client.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "v8/include/v8.h"

namespace blink {
class URLLoaderThrottle;
class WebAssociatedURLLoader;
}  // namespace blink

namespace content {
class RenderFrame;
struct WebPluginInfo;
}  // namespace content


namespace extensions {
// TODO(ekaramad): This class is no longer needed and should be merged into
// its subclass, MimeHandlerViewContainer (https://crbug.com/659750).
class MimeHandlerViewContainerBase : public blink::WebAssociatedURLLoaderClient,
                                     public mime_handler::BeforeUnloadControl,
                                     public PostMessageSupport::Delegate {
 public:
  MimeHandlerViewContainerBase(content::RenderFrame* embedder_render_frame,
                               const content::WebPluginInfo& info,
                               const std::string& mime_type,
                               const GURL& original_url);

  ~MimeHandlerViewContainerBase() override;

  static mojom::GuestView* GuestView();

  // TODO(ekaramad): Remove this and make MimeHandlerViewContainerManager of
  // |render_frame| hold on to the list of MimeHandlerViewContainerBase.
  static std::vector<MimeHandlerViewContainerBase*> FromRenderFrame(
      content::RenderFrame* render_frame);

  // If the URL matches the same URL that this object has created and it hasn't
  // added a throttle yet, it will return a new one for the purpose of
  // intercepting it.
  std::unique_ptr<blink::URLLoaderThrottle> MaybeCreatePluginThrottle(
      const GURL& url);

  // WebAssociatedURLLoaderClient overrides.
  void DidReceiveData(const char* data, int data_length) override;
  void DidFinishLoading() override;

 protected:
  MimeHandlerViewContainerBase();

  virtual void CreateMimeHandlerViewGuestIfNecessary();
  virtual int32_t GetInstanceId() const = 0;
  virtual gfx::Size GetElementSize() const = 0;

  void DidLoadInternal();
  void SendResourceRequest();
  void EmbedderRenderFrameWillBeGone();
  v8::Local<v8::Object> GetScriptableObjectInternal(v8::Isolate* isolate);
  void RecordInteraction(MimeHandlerViewUMATypes::Type uma_type);

  // Returns the frame which is embedding the corresponding plugin element.
  content::RenderFrame* GetEmbedderRenderFrame() const;

  bool guest_created() const { return guest_created_; }

  // Used when network service is enabled:
  bool waiting_to_create_throttle_ = false;

  // The URL of the extension to navigate to.
  std::string view_id_;

  // Whether the plugin is embedded or not.
  bool is_embedded_;

  // The original URL of the plugin.
  const GURL original_url_;

 private:
  class PluginResourceThrottle;

  // Called for embedded plugins when network service is enabled. This is called
  // by the URLLoaderThrottle which intercepts the resource load, which is then
  // sent to the browser to be handed off to the plugin.
  void SetEmbeddedLoader(
      content::mojom::TransferrableURLLoaderPtr transferrable_url_loader);

  // mime_handler::BeforeUnloadControl implementation.
  void SetShowBeforeUnloadDialog(
      bool show_dialog,
      SetShowBeforeUnloadDialogCallback callback) override;

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

  // True if a guest process has been requested.
  bool guest_created_ = false;

  // True if the guest page has fully loaded and its JavaScript onload function
  // has been called.
  bool guest_loaded_ = false;
  // The routing ID of the frame which contains the plugin element.
  const int32_t embedder_render_frame_routing_id_;

  mojo::Receiver<mime_handler::BeforeUnloadControl>
      before_unload_control_receiver_{this};

  base::WeakPtrFactory<MimeHandlerViewContainerBase> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MimeHandlerViewContainerBase);
};

}  // namespace extensions
#endif  // EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_CONTAINER_BASE_H_
