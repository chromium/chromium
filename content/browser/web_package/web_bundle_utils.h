// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_UTILS_H_
#define CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_UTILS_H_

#include <string>

#include "base/files/file_path.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "url/gurl.h"

namespace content {

class WebBundleURLLoaderFactory;

using WebBundleDoneCallback = base::OnceCallback<void(
    const GURL& target_inner_url,
    std::unique_ptr<WebBundleURLLoaderFactory> url_loader_factory)>;

namespace web_bundle_utils {

// The "application/webbundle" MIME type must have a "v" parameter whose value
// is a format version [1]. But we use the "application/webbundle" MIME type
// without "v" parameter while loading Web Bundle files in local storage.
// This is because Android's intent-filter is not designed to support such
// parameters.
// IntentResolver.queryIntent() [2] is called when a filer application calls
// PackageManager.queryIntentActivities() to open files. But this queryIntent()
// doesn't support Media Type's parameters matching. For example, if we add a
// <intent-filter> with <data android:mimeType="application/webbundle;v=b1" />
// in AndroidManifest.xml, the data with "application/webbundle; v=b1" which has
// a space after ";" doesn't match the filter.
// [1]
// https://wicg.github.io/webpackage/draft-yasskin-wpack-bundled-exchanges.html#internet-media-type-registration
// [2]
// https://android.googlesource.com/platform/frameworks/base/+/4ab9511/services/core/java/com/android/server/IntentResolver.java#398
constexpr char kWebBundleFileMimeTypeWithoutParameters[] =
    "application/webbundle";

constexpr char kNoSniffErrorMessage[] =
    "Web Bundle response must have \"X-Content-Type-Options: nosniff\" header.";

constexpr char kNoPrimaryUrlErrorMessage[] =
    "Web Bundle is missing the Primary URL to navigate to.";

constexpr char kInvalidPrimaryUrlErrorMessage[] =
    "Primary URL is not a valid exchange URL.";

constexpr char kInvalidExchangeUrlErrorMessage[] = "Exchange URL is not valid.";

extern const net::NetworkTrafficAnnotationTag kTrafficAnnotation;

// Adds |error_message| to the console and calls OnComplete() of |client|.
void CompleteWithInvalidWebBundleError(
    mojo::Remote<network::mojom::URLLoaderClient> client,
    int frame_tree_node_id,
    const std::string& error_message);

void LogErrorMessageToConsole(const int frame_tree_node_id,
                              const std::string& error_message);

std::string GetMetadataParseErrorMessage(
    const web_package::mojom::BundleMetadataParseErrorPtr& metadata_error);

// On Android, returns true if the url scheme is file or content. On other
// platforms, returns true if the url scheme is file.
bool IsSupportedFileScheme(const GURL& url);

// Returns true if |url| is the file URL which is specified with
// --trustable-bundled-exchanges-file-url flag. Always returns false when
// ContentBrowserClient::CanAcceptUntrustedExchangesIfNeeded() is false.
bool CanLoadAsTrustableWebBundleFile(const GURL& url);

// Returns whether Web Bundle file can be loaded from the |url|. Always returns
// false when WebBundle feature is not enabled.
bool CanLoadAsWebBundleFile(const GURL& url);

// Returns whether Web Bundle file can be loaded from the |url| with the
// |mime_type|. Always returns false when WebBundle feature is not enabled.
bool CanLoadAsWebBundle(const GURL& url, const std::string& mime_type);

// Sets |mime_type| to "application/webbundle" and returns true, when
// WebBundle feature is enabled, and the extension of the |path| is ".wbn".
// Otherwise returns false.
bool GetWebBundleFileMimeTypeFromFile(const base::FilePath& path,
                                      std::string* mime_type);

// Generate a synthesized URL which can indicate the url in Web Bundle file.
// Example:
//   web_bundle_file_url: file:///dir/x.wbn?query1#ref1
//   url_in_bundles: https://example.com/a.html?query2#ref2
//      => synthesized URL:
//           file:///dir/x.wbn?https://example.com/a.html?query2#ref2
CONTENT_EXPORT GURL
GetSynthesizedUrlForWebBundle(const GURL& web_bundle_file_url,
                              const GURL& url_in_bundles);

// Checks whether the URL is allowed to be used as the URL of an exchange.
// TODO(crbug.com/966753): Revisit this once
// https://github.com/WICG/webpackage/issues/468 is resolved.
bool IsAllowedExchangeUrl(const GURL& url);

}  // namespace web_bundle_utils
}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_UTILS_H_
