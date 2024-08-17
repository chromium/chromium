// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/resource_request_policy.h"

#include <string_view>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/types/optional_util.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"
#include "extensions/common/manifest_handlers/webview_info.h"
#include "extensions/renderer/dispatcher.h"
#include "pdf/buildflags.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

ResourceRequestPolicy::ResourceRequestPolicy(Dispatcher* dispatcher,
                                             std::unique_ptr<Delegate> delegate)
    : dispatcher_(dispatcher), delegate_(std::move(delegate)) {}
ResourceRequestPolicy::~ResourceRequestPolicy() = default;

void ResourceRequestPolicy::OnExtensionLoaded(const Extension& extension) {
  if (WebAccessibleResourcesInfo::HasWebAccessibleResources(&extension) ||
      WebviewInfo::HasWebviewAccessibleResources(
          extension,
          dispatcher_->webview_partition_id().value_or(std::string())) ||
      // Hosted app icons are accessible.
      // TODO(devlin): Should we incorporate this into
      // WebAccessibleResourcesInfo?
      (extension.is_hosted_app() && !IconsInfo::GetIcons(&extension).empty())) {
    web_accessible_resources_map_[extension.id()] = extension.guid();
  }
}

bool ResourceRequestPolicy::IsWebAccessibleHost(const std::string& host) {
  if (web_accessible_resources_map_.find(host) !=
      web_accessible_resources_map_.end()) {
    return true;
  }
  for (const auto& [id, guid] : web_accessible_resources_map_) {
    if (host == guid) {
      return true;
    }
  }
  return false;
}

void ResourceRequestPolicy::OnExtensionUnloaded(
    const ExtensionId& extension_id) {
  web_accessible_resources_map_.erase(extension_id);
}

