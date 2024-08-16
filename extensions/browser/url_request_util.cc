// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/url_request_util.h"

#include <string_view>

#include "base/types/optional_util.h"
#include "extensions/browser/extension_navigation_ui_data.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"
#include "extensions/common/manifest_handlers/webview_info.h"
#include "pdf/buildflags.h"
#include "services/network/public/cpp/request_destination.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/blink/public/common/loader/resource_type_util.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_GUEST_VIEW)
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#endif

#if BUILDFLAG(ENABLE_PDF)
#include "pdf/pdf_features.h"
#endif  // BUILDFLAG(ENABLE_PDF)

namespace extensions::url_request_util {

bool AllowCrossRendererResourceLoad(
    const network::ResourceRequest& request,
    network::mojom::RequestDestination destination,
    ui::PageTransition page_transition,
    int child_id,
    bool is_incognito,
    const Extension* extension,
    const ExtensionSet& extensions,
    const ProcessMap& process_map,
    const GURL& upstream_url,
    bool* allowed) {
  const GURL& url = request.url;
  std::string_view resource_path = url.path_piece();

  // This logic is performed for main frame requests in
  // ExtensionNavigationThrottle::WillStartRequest.
  if (child_id != -1 ||
      destination != network::mojom::RequestDestination::kDocument) {
    // Extensions with webview: allow loading certain resources by guest
    // renderers with privileged partition IDs as specified in owner's extension
    // the manifest file.
    bool is_guest = false;
    std::string partition_id;
    const Extension* owner_extension = nullptr;

#if BUILDFLAG(ENABLE_GUEST_VIEW)
    int owner_process_id;
    std::string owner_extension_id;
    WebViewRendererState::GetInstance()->GetOwnerInfo(
        child_id, &owner_process_id, &owner_extension_id);
    owner_extension = extensions.GetByID(owner_extension_id);
    is_guest = WebViewRendererState::GetInstance()->GetPartitionID(
        child_id, &partition_id);
#endif

    if (AllowCrossRendererResourceLoadHelper(
            is_guest, extension, owner_extension, partition_id, resource_path,
            page_transition, allowed)) {
      return true;
    }
  }

  // The following checks require that we have an actual extension object. If we
  // don't have it, allow the request handling to continue with the rest of the
  // checks.
  if (!extension) {
    *allowed = true;
    return true;
  }

  // Disallow loading of packaged resources for hosted apps. We don't allow
  // hybrid hosted/packaged apps. The one exception is access to icons, since
  // some extensions want to be able to do things like create their own
  // launchers.
  std::string_view resource_root_relative_path =
      url.path_piece().empty() ? std::string_view()
                               : url.path_piece().substr(1);
  if (extension->is_hosted_app() &&
      !IconsInfo::GetIcons(extension)
           .ContainsPath(resource_root_relative_path)) {
    LOG(ERROR) << "Denying load of " << url.spec() << " from hosted app.";
    *allowed = false;
    return true;
  }

  DCHECK_EQ(extension->url(), url.GetWithEmptyPath());

  // Navigating the main frame to an extension URL is allowed, even if not
  // explicitly listed as web_accessible_resource.
  if (destination == network::mojom::RequestDestination::kDocument) {
    *allowed = true;
    return true;
  }

  // When navigating in subframe, verify that the extension the resource is
  // loaded from matches the process loading it.
  if (network::IsRequestDestinationEmbeddedFrame(destination) &&
      process_map.Contains(extension->id(), child_id)) {
    *allowed = true;
    return true;
  }

  // If the request is initiated by an opaque origin, allow it if the origin's
  // precursor matches the extension. This allows sandboxed data URLs and srcdoc
  // documents from an extension to access its resources (necessary for
  // backwards compatibility), even if they rendered in a non-extension process.
  if (request.request_initiator && request.request_initiator.value().opaque()) {
    const GURL precursor_url = request.request_initiator.value()
                                   .GetTupleOrPrecursorTupleIfOpaque()
                                   .GetURL();
    if (extension->origin() == url::Origin::Create(precursor_url)) {
      *allowed = true;
      return true;
    }
  }

  // Allow web accessible extension resources to be loaded as
  // subresources/sub-frames.
  if (url.SchemeIs(extensions::kExtensionScheme) &&
      WebAccessibleResourcesInfo::IsResourceWebAccessibleRedirect(
          extension, url, request.request_initiator, upstream_url)) {
    *allowed = true;
    return true;
  }

  if (!ui::PageTransitionIsWebTriggerable(page_transition)) {
    *allowed = false;
    return true;
  }

  // Couldn't determine if the resource is allowed or not.
  return false;
}

bool AllowCrossRendererResourceLoadHelper(bool is_guest,
                                          const Extension* extension,
                                          const Extension* owner_extension,
                                          const std::string& partition_id,
                                          std::string_view resource_path,
                                          ui::PageTransition page_transition,
                                          bool* allowed) {
  if (is_guest) {
#if BUILDFLAG(ENABLE_PDF)
    // Allow the PDF Viewer extension to load in guests.
    if (chrome_pdf::features::IsOopifPdfEnabled() &&
        extension->id() == extension_misc::kPdfExtensionId) {
      *allowed = true;
      return true;
    }
#endif  // BUILDFLAG(ENABLE_PDF)

    // An extension's resources should only be accessible to WebViews owned by
    // that extension.
    if (owner_extension != extension) {
      *allowed = false;
      return true;
    }

    *allowed = WebviewInfo::IsResourceWebviewAccessible(
        extension, partition_id, std::string(resource_path));
    return true;
  }

  return false;
}

}  // namespace extensions::url_request_util
