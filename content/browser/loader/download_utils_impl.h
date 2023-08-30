// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_DOWNLOAD_UTILS_IMPL_H_
#define CONTENT_BROWSER_LOADER_DOWNLOAD_UTILS_IMPL_H_

#include "content/public/browser/download_utils.h"

namespace content {
class BrowserContext;

namespace download_utils {

// Determines whether given response would result in a download.
// Note this doesn't handle the case when a plugin exists for the |mime_type|.
bool IsDownload(BrowserContext* browser_context,
                const GURL& url,
                const net::HttpResponseHeaders* headers,
                const std::string& mime_type);

}  // namespace download_utils
}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_DOWNLOAD_UTILS_IMPL_H_
