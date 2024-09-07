// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSION_OPTIONS_EXTENSION_OPTIONS_GUEST_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSION_OPTIONS_EXTENSION_OPTIONS_GUEST_H_

#include <stdint.h>

#include <memory>

#include "components/guest_view/browser/guest_view.h"
#include "extensions/browser/guest_view/extension_options/extension_options_guest_delegate.h"
#include "url/gurl.h"

namespace extensions {

class ExtensionOptionsGuest
    : public guest_view::GuestView<ExtensionOptionsGuest> {
 public:
  static const char Type[];
  static const guest_view::GuestViewHistogramValue HistogramValue;

  ~ExtensionOptionsGuest() override;
  ExtensionOptionsGuest(const ExtensionOptionsGuest&) = delete;
  ExtensionOptionsGuest& operator=(const ExtensionOptionsGuest&) = delete;

  static std::unique_ptr<GuestViewBase> Create(
      content::RenderFrameHost* owner_rfh);

 private:
  explicit ExtensionOptionsGuest(content::RenderFrameHost* owner_rfh);

  // GuestViewBase implementation.
  void CreateWebContents(std::unique_ptr<GuestViewBase> owned_this,
                         const base::Value::Dict& create_params,
                         WebContentsCreatedCallback callback) final;
  void DidInitialize(const base::Value::Dict& create_params) final;
  void MaybeRecreateGuestContents(
      content::RenderFrameHost* outer_contents_frame) final;
  void GuestViewDidStopLoading() final;
  const char* GetAPINamespace() const final;
  int GetTaskPrefix() const final;
  bool IsPreferredSizeModeEnabled() const final;
  void OnPreferredSizeChanged(const gfx::Size& pref_size) final;

  // content::WebContentsDelegate implementation.
  content::WebContents* AddNewContents(
      content::WebContents* source,
      std::unique_ptr<content::WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      bool user_gesture,
      bool* was_blocked) final;
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) final;
  void CloseContents(content::WebContents* source) final;
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) final;
  bool ShouldResumeRequestsForCreatedWindow() override;
  bool IsWebContentsCreationOverridden(
      content::SiteInstance* source_site_instance,
      content::mojom::WindowContainerType window_container_type,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url) final;
  content::WebContents* CreateCustomWebContents(
      content::RenderFrameHost* opener,
      content::SiteInstance* source_site_instance,
      bool is_new_browsing_instance,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url,
      const content::StoragePartitionConfig& partition_config,
      content::SessionStorageNamespace* session_storage_namespace) final;

  // content::WebContentsObserver implementation.
  void DidFinishNavigation(content::NavigationHandle* navigation_handle) final;

  std::unique_ptr<extensions::ExtensionOptionsGuestDelegate>
      extension_options_guest_delegate_;
  GURL options_page_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSION_OPTIONS_EXTENSION_OPTIONS_GUEST_H_
