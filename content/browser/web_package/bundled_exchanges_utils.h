// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_UTILS_H_
#define CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_UTILS_H_

#include "base/files/file_path.h"
#include "content/common/content_export.h"

class GURL;

namespace content {
namespace bundled_exchanges_utils {

// The "application/webbundle" MIME type must have a "v" parameter whose value
// is a format version [1]. But we use the "application/webbundle" MIME type
// without "v" parameter while loading bundled exchanges files in local storage.
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
constexpr char kBundledExchangesFileMimeTypeWithoutParameters[] =
    "application/webbundle";

// On Android, returns true if the url scheme is file or content. On other
// platforms, returns true if the url scheme is file.
bool IsSupprtedFileScheme(const GURL& url);

// Returns true if |url| is the file URL which is specified with
// --trustable-bundled-exchanges-file-url flag. Always returns false when
// ContentBrowserClient::CanAcceptUntrustedExchangesIfNeeded() is false.
bool CanLoadAsTrustableBundledExchangesFile(const GURL& url);

// Returns whether bundled exchanges file can be loaded from the |url|. Always
// returns false when BundledHTTPExchanges feature is not enabled.
bool CanLoadAsBundledExchangesFile(const GURL& url);

// Returns whether bundled exchanges file can be loaded from the |url| with
// the |mime_type|. Always returns false when BundledHTTPExchanges feature is
// not enabled.
bool CanLoadAsBundledExchanges(const GURL& url, const std::string& mime_type);

// Sets |mime_type| to "application/webbundle" and returns true, when
// BundledHTTPExchanges feature is enabled, and the extension of the |path| is
// ".wbn". Otherwise returns false.
bool GetBundledExchangesFileMimeTypeFromFile(const base::FilePath& path,
                                             std::string* mime_type);

// Generate a synthesized URL which can indicate the url in bundled exchanges
// file.
// Example:
//   bundled_exchanges_file_url: file:///dir/x.wbn?query1#ref1
//   url_in_bundles: https://example.com/a.html?query2#ref2
//      => synthesized URL:
//           file:///dir/x.wbn?https://example.com/a.html?query2#ref2
CONTENT_EXPORT GURL
GetSynthesizedUrlForBundledExchanges(const GURL& bundled_exchanges_file_url,
                                     const GURL& url_in_bundles);

}  // namespace bundled_exchanges_utils
}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_UTILS_H_
