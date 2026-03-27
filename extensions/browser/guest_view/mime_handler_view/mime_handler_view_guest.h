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
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace content {
class WebContents;
struct ContextMenuParams;
}  // namespace content

namespace extensions {
class MimeHandlerViewGuestDelegate;
class StreamContainer;

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
  void GuestOverrideRendererPreferences(
      blink::RendererPreferences& preferences) final;

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
  void CreateInnerPage(std::unique_ptr<GuestViewBase> owned_this,
                       scoped_refptr<content::SiteInstance> site_instance,
                       const base::DictValue& create_params,
                       GuestPageCreatedCallback callback) override;
  void DidAttachToEmbedder() override;
  void DidInitialize(const base::DictValue& create_params) final;
  void MaybeRecreateGuestContents(
      content::RenderFrameHost* outer_contents_frame) final;
  void EmbedderFullscreenToggled(bool entered_fullscreen) final;
  bool ZoomPropagatesFromEmbedderToGuest() const final;
  void GuestViewDocumentOnLoadCompleted() final;

  // BrowserPluginGuestDelegate implementation.
  content::RenderFrameHost* GetProspectiveOuterDocument() final;

  // GuestpageHolder::Delegate implementation.
  bool GuestHandleContextMenu(content::RenderFrameHost& render_frame_host,
                              const content::ContextMenuParams& params) final;
  content::JavaScriptDialogManager* GuestGetJavascriptDialogManager() final;

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
      content::RenderFrameHost* opener,
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
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      const content::StoragePartitionConfig& partition_config,
      content::SessionStorageNamespace* session_storage_namespace) override;

  // Updates the fullscreen state for the guest. Returns whether the change
  // needs to be propagated to the embedder.
  bool SetFullscreenState(bool is_fullscreen);

  // content::WebContentsObserver implementation.
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
