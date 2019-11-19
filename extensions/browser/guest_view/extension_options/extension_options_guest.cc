// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/extension_options/extension_options_guest.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "components/crx_file/id_util.h"
#include "components/guest_view/browser/guest_view_event.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/result_codes.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/bad_message.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/guest_view/extension_options/extension_options_constants.h"
#include "extensions/browser/guest_view/extension_options/extension_options_guest_delegate.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/api/extension_options_internal.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "ipc/ipc_message_macros.h"

using content::WebContents;
using guest_view::GuestViewBase;
using guest_view::GuestViewEvent;

namespace extensions {

// static
const char ExtensionOptionsGuest::Type[] = "extensionoptions";

ExtensionOptionsGuest::ExtensionOptionsGuest(WebContents* owner_web_contents)
    : GuestView<ExtensionOptionsGuest>(owner_web_contents),
      extension_options_guest_delegate_(
          extensions::ExtensionsAPIClient::Get()
              ->CreateExtensionOptionsGuestDelegate(this)) {}

ExtensionOptionsGuest::~ExtensionOptionsGuest() {
}

// static
GuestViewBase* ExtensionOptionsGuest::Create(WebContents* owner_web_contents) {
  return new ExtensionOptionsGuest(owner_web_contents);
}

void ExtensionOptionsGuest::CreateWebContents(
    const base::DictionaryValue& create_params,
    WebContentsCreatedCallback callback) {
  // Get the extension's base URL.
  std::string extension_id;
  create_params.GetString(extensionoptions::kExtensionId, &extension_id);

  if (!crx_file::id_util::IdIsValid(extension_id)) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::string embedder_extension_id = GetOwnerSiteURL().host();
  if (crx_file::id_util::IdIsValid(embedder_extension_id) &&
      extension_id != embedder_extension_id) {
    // Extensions cannot embed other extensions' options pages.
    std::move(callback).Run(nullptr);
    return;
  }

  GURL extension_url =
      extensions::Extension::GetBaseURLFromExtensionId(extension_id);
  if (!extension_url.is_valid()) {
    std::move(callback).Run(nullptr);
    return;
  }

  // Get the options page URL for later use.
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser_context());
  const extensions::Extension* extension =
      registry->enabled_extensions().GetByID(extension_id);
  if (!extension) {
    // The ID was valid but the extension didn't exist. Typically this will
    // happen when an extension is disabled.
    std::move(callback).Run(nullptr);
    return;
  }

  options_page_ = extensions::OptionsPageInfo::GetOptionsPage(extension);
  if (!options_page_.is_valid()) {
    std::move(callback).Run(nullptr);
    return;
  }

  // Create a WebContents using the extension URL. The options page's
  // WebContents should live in the same process as its parent extension's
  // WebContents, so we can use |extension_url| for creating the SiteInstance.
  WebContents::CreateParams params(
      browser_context(),
      content::SiteInstance::CreateForURL(browser_context(), extension_url));
  params.guest_delegate = this;
  // TODO(erikchen): Fix ownership semantics for guest views.
  // https://crbug.com/832879.
  std::move(callback).Run(WebContents::Create(params).release());
}

void ExtensionOptionsGuest::DidInitialize(
    const base::DictionaryValue& create_params) {
  ExtensionsAPIClient::Get()->AttachWebContentsHelpers(web_contents());
  web_contents()->GetController().LoadURL(options_page_,
                                          content::Referrer(),
                                          ui::PAGE_TRANSITION_LINK,
                                          std::string());
}

void ExtensionOptionsGuest::GuestViewDidStopLoading() {
  std::unique_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  DispatchEventToView(std::make_unique<GuestViewEvent>(
      api::extension_options_internal::OnLoad::kEventName, std::move(args)));
}

const char* ExtensionOptionsGuest::GetAPINamespace() const {
  return extensionoptions::kAPINamespace;
}

int ExtensionOptionsGuest::GetTaskPrefix() const {
  return IDS_EXTENSION_TASK_MANAGER_EXTENSIONOPTIONS_TAG_PREFIX;
}

bool ExtensionOptionsGuest::IsPreferredSizeModeEnabled() const {
  return true;
}

void ExtensionOptionsGuest::OnPreferredSizeChanged(const gfx::Size& pref_size) {
  api::extension_options_internal::PreferredSizeChangedOptions options;
  // Convert the size from physical pixels to logical pixels.
  options.width = PhysicalPixelsToLogicalPixels(pref_size.width());
  options.height = PhysicalPixelsToLogicalPixels(pref_size.height());
  DispatchEventToView(std::make_unique<GuestViewEvent>(
      api::extension_options_internal::OnPreferredSizeChanged::kEventName,
      options.ToValue()));
}

