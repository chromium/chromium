// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSION_OPTIONS_EXTENSION_OPTIONS_GUEST_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSION_OPTIONS_EXTENSION_OPTIONS_GUEST_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "components/guest_view/browser/guest_view.h"
#include "extensions/browser/guest_view/extension_options/extension_options_guest_delegate.h"
#include "url/gurl.h"

namespace extensions {

class ExtensionOptionsGuest
    : public guest_view::GuestView<ExtensionOptionsGuest> {
 public:
  static const char Type[];
  static guest_view::GuestViewBase* Create(
      content::WebContents* owner_web_contents);

 private:
  explicit ExtensionOptionsGuest(content::WebContents* owner_web_contents);
  ~ExtensionOptionsGuest() override;

  // GuestViewBase implementation.
  void CreateWebContents(const base::DictionaryValue& create_params,
                         WebContentsCreatedCallback callback) final;
  void DidInitialize(const base::DictionaryValue& create_params) final;
  void GuestViewDidStopLoading() final;
  const char* GetAPINamespace() const final;
  int GetTaskPrefix() const final;
  bool IsPreferredSizeModeEnabled() const final;
  void OnPreferredSizeChanged(const gfx::Size& pref_size) final;

  // content::WebContentsDelegate implementation.
  void AddNewContents(content::WebContents* source,
                      std::unique_ptr<content::WebContents> new_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_rect,
                      bool user_gesture,
                      bool* was_blocked) final;
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) final;
  void CloseContents(content::WebContents* source) final;
  bool HandleContextMenu(content::RenderFrameHost* render_frame_host,
                         const content::ContextMenuParams& params) final;
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
      const std::string& partition_id,
      content::SessionStorageNamespace* session_storage_namespace) final;

  // content::WebContentsObserver implementation.
  void DidFinishNavigation(content::NavigationHandle* navigation_handle) final;

  std::unique_ptr<extensions::ExtensionOptionsGuestDelegate>
      extension_options_guest_delegate_;
  GURL options_page_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionOptionsGuest);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSION_OPTIONS_EXTENSION_OPTIONS_GUEST_H_
