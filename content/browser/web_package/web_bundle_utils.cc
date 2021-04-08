// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/web_bundle_utils.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "content/browser/web_package/web_bundle_url_loader_factory.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "url/url_constants.h"
#endif  // defined(OS_ANDROID)

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
#if defined(OS_ANDROID)
  if (url.SchemeIs(url::kContentScheme))
    return true;
#endif  // defined(OS_ANDROID)
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
  url::Replacements<char> replacements;

  url::Replacements<char> clear_ref;
  clear_ref.ClearRef();
  std::string query_string = url_in_bundles.ReplaceComponents(clear_ref).spec();
  url::Component new_query(0, query_string.size());
  replacements.SetQuery(query_string.c_str(), new_query);

  if (!url_in_bundles.has_ref()) {
    replacements.ClearRef();
    return web_bundle_file_url.ReplaceComponents(replacements);
  }
  url::Component new_ref(0, url_in_bundles.ref().size());
  std::string ref_string = url_in_bundles.ref();
  replacements.SetRef(ref_string.c_str(), new_ref);
  return web_bundle_file_url.ReplaceComponents(replacements);
}

void CompleteWithInvalidWebBundleError(
    mojo::Remote<network::mojom::URLLoaderClient> client,
    int frame_tree_node_id,
    const std::string& error_message) {
  WebContents* web_contents =
      WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  if (web_contents) {
    web_contents->GetMainFrame()->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError, error_message);
  }
  std::move(client)->OnComplete(
      network::URLLoaderCompletionStatus(net::ERR_INVALID_WEB_BUNDLE));
}

std::string GetMetadataParseErrorMessage(
    const web_package::mojom::BundleMetadataParseErrorPtr& metadata_error) {
  return std::string("Failed to read metadata of Web Bundle file: ") +
         metadata_error->message;
}

}  // namespace web_bundle_utils
}  // namespace content
