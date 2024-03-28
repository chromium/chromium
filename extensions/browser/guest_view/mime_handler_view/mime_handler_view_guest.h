// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_GUEST_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_GUEST_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/guest_view/browser/guest_view.h"
#include "content/public/browser/global_routing_id.h"
#include "extensions/common/api/mime_handler.mojom.h"
#include "extensions/common/extension_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/loader/transferrable_url_loader.mojom.h"

namespace content {
class WebContents;
struct ContextMenuParams;
}  // namespace content

namespace extensions {
class MimeHandlerViewGuestDelegate;

// A container for a information necessary for a MimeHandler to handle a
// resource stream.
class StreamContainer {
 public:
  StreamContainer(int tab_id,
                  bool embedded,
                  const GURL& handler_url,
                  const ExtensionId& extension_id,
                  blink::mojom::TransferrableURLLoaderPtr transferrable_loader,
                  const GURL& original_url);

  StreamContainer(const StreamContainer&) = delete;
  StreamContainer& operator=(const StreamContainer&) = delete;

  ~StreamContainer();

  base::WeakPtr<StreamContainer> GetWeakPtr();

  blink::mojom::TransferrableURLLoaderPtr TakeTransferrableURLLoader();

  bool embedded() const { return embedded_; }
  int tab_id() const { return tab_id_; }
  GURL handler_url() const { return handler_url_; }
  ExtensionId extension_id() const { return extension_id_; }

  const std::string& mime_type() const { return mime_type_; }
  const GURL& original_url() const { return original_url_; }
  const GURL& stream_url() const { return stream_url_; }
  net::HttpResponseHeaders* response_headers() const {
    return response_headers_.get();
  }

  const mime_handler::PdfPluginAttributesPtr& pdf_plugin_attributes() const {
    return pdf_plugin_attributes_;
  }
  void set_pdf_plugin_attributes(
      mime_handler::PdfPluginAttributesPtr pdf_plugin_attributes) {
    pdf_plugin_attributes_ = std::move(pdf_plugin_attributes);
  }

 private:
  const bool embedded_;
  const int tab_id_;
  const GURL handler_url_;
  const ExtensionId extension_id_;
  blink::mojom::TransferrableURLLoaderPtr transferrable_loader_;

  std::string mime_type_;
  GURL original_url_;
  GURL stream_url_;
  scoped_refptr<net::HttpResponseHeaders> response_headers_;
  mime_handler::PdfPluginAttributesPtr pdf_plugin_attributes_;

