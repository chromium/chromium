// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_container_base.h"

#include "base/bind.h"
#include "base/guid.h"
#include "base/lazy_instance.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "components/guest_view/common/guest_view_constants.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/guest_view/extensions_guest_view_messages.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "ipc/ipc_sync_channel.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_associated_url_loader.h"
#include "third_party/blink/public/web/web_associated_url_loader_options.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace extensions {
using UMAType = MimeHandlerViewUMATypes::Type;

namespace {

base::LazyInstance<mojom::GuestViewAssociatedPtr>::Leaky g_guest_view;

mojom::GuestView* GetGuestView() {
  if (!g_guest_view.Get()) {
    content::RenderThread::Get()->GetChannel()->GetRemoteAssociatedInterface(
        &g_guest_view.Get());
  }

  return g_guest_view.Get().get();
}

// Maps from content::RenderFrame to the set of MimeHandlerViewContainerBases
//  within it.
base::LazyInstance<
    std::map<content::RenderFrame*, std::set<MimeHandlerViewContainerBase*>>>::
    DestructorAtExit g_mime_handler_view_container_base_map =
        LAZY_INSTANCE_INITIALIZER;

}  // namespace

// Stores a raw pointer to MimeHandlerViewContainerBase since this throttle's
// lifetime is shorter (it matches |container|'s loader_).
class MimeHandlerViewContainerBase::PluginResourceThrottle
    : public blink::URLLoaderThrottle {
 public:
  explicit PluginResourceThrottle(
      base::WeakPtr<MimeHandlerViewContainerBase> container)
      : container_(container) {}
  ~PluginResourceThrottle() override {}

 private:
  // blink::URLLoaderThrottle overrides;
  void WillProcessResponse(const GURL& response_url,
                           network::mojom::URLResponseHead* response_head,
                           bool* defer) override {
    if (!container_) {
      // In the embedder case if the plugin element is removed right after an
      // ongoing request is made, MimeHandlerViewContainerBase is destroyed
      // synchronously but the WebURLLoaderImpl corresponding to this throttle
      // goes away asynchronously when ResourceLoader::CancelTimerFired() is
      // called (see https://crbug.com/878359).
      return;
    }
    network::mojom::URLLoaderPtr dummy_new_loader;
    mojo::MakeRequest(&dummy_new_loader);
    mojo::PendingRemote<network::mojom::URLLoaderClient> new_client;

    network::mojom::URLLoaderPtr original_loader;
    mojo::PendingReceiver<network::mojom::URLLoaderClient> original_client;
    delegate_->InterceptResponse(std::move(dummy_new_loader),
                                 new_client.InitWithNewPipeAndPassReceiver(),
                                 &original_loader, &original_client);

    auto transferrable_loader = content::mojom::TransferrableURLLoader::New();
    transferrable_loader->url_loader = original_loader.PassInterface();
    transferrable_loader->url_loader_client = std::move(original_client);

    // Make a deep copy of URLResponseHead before passing it cross-thread.
    auto deep_copied_response = response_head->Clone();
    if (response_head->headers) {
      deep_copied_response->headers =
          base::MakeRefCounted<net::HttpResponseHeaders>(
              response_head->headers->raw_headers());
    }
    transferrable_loader->head = std::move(deep_copied_response);
    container_->SetEmbeddedLoader(std::move(transferrable_loader));
  }

  base::WeakPtr<MimeHandlerViewContainerBase> container_;

  DISALLOW_COPY_AND_ASSIGN(PluginResourceThrottle);
};

MimeHandlerViewContainerBase::MimeHandlerViewContainerBase(
    content::RenderFrame* embedder_render_frame,
    const content::WebPluginInfo& info,
    const std::string& mime_type,
    const GURL& original_url)
    : original_url_(original_url),
      plugin_path_(info.path.MaybeAsASCII()),
      mime_type_(mime_type),
      embedder_render_frame_routing_id_(embedder_render_frame->GetRoutingID()) {
  DCHECK(!mime_type_.empty());
  g_mime_handler_view_container_base_map.Get()[embedder_render_frame].insert(
      this);
}

MimeHandlerViewContainerBase::~MimeHandlerViewContainerBase() {
  if (loader_) {
    DCHECK(is_embedded_);
    loader_->Cancel();
  }

  if (auto* rf = GetEmbedderRenderFrame()) {
    g_mime_handler_view_container_base_map.Get()[rf].erase(this);
    if (g_mime_handler_view_container_base_map.Get()[rf].empty())
      g_mime_handler_view_container_base_map.Get().erase(rf);
  }
}

// static
PostMessageSupport::Delegate* PostMessageSupport::Delegate::FromWebLocalFrame(
    blink::WebLocalFrame* web_local_frame) {
  if (!web_local_frame->GetDocument().IsPluginDocument())
    return nullptr;
  auto mime_handlers = MimeHandlerViewContainerBase::FromRenderFrame(
      content::RenderFrame::FromWebFrame(web_local_frame));
  if (mime_handlers.empty())
    return nullptr;
  return mime_handlers.front();
}

// static
mojom::GuestView* MimeHandlerViewContainerBase::GuestView() {
  return GetGuestView();
}

// static
std::vector<MimeHandlerViewContainerBase*>
MimeHandlerViewContainerBase::FromRenderFrame(
    content::RenderFrame* render_frame) {
  auto it = g_mime_handler_view_container_base_map.Get().find(render_frame);
  if (it == g_mime_handler_view_container_base_map.Get().end())
    return std::vector<MimeHandlerViewContainerBase*>();

  return std::vector<MimeHandlerViewContainerBase*>(it->second.begin(),
                                                    it->second.end());
}

