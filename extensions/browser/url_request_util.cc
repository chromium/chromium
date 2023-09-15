// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/url_request_util.h"

#include <string>

#include "base/strings/string_piece.h"
#include "base/types/optional_util.h"
#include "extensions/browser/extension_navigation_ui_data.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"
#include "extensions/common/manifest_handlers/webview_info.h"
#include "services/network/public/cpp/request_destination.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/blink/public/common/loader/resource_type_util.h"

namespace extensions {
namespace url_request_util {

bool AllowCrossRendererResourceLoad(
    const network::ResourceRequest& request,
    network::mojom::RequestDestination destination,
    ui::PageTransition page_transition,
    int child_id,
    bool is_incognito,
    const Extension* extension,
    const ExtensionSet& extensions,
    const ProcessMap& process_map,
    bool* allowed) {
  const GURL& url = request.url;
  base::StringPiece resource_path = url.path_piece();

  // This logic is performed for main frame requests in
  // ExtensionNavigationThrottle::WillStartRequest.
  if (child_id != -1 ||
      destination != network::mojom::RequestDestination::kDocument) {
    // Extensions with webview: allow loading certain resources by guest
    // renderers with privileged partition IDs as specified in owner's extension
    // the manifest file.
    std::string owner_extension_id;
    int owner_process_id;
    WebViewRendererState::GetInstance()->GetOwnerInfo(
        child_id, &owner_process_id, &owner_extension_id);
    const Extension* owner_extension = extensions.GetByID(owner_extension_id);
    std::string partition_id;
    bool is_guest = WebViewRendererState::GetInstance()->GetPartitionID(
        child_id, &partition_id);

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
  base::StringPiece resource_root_relative_path =
      url.path_piece().empty() ? base::StringPiece()
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

  // When navigating in subframe, allow if it is the same origin
  // as the top-level frame. This can only be the case if the subframe
  // request is coming from the extension process.
  if (network::IsRequestDestinationEmbeddedFrame(destination) &&
      process_map.Contains(child_id)) {
    *allowed = true;
    return true;
  }

  // Allow web accessible extension resources to be loaded as
  // subresources/sub-frames.
  if (WebAccessibleResourcesInfo::IsResourceWebAccessible(
          extension, std::string(resource_path),
          base::OptionalToPtr(request.request_initiator))) {
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
                                          base::StringPiece resource_path,
                                          ui::PageTransition page_transition,
                                          bool* allowed) {
  if (is_guest) {
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

}  // namespace url_request_util
}  // namespace extensions
