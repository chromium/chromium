// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DOWNLOAD_UTILS_H_
#define CONTENT_PUBLIC_BROWSER_DOWNLOAD_UTILS_H_

#include "content/common/content_export.h"

#include <string>

namespace content {
class BrowserContext;
}  // namespace content

class GURL;
namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace content {
namespace download_utils {

// Returns true if the given response must be downloaded because of the headers.
CONTENT_EXPORT bool MustDownload(BrowserContext* browser_context,
                                 const GURL& url,
                                 const net::HttpResponseHeaders* headers,
                                 const std::string& mime_type);

}  // namespace download_utils
}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DOWNLOAD_UTILS_H_
