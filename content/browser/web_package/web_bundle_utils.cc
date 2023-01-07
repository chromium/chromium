// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/web_bundle_utils.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/web_package/web_bundle_url_loader_factory.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "url/url_constants.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace content {
namespace web_bundle_utils {
namespace {

const base::FilePath::CharType kWebBundleFileExtension[] =
    FILE_PATH_LITERAL(".wbn");

}  // namespace

const net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("web_bundle_loader",
                                        R"(
  semantics {
    sender: "Web Bundle Loader"
    description:
      "Navigation request and subresource request inside a Web Bundle. "
      "This does not trigger any network transaction directly, but "
      "access to an entry in a local file, or in a previously fetched "
      "resource over network."
    trigger: "The user navigates to a Web Bundle."
    data: "Nothing."
    destination: LOCAL
  }
  policy {
    cookies_allowed: NO
    setting: "These requests cannot be disabled in settings."
    policy_exception_justification:
      "Not implemented. This request does not make any network transaction."
  }
  comments:
    "Usually the request accesses an entry in a local file or a previously "
    "fetched resource over network that contains multiple archived entries. "
    "But once the feature is exposed to the public web API, the archive file "
    "can be streamed over network. In such case, the streaming should be "
    "provided by another URLLoader request that is issued by Blink, but based "
    "on a user initiated navigation."
  )");

bool IsSupportedFileScheme(const GURL& url) {
  if (url.SchemeIsFile())
    return true;
#if BUILDFLAG(IS_ANDROID)
  if (url.SchemeIs(url::kContentScheme))
    return true;
#endif  // BUILDFLAG(IS_ANDROID)
  return false;
}

bool CanLoadAsTrustableWebBundleFile(const GURL& url) {
  if (!GetContentClient()->browser()->CanAcceptUntrustedExchangesIfNeeded())
    return false;
  if (!IsSupportedFileScheme(url))
    return false;
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kTrustableWebBundleFileUrl)) {
    return false;
  }
  return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
             switches::kTrustableWebBundleFileUrl) == url.spec();
}

bool CanLoadAsWebBundleFile(const GURL& url) {
  if (!base::FeatureList::IsEnabled(features::kWebBundles))
    return false;
  return IsSupportedFileScheme(url);
}

bool CanLoadAsWebBundle(const GURL& url, const std::string& mime_type) {
  if (!base::FeatureList::IsEnabled(features::kWebBundles))
    return false;
  // Currently loading Web Bundle file from server response is not
  // implemented yet.
  if (!IsSupportedFileScheme(url))
    return false;
  return mime_type == kWebBundleFileMimeTypeWithoutParameters;
}

bool GetWebBundleFileMimeTypeFromFile(const base::FilePath& path,
                                      std::string* mime_type) {
  DCHECK(mime_type);
  if (!base::FeatureList::IsEnabled(features::kWebBundles))
    return false;
  if (path.Extension() != kWebBundleFileExtension)
    return false;
  *mime_type = kWebBundleFileMimeTypeWithoutParameters;
  return true;
}

GURL GetSynthesizedUrlForWebBundle(const GURL& web_bundle_file_url,
                                   const GURL& url_in_bundles) {
  GURL::Replacements replacements;

  GURL::Replacements clear_ref;
  clear_ref.ClearRef();
  std::string query_string = url_in_bundles.ReplaceComponents(clear_ref).spec();
  replacements.SetQueryStr(query_string);

  if (!url_in_bundles.has_ref()) {
    replacements.ClearRef();
    return web_bundle_file_url.ReplaceComponents(replacements);
  }
  std::string ref_string = url_in_bundles.ref();
  replacements.SetRefStr(ref_string);
  return web_bundle_file_url.ReplaceComponents(replacements);
}

bool IsAllowedExchangeUrl(const GURL& url) {
  return url.SchemeIsHTTPOrHTTPS();
}

void CompleteWithInvalidWebBundleError(
    mojo::Remote<network::mojom::URLLoaderClient> client,
    int frame_tree_node_id,
    const std::string& error_message) {
  LogErrorMessageToConsole(frame_tree_node_id, error_message);
  std::move(client)->OnComplete(
      network::URLLoaderCompletionStatus(net::ERR_INVALID_WEB_BUNDLE));
}

std::string GetMetadataParseErrorMessage(
    const web_package::mojom::BundleMetadataParseErrorPtr& metadata_error) {
  return std::string("Failed to read metadata of Web Bundle file: ") +
         metadata_error->message;
}

void LogErrorMessageToConsole(const int frame_tree_node_id,
                              const std::string& error_message) {
  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id);
  if (!frame_tree_node) {
    return;
  }

  // By default, Dev Tools clears all console messages on navigations, and only
  // preserves them if the 'Preserve Log' option is enabled. Thus, we use
  // `AddDeferredConsoleMessage` if a `navigation_request` is currently in
  // progress, so that the the console messages are logged to the console only
  // after the navigation is finished and developers can see the detailed
  // error messages. crbug.com/1068481
  if (auto* navigation_request = frame_tree_node->navigation_request()) {
    navigation_request->AddDeferredConsoleMessage(
        blink::mojom::ConsoleMessageLevel::kError, error_message);
  } else {
    frame_tree_node->current_frame_host()->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError, error_message);
  }
}

}  // namespace web_bundle_utils
}  // namespace content
