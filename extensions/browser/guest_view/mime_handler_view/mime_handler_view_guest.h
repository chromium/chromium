// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_GUEST_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_GUEST_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/guest_view/browser/guest_view.h"
#include "content/public/common/transferrable_url_loader.mojom.h"
#include "extensions/common/api/mime_handler.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace content {
class WebContents;
struct ContextMenuParams;
struct StreamInfo;
}  // namespace content

namespace extensions {
class MimeHandlerViewGuestDelegate;

// A container for a StreamHandle and any other information necessary for a
// MimeHandler to handle a resource stream.
class StreamContainer {
 public:
  StreamContainer(
      std::unique_ptr<content::StreamInfo> stream,
      int tab_id,
      bool embedded,
      const GURL& handler_url,
      const std::string& extension_id,
      content::mojom::TransferrableURLLoaderPtr transferrable_loader,
      const GURL& original_url);
  ~StreamContainer();

  // Aborts the stream.
  void Abort(const base::Closure& callback);

  base::WeakPtr<StreamContainer> GetWeakPtr();

  content::mojom::TransferrableURLLoaderPtr TakeTransferrableURLLoader();

  bool embedded() const { return embedded_; }
  int tab_id() const { return tab_id_; }
  GURL handler_url() const { return handler_url_; }
  std::string extension_id() const { return extension_id_; }

  const std::string& mime_type() const { return mime_type_; }
  const GURL& original_url() const { return original_url_; }
  const GURL& stream_url() const { return stream_url_; }
  net::HttpResponseHeaders* response_headers() const {
    return response_headers_.get();
  }

 private:
  const std::unique_ptr<content::StreamInfo> stream_;
  const bool embedded_;
  const int tab_id_;
  const GURL handler_url_;
  const std::string extension_id_;
  content::mojom::TransferrableURLLoaderPtr transferrable_loader_;

  std::string mime_type_;
  GURL original_url_;
  GURL stream_url_;
  scoped_refptr<net::HttpResponseHeaders> response_headers_;

  base::WeakPtrFactory<StreamContainer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(StreamContainer);
};

class MimeHandlerViewGuest
    : public guest_view::GuestView<MimeHandlerViewGuest> {
 public:
  static guest_view::GuestViewBase* Create(
      content::WebContents* owner_web_contents);

  static const char Type[];

  // BrowserPluginGuestDelegate overrides.
  bool CanUseCrossProcessFrames() override;
  bool CanBeEmbeddedInsideCrossProcessFrames() override;
  content::RenderWidgetHost* GetOwnerRenderWidgetHost() override;
  content::SiteInstance* GetOwnerSiteInstance() override;

  void SetEmbedderFrame(int process_id, int routing_id);

  void SetBeforeUnloadController(
      mime_handler::BeforeUnloadControlPtrInfo pending_before_unload_control);

  content::RenderFrameHost* GetEmbedderFrame() const;

 protected:
  explicit MimeHandlerViewGuest(content::WebContents* owner_web_contents);
  ~MimeHandlerViewGuest() override;

 private:
  friend class TestMimeHandlerViewGuest;

  // GuestViewBase implementation.
  const char* GetAPINamespace() const final;
  int GetTaskPrefix() const final;
  void CreateWebContents(const base::DictionaryValue& create_params,
                         WebContentsCreatedCallback callback) override;
  void DidAttachToEmbedder() override;
  void DidInitialize(const base::DictionaryValue& create_params) final;
  void EmbedderFullscreenToggled(bool entered_fullscreen) final;
  bool ZoomPropagatesFromEmbedderToGuest() const final;
  bool ShouldDestroyOnDetach() const final;

  // WebContentsDelegate implementation.
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) final;
  void NavigationStateChanged(content::WebContents* source,
                              content::InvalidateTypes changed_flags) final;
  bool HandleContextMenu(const content::ContextMenuParams& params) final;
  bool PreHandleGestureEvent(content::WebContents* source,
                             const blink::WebGestureEvent& event) final;
  content::JavaScriptDialogManager* GetJavaScriptDialogManager(
      content::WebContents* source) final;
  bool SaveFrame(const GURL& url, const content::Referrer& referrer) final;
  void OnRenderFrameHostDeleted(int process_id, int routing_id) final;
  void EnterFullscreenModeForTab(
      content::WebContents* web_contents,
      const GURL& origin,
      const blink::WebFullscreenOptions& options) override;
  void ExitFullscreenModeForTab(content::WebContents*) override;
  bool IsFullscreenForTabOrPending(
      const content::WebContents* web_contents) const override;
  bool ShouldCreateWebContents(
      content::WebContents* web_contents,
      content::RenderFrameHost* opener,
      content::SiteInstance* source_site_instance,
      int32_t route_id,
      int32_t main_frame_route_id,
      int32_t main_frame_widget_route_id,
      content::mojom::WindowContainerType window_container_type,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url,
      const std::string& partition_id,
      content::SessionStorageNamespace* session_storage_namespace) override;

  // Updates the fullscreen state for the guest. Returns whether the change
  // needs to be propagated to the embedder.
  bool SetFullscreenState(bool is_fullscreen);

  // content::WebContentsObserver implementation.
  void DocumentOnLoadCompletedInMainFrame() final;
  void OnInterfaceRequestFromFrame(
      content::RenderFrameHost* render_frame_host,
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle* interface_pipe) final;
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) final;

  void FuseBeforeUnloadControl(
      mime_handler::BeforeUnloadControlRequest request);

  std::unique_ptr<MimeHandlerViewGuestDelegate> delegate_;
  std::unique_ptr<StreamContainer> stream_;

  int embedder_frame_process_id_;
  int embedder_frame_routing_id_;
  int embedder_widget_routing_id_;

  service_manager::BinderRegistry registry_;

  bool is_guest_fullscreen_ = false;
  bool is_embedder_fullscreen_ = false;

  mime_handler::BeforeUnloadControlPtrInfo pending_before_unload_control_;

  DISALLOW_COPY_AND_ASSIGN(MimeHandlerViewGuest);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_GUEST_H_