// This method does a security check whether chrome-extension:// URLs can be
// requested by the renderer. Since this is in an untrusted process, the browser
// has a similar check to enforce the policy, in case this process is exploited.
// If you are changing this function, ensure equivalent checks are added to
// extension_protocols.cc's AllowExtensionResourceLoad.
bool ResourceRequestPolicy::CanRequestResource(
    const GURL& upstream_url,
    const GURL& target_url,
    blink::WebLocalFrame* frame,
    ui::PageTransition transition_type,
    const url::Origin* initiator_origin) {
  // `target_url` is expected to have a chrome-extension scheme.
  // `upstream_url` could be empty, have a chrome-extension scheme, or other.
  CHECK(target_url.SchemeIs(kExtensionScheme));

  GURL frame_url = frame->GetDocument().Url();
  url::Origin frame_origin = frame->GetDocument().GetSecurityOrigin();

  // Navigations from chrome://, devtools://, or embedder-specified pages need
  // to be allowed, even if the target |url| is not web-accessible.  See also:
  // - https://crbug.com/662602
  // - similar scheme checks in ExtensionNavigationThrottle
  if (frame_origin.scheme() == content::kChromeUIScheme ||
      frame_origin.scheme() == content::kChromeDevToolsScheme) {
    return true;
  }
  if (delegate_ &&
      delegate_->ShouldAlwaysAllowRequestForFrameOrigin(frame_origin)) {
    return true;
  }

  // The page_origin may be GURL("null") for unique origins like data URLs,
  // but this is ok for the checks below.  We only care if it matches the
  // current extension or has a devtools scheme.
  GURL page_origin = url::Origin(frame->Top()->GetSecurityOrigin()).GetURL();

  GURL extension_origin = target_url.DeprecatedGetOriginAsURL();

  // We always allow loads in the following cases, regardless of web accessible
  // resources:

  // Empty urls (needed for some edge cases when we have empty urls).
  if (frame_url.is_empty()) {
    return true;
  }

  // Extensions requesting their own resources (frame_url check is for images,
  // page_url check is for iframes).
  // TODO(devlin): We should be checking the ancestor chain, not just the
  // top-level frame. Additionally, we should be checking the security origin
  // of the frame, to account for about:blank subframes being scripted by an
  // extension parent (though we'll still need the frame origin check for
  // sandboxed frames).
  if (frame_url.DeprecatedGetOriginAsURL() == extension_origin ||
      page_origin == extension_origin) {
    return true;
  }

  if (!ui::PageTransitionIsWebTriggerable(transition_type)) {
    return true;
  }

  // Unreachable web page error page (to allow showing the icon of the
  // unreachable app on this page).
  if (frame_url == content::kUnreachableWebDataURL) {
    return true;
  }

#if BUILDFLAG(ENABLE_PDF)
  // Handle specific cases for the PDF viewer.
  if (extension_origin.scheme() == kExtensionScheme &&
      extension_origin.host() == extension_misc::kPdfExtensionId) {
    // For the PDF viewer, `page_origin` doesn't match the `extension_origin`,
    // but the PDF extension frame should still be able to request resources
    // from itself. The PDF content frame should also be able to request
    // resources from the PDF extension. For both cases, the parent origin of
    // the current frame matches the extension origin.
    blink::WebFrame* parent = frame->Parent();
    if (parent) {
      GURL parent_origin = url::Origin(parent->GetSecurityOrigin()).GetURL();
      if (parent_origin == extension_origin) {
        return true;
      }
    }
  }
#endif  // BUILDFLAG(ENABLE_PDF)

  if (delegate_ &&
      delegate_->AllowLoadForDevToolsPage(page_origin, target_url)) {
    return true;
  }

  // Note: we check |web_accessible_ids_| (rather than first looking up the
  // extension in the registry and checking that) to be more resistant against
  // timing attacks. This way, determining access for an extension that isn't
  // installed takes the same amount of time as determining access for an
  // extension with no web accessible resources. We aren't worried about any
  // extensions with web accessible resources, since those are inherently
  // identifiable.
  if (!IsWebAccessibleHost(extension_origin.host())) {
    return false;
  }

  const Extension* extension =
      RendererExtensionRegistry::Get()->GetExtensionOrAppByURL(
          target_url, true /*include_guid*/);
  DCHECK(extension);

  // Disallow loading of packaged resources for hosted apps. We don't allow
  // hybrid hosted/packaged apps. The one exception is access to icons, since
  // some extensions want to be able to do things like create their own
  // launchers.
  std::string_view resource_root_relative_path =
      target_url.path_piece().empty() ? std::string_view()
                                      : target_url.path_piece().substr(1);
  if (extension->is_hosted_app() &&
      !IconsInfo::GetIcons(extension).ContainsPath(
          resource_root_relative_path)) {
    LOG(ERROR) << "Denying load of " << target_url.spec() << " from "
               << "hosted app.";
    return false;
  }

  // Disallow loading of extension resources which are not explicitly listed
  // as web or WebView accessible if the manifest version is 2 or greater.
  // `upstream_url` might be set to GURL() in some cases, if it's not available.
  auto opt_initiator_origin = base::OptionalFromPtr(initiator_origin);
  if (!WebAccessibleResourcesInfo::IsResourceWebAccessibleRedirect(
          extension, target_url, opt_initiator_origin, upstream_url) &&
      !WebviewInfo::IsResourceWebviewAccessible(
          extension,
          dispatcher_->webview_partition_id().value_or(std::string()),
          target_url.path())) {
    std::string message = base::StringPrintf(
        "Denying load of %s. Resources must be listed in the "
        "web_accessible_resources manifest key in order to be loaded by "
        "pages outside the extension.",
        target_url.spec().c_str());
    frame->AddMessageToConsole(
        blink::WebConsoleMessage(blink::mojom::ConsoleMessageLevel::kError,
                                 blink::WebString::FromUTF8(message)));
    return false;
  }

  return true;
}

}  // namespace extensions
