// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/blocked_scheme_navigation_throttle.h"

#include "base/feature_list.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/common/features.h"
#include "content/common/navigation_params_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "url/url_constants.h"

namespace content {

namespace {
const char kConsoleError[] = "Not allowed to navigate top frame to %s URL: %s";
const char kAnyFrameConsoleError[] = "Not allowed to navigate to %s URL: %s";

bool IsExternalMountedFile(const GURL& url) {
  storage::FileSystemURL file_system_url =
      storage::ExternalMountPoints::GetSystemInstance()->CrackURL(
          url, blink::StorageKey::CreateFirstParty(url::Origin::Create(url)));
  return file_system_url.is_valid();
}
}

BlockedSchemeNavigationThrottle::BlockedSchemeNavigationThrottle(
    NavigationHandle* navigation_handle)
    : NavigationThrottle(navigation_handle) {}

BlockedSchemeNavigationThrottle::~BlockedSchemeNavigationThrottle() {}

NavigationThrottle::ThrottleCheckResult
BlockedSchemeNavigationThrottle::WillStartRequest() {
  NavigationRequest* request = NavigationRequest::From(navigation_handle());
  if (!request->GetURL().SchemeIs(url::kFileSystemScheme))
    return PROCEED;

  if (base::FeatureList::IsEnabled(blink::features::kFileSystemUrlNavigation))
    return PROCEED;

  RenderFrameHost* top_frame =
      request->frame_tree_node()->frame_tree().root()->current_frame_host();
  BrowserContext* browser_context = top_frame->GetBrowserContext();

  if (base::FeatureList::IsEnabled(
          blink::features::kFileSystemUrlNavigationForChromeAppsOnly) &&
      !IsExternalMountedFile(request->GetURL()) &&
      (url::Origin::Create(request->GetURL()) ==
       request->GetInitiatorOrigin()) &&
      GetContentClient()->browser()->IsFileSystemURLNavigationAllowed(
          browser_context, request->GetURL())) {
    return PROCEED;
  }

  top_frame->AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kError,
      base::StringPrintf(kAnyFrameConsoleError,
                         request->GetURL().scheme().c_str(),
                         request->GetURL().spec().c_str()));

  return CANCEL;
}

NavigationThrottle::ThrottleCheckResult
BlockedSchemeNavigationThrottle::WillProcessResponse() {
  NavigationRequest* request = NavigationRequest::From(navigation_handle());
  if (request->IsDownload())
    return PROCEED;

  RenderFrameHost* top_frame =
      request->frame_tree_node()->frame_tree().root()->current_frame_host();
  top_frame->AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kError,
      base::StringPrintf(kConsoleError, request->GetURL().scheme().c_str(),
                         request->GetURL().spec().c_str()));
  return CANCEL;
}

const char* BlockedSchemeNavigationThrottle::GetNameForLogging() {
  return "BlockedSchemeNavigationThrottle";
}

// static
std::unique_ptr<NavigationThrottle>
BlockedSchemeNavigationThrottle::CreateThrottleForNavigation(
    NavigationHandle* navigation_handle) {
  NavigationRequest* request = NavigationRequest::From(navigation_handle);
  // Create throttles when going to blocked schemes via renderer-initiated
  // navigations (which are cross-document in the main frame). Note that history
  // navigations can bypass this, because the blocked scheme must have
  // originally committed in a permitted case (e.g., omnibox navigation).
  if (request->IsInMainFrame() && request->IsRendererInitiated() &&
      !request->IsSameDocument() &&
      !NavigationTypeUtils::IsHistory(
          request->common_params().navigation_type) &&
      (request->GetURL().SchemeIs(url::kDataScheme) ||
       request->GetURL().SchemeIs(url::kFileSystemScheme)) &&
      !base::FeatureList::IsEnabled(
          features::kAllowContentInitiatedDataUrlNavigations)) {
    return std::make_unique<BlockedSchemeNavigationThrottle>(request);
  }
  // Block all renderer initiated navigations to filesystem: URLs except for
  // when explicitly allowed by the embedder. These won't load anyway since no
  // URL Loader exists for them, but the throttle lets us add a message to the
  // console.
  RenderFrameHost* current_frame_host =
      request->frame_tree_node()->current_frame_host();
  BrowserContext* browser_context = current_frame_host->GetBrowserContext();
  // A navigation is permitted if the relevant feature flag is enabled, the
  // request origin is equivalent to the initiator origin, and the embedder
  // explicitly allows it.
  bool is_navigation_allowed =
      base::FeatureList::IsEnabled(
          blink::features::kFileSystemUrlNavigationForChromeAppsOnly) &&
      (url::Origin::Create(request->GetURL()) ==
       request->GetInitiatorOrigin()) &&
      GetContentClient()->browser()->IsFileSystemURLNavigationAllowed(
          browser_context, request->GetURL());
  if (!is_navigation_allowed &&
      !base::FeatureList::IsEnabled(
          blink::features::kFileSystemUrlNavigation) &&
      request->IsRendererInitiated() &&
      request->GetURL().SchemeIs(url::kFileSystemScheme)) {
    return std::make_unique<BlockedSchemeNavigationThrottle>(request);
  }
  // Block any external mounted files.
  if (!base::FeatureList::IsEnabled(
          blink::features::kFileSystemUrlNavigation) &&
      IsExternalMountedFile(request->GetURL())) {
    return std::make_unique<BlockedSchemeNavigationThrottle>(request);
  }
  return nullptr;
}

}  // namespace content