std::unique_ptr<blink::URLLoaderThrottle>
MimeHandlerViewContainerBase::MaybeCreatePluginThrottle(const GURL& url) {
  if (!waiting_to_create_throttle_ || url != original_url_)
    return nullptr;

  waiting_to_create_throttle_ = false;
  return std::make_unique<PluginResourceThrottle>(weak_factory_.GetWeakPtr());
}

void MimeHandlerViewContainerBase::DidReceiveData(const char* data,
                                                  int data_length) {
  view_id_ += std::string(data, data_length);
}

void MimeHandlerViewContainerBase::DidFinishLoading() {
  DCHECK(is_embedded_);
  // Warning: It is possible that |this| gets destroyed after this line (when
  // the MHVCB is of the frame type and the associated plugin element does not
  // have a content frame).
  CreateMimeHandlerViewGuestIfNecessary();
}

content::RenderFrame* MimeHandlerViewContainerBase::GetEmbedderRenderFrame()
    const {
  DCHECK_NE(embedder_render_frame_routing_id_, MSG_ROUTING_NONE);
  return content::RenderFrame::FromRoutingID(embedder_render_frame_routing_id_);
}

void MimeHandlerViewContainerBase::CreateMimeHandlerViewGuestIfNecessary() {
  if (guest_created_)
    return;
  auto* embedder_render_frame = GetEmbedderRenderFrame();
  if (!embedder_render_frame) {
    // TODO(ekaramad): How can this happen? We should destroy the container if
    // this happens at all. The process is however different for a plugin-based
    // container.
    return;
  }

  auto* guest_view = GetGuestView();
  // Subresource requests like plugins are made directly from the renderer to
  // the network service. So we need to intercept the URLLoader and send it to
  // the browser so that it can forward it to the plugin.
  if (is_embedded_) {
    if (transferrable_url_loader_.is_null())
      return;

    auto* extension_frame_helper =
        ExtensionFrameHelper::Get(embedder_render_frame);
    if (!extension_frame_helper)
      return;

    guest_view->CreateEmbeddedMimeHandlerViewGuest(
        embedder_render_frame->GetRoutingID(), extension_frame_helper->tab_id(),
        original_url_, GetInstanceId(), GetElementSize(),
        std::move(transferrable_url_loader_));
    guest_created_ = true;
    return;
  }

  if (view_id_.empty())
    return;

  // The loader has completed loading |view_id_| so we can dispose it.
  if (loader_) {
    DCHECK(is_embedded_);
    loader_.reset();
  }

  DCHECK_NE(GetInstanceId(), guest_view::kInstanceIDNone);

  mojo::PendingRemote<mime_handler::BeforeUnloadControl> before_unload_control;
  if (!is_embedded_) {
    before_unload_control =
        before_unload_control_receiver_.BindNewPipeAndPassRemote();
  }
  guest_view->CreateMimeHandlerViewGuest(
      embedder_render_frame->GetRoutingID(), view_id_, GetInstanceId(),
      GetElementSize(), std::move(before_unload_control));

  guest_created_ = true;
}

void MimeHandlerViewContainerBase::DidLoadInternal() {
  RecordInteraction(UMAType::kDidLoadExtension);
  if (!GetEmbedderRenderFrame())
    return;
  guest_loaded_ = true;
  post_message_support()->SetActive();
}

void MimeHandlerViewContainerBase::SendResourceRequest() {
  blink::WebLocalFrame* frame = GetEmbedderRenderFrame()->GetWebFrame();

  blink::WebAssociatedURLLoaderOptions options;
  DCHECK(!loader_);
  loader_.reset(frame->CreateAssociatedURLLoader(options));

  // The embedded plugin is allowed to be cross-origin and we should always
  // send credentials/cookies with the request. So, use the default mode
  // "no-cors" and credentials mode "include".
  blink::WebURLRequest request(original_url_);
  request.SetRequestContext(blink::mojom::RequestContextType::OBJECT);
  // The plugin resource request should skip service workers since "plug-ins
  // may get their security origins from their own urls".
  // https://w3c.github.io/ServiceWorker/#implementer-concerns
  request.SetSkipServiceWorker(true);

  waiting_to_create_throttle_ = true;
  loader_->LoadAsynchronously(request, this);
}

void MimeHandlerViewContainerBase::EmbedderRenderFrameWillBeGone() {
  g_mime_handler_view_container_base_map.Get().erase(GetEmbedderRenderFrame());
}

void MimeHandlerViewContainerBase::SetEmbeddedLoader(
    content::mojom::TransferrableURLLoaderPtr transferrable_url_loader) {
  transferrable_url_loader_ = std::move(transferrable_url_loader);
  transferrable_url_loader_->url = GURL(plugin_path_ + base::GenerateGUID());
  // Warning: It is possible that |this| gets destroyed after this line (when
  // the MHVCB is of the frame type and the associated plugin element does not
  // have a content frame).
  CreateMimeHandlerViewGuestIfNecessary();
}

void MimeHandlerViewContainerBase::SetShowBeforeUnloadDialog(
    bool show_dialog,
    SetShowBeforeUnloadDialogCallback callback) {
  DCHECK(!is_embedded_);
  GetEmbedderRenderFrame()
      ->GetWebFrame()
      ->GetDocument()
      .SetShowBeforeUnloadDialog(show_dialog);
  std::move(callback).Run();
}

v8::Local<v8::Object> MimeHandlerViewContainerBase::GetScriptableObjectInternal(
    v8::Isolate* isolate) {
  return post_message_support()->GetScriptableObject(isolate);
}

void MimeHandlerViewContainerBase::RecordInteraction(UMAType uma_type) {
  base::UmaHistogramEnumeration(MimeHandlerViewUMATypes::kUMAName, uma_type);
}

}  // namespace extensions