void ExtensionOptionsGuest::AddNewContents(
    WebContents* source,
    std::unique_ptr<WebContents> new_contents,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_rect,
    bool user_gesture,
    bool* was_blocked) {
  // |new_contents| is potentially used as a non-embedded WebContents, so we
  // check that it isn't a guest. The only place that this method should be
  // called is WebContentsImpl::ViewSource - which generates a non-guest
  // WebContents.
  DCHECK(!ExtensionOptionsGuest::FromWebContents(new_contents.get()));
  if (!attached() || !embedder_web_contents()->GetDelegate())
    return;

  embedder_web_contents()->GetDelegate()->AddNewContents(
      source, std::move(new_contents), disposition, initial_rect, user_gesture,
      was_blocked);
}

WebContents* ExtensionOptionsGuest::OpenURLFromTab(
    WebContents* source,
    const content::OpenURLParams& params) {
  if (!extension_options_guest_delegate_)
    return nullptr;

  // Don't allow external URLs with the CURRENT_TAB disposition be opened in
  // this guest view, change the disposition to NEW_FOREGROUND_TAB.
  if ((!params.url.SchemeIs(extensions::kExtensionScheme) ||
       params.url.host() != options_page_.host()) &&
      params.disposition == WindowOpenDisposition::CURRENT_TAB) {
    return extension_options_guest_delegate_->OpenURLInNewTab(
        content::OpenURLParams(
            params.url, params.referrer, params.frame_tree_node_id,
            WindowOpenDisposition::NEW_FOREGROUND_TAB, params.transition,
            params.is_renderer_initiated));
  }
  return extension_options_guest_delegate_->OpenURLInNewTab(params);
}

void ExtensionOptionsGuest::CloseContents(WebContents* source) {
  DispatchEventToView(std::make_unique<GuestViewEvent>(
      api::extension_options_internal::OnClose::kEventName,
      base::WrapUnique(new base::DictionaryValue())));
}

bool ExtensionOptionsGuest::HandleContextMenu(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
  if (!extension_options_guest_delegate_)
    return false;

  return extension_options_guest_delegate_->HandleContextMenu(params);
}

bool ExtensionOptionsGuest::IsWebContentsCreationOverridden(
    content::SiteInstance* source_site_instance,
    content::mojom::WindowContainerType window_container_type,
    const GURL& opener_url,
    const std::string& frame_name,
    const GURL& target_url) {
  // This method handles opening links from within the guest. Since this guest
  // view is used for displaying embedded extension options, we want any
  // external links to be opened in a new tab, not in a new guest view so we
  // override creation.
  return true;
}

WebContents* ExtensionOptionsGuest::CreateCustomWebContents(
    content::RenderFrameHost* opener,
    content::SiteInstance* source_site_instance,
    bool is_renderer_initiated,
    const GURL& opener_url,
    const std::string& frame_name,
    const GURL& target_url,
    const std::string& partition_id,
    content::SessionStorageNamespace* session_storage_namespace) {
  // To get links out of the guest view, we just open the URL in a new tab.
  // TODO(ericzeng): Open the tab in the background if the click was a
  //   ctrl-click or middle mouse button click
  if (extension_options_guest_delegate_) {
    extension_options_guest_delegate_->OpenURLInNewTab(
        content::OpenURLParams(target_url, content::Referrer(),
                               WindowOpenDisposition::NEW_FOREGROUND_TAB,
                               ui::PAGE_TRANSITION_LINK, false));
  }

  // Returning nullptr here ensures that the guest-view can never get a
  // reference to the new WebContents. It effectively forces a new browsing
  // instance for all popups from an extensions guest.
  return nullptr;
}

void ExtensionOptionsGuest::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() || !attached()) {
    return;
  }

  auto* guest_zoom_controller =
      zoom::ZoomController::FromWebContents(web_contents());
  guest_zoom_controller->SetZoomMode(zoom::ZoomController::ZOOM_MODE_ISOLATED);
  SetGuestZoomLevelToMatchEmbedder();

  if (!url::IsSameOriginWith(navigation_handle->GetURL(), options_page_)) {
    bad_message::ReceivedBadMessage(
        web_contents()->GetMainFrame()->GetProcess(),
        bad_message::EOG_BAD_ORIGIN);
  }
}

}  // namespace extensions