  base::WeakPtrFactory<StreamContainer> weak_factory_{this};
};

class MimeHandlerViewGuest
    : public guest_view::GuestView<MimeHandlerViewGuest> {
 public:
  ~MimeHandlerViewGuest() override;
  MimeHandlerViewGuest(const MimeHandlerViewGuest&) = delete;
  MimeHandlerViewGuest& operator=(const MimeHandlerViewGuest&) = delete;

  static std::unique_ptr<GuestViewBase> Create(
      content::RenderFrameHost* owner_rfh);

  static const char Type[];
  static const guest_view::GuestViewHistogramValue HistogramValue;

  // GuestViewBase overrides.
  bool CanBeEmbeddedInsideCrossProcessFrames() const override;

  content::RenderFrameHost* GetEmbedderFrame();

  void SetBeforeUnloadController(
      mojo::PendingRemote<mime_handler::BeforeUnloadControl>
          pending_before_unload_control);

  void SetPluginCanSave(bool can_save) { plugin_can_save_ = can_save; }

  void FuseBeforeUnloadControl(
      mojo::PendingReceiver<mime_handler::BeforeUnloadControl> receiver);

  // Asks the plugin to do save.
  bool PluginDoSave();

  void set_original_resource_url(const GURL& url) {
    original_resource_url_ = url;
  }

  // Returns true when the MHVG's embedder frame has a plugin element type of
  // frame owner. In such a case there might be a MHVFC assigned to MHVG in the
  // parent frame of the embedder frame (for post message).
  bool maybe_has_frame_container() const { return maybe_has_frame_container_; }

  const std::string& mime_type() const { return mime_type_; }

  base::WeakPtr<MimeHandlerViewGuest> GetWeakPtr();

  base::WeakPtr<StreamContainer> GetStreamWeakPtr();

 protected:
  explicit MimeHandlerViewGuest(content::RenderFrameHost* owner_rfh);

 private:
  friend class TestMimeHandlerViewGuest;

  // GuestViewBase implementation.
  const char* GetAPINamespace() const final;
  int GetTaskPrefix() const final;
  void CreateWebContents(std::unique_ptr<GuestViewBase> owned_this,
                         const base::Value::Dict& create_params,
                         WebContentsCreatedCallback callback) override;
  void DidAttachToEmbedder() override;
  void DidInitialize(const base::Value::Dict& create_params) final;
  void MaybeRecreateGuestContents(
      content::RenderFrameHost* outer_contents_frame) final;
  void EmbedderFullscreenToggled(bool entered_fullscreen) final;
  bool ZoomPropagatesFromEmbedderToGuest() const final;

  // BrowserPluginGuestDelegate implementation.
  content::RenderFrameHost* GetProspectiveOuterDocument() final;

  // WebContentsDelegate implementation.
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) final;
  void NavigationStateChanged(content::WebContents* source,
                              content::InvalidateTypes changed_flags) final;
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) final;
  bool PreHandleGestureEvent(content::WebContents* source,
                             const blink::WebGestureEvent& event) final;
  content::JavaScriptDialogManager* GetJavaScriptDialogManager(
      content::WebContents* source) final;
  bool GuestSaveFrame(content::WebContents* guest_web_contents) final;
  bool SaveFrame(const GURL& url,
                 const content::Referrer& referrer,
                 content::RenderFrameHost* render_frame_host) final;
  void EnterFullscreenModeForTab(
      content::RenderFrameHost* requesting_frame,
      const blink::mojom::FullscreenOptions& options) override;
  void ExitFullscreenModeForTab(content::WebContents*) override;
  bool IsFullscreenForTabOrPending(
      const content::WebContents* web_contents) override;
  bool ShouldResumeRequestsForCreatedWindow() override;
  bool IsWebContentsCreationOverridden(
      content::SiteInstance* source_site_instance,
      content::mojom::WindowContainerType window_container_type,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url) override;
  content::WebContents* CreateCustomWebContents(
      content::RenderFrameHost* opener,
      content::SiteInstance* source_site_instance,
      bool is_new_browsing_instance,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url,
      const content::StoragePartitionConfig& partition_config,
      content::SessionStorageNamespace* session_storage_namespace) override;

  // Updates the fullscreen state for the guest. Returns whether the change
  // needs to be propagated to the embedder.
  bool SetFullscreenState(bool is_fullscreen);

  // content::WebContentsObserver implementation.
  void DocumentOnLoadCompletedInPrimaryMainFrame() final;
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) final;
  void DidFinishNavigation(content::NavigationHandle* navigation_handle) final;

  std::unique_ptr<MimeHandlerViewGuestDelegate> delegate_;
  std::unique_ptr<StreamContainer> stream_;

  bool is_guest_fullscreen_ = false;
  bool is_embedder_fullscreen_ = false;
  bool plugin_can_save_ = false;
  GURL original_resource_url_;
  std::string mime_type_;

  // True when the MimeHandlerViewGeust might have a frame container in its
  // embedder's parent frame to facilitate postMessage.
  bool maybe_has_frame_container_ = false;
  mojo::PendingRemote<mime_handler::BeforeUnloadControl>
      pending_before_unload_control_;

  base::WeakPtrFactory<MimeHandlerViewGuest> weak_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_VIEW_GUEST_H_
