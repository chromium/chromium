// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_UTILS_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_UTILS_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "content/common/content_export.h"

namespace download {
class DownloadUrlParameters;
}  // namespace download

namespace net {
class URLRequest;
class URLRequestContextGetter;
}  // namespace net

namespace storage {
class BlobStorageContext;
}

namespace content {

class ResourceContext;

// Create a URLRequest from |params| using the specified
// URLRequestContextGetter.
std::unique_ptr<net::URLRequest> CONTENT_EXPORT CreateURLRequestOnIOThread(
    download::DownloadUrlParameters* params,
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter);

storage::BlobStorageContext* BlobStorageContextGetter(
    ResourceContext* resource_context);

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_UTILS_H_
