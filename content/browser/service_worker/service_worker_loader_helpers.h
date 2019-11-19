// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_LOADER_HELPERS_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_LOADER_HELPERS_H_

#include <memory>
#include <string>

#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"

namespace base {
class TimeDelta;
}

namespace blink {
namespace mojom {
enum class ServiceWorkerUpdateViaCache;
}
}  // namespace blink

namespace content {

namespace service_worker_loader_helpers {

// Creates net::HttpResponseInfo from |response_head|. If |response_head| is
// invalid as a service worker script (e.g. bad mime type), returns nullptr and
// sets error code and a message.
std::unique_ptr<net::HttpResponseInfo> CreateHttpResponseInfoAndCheckHeaders(
    const network::ResourceResponseHead& response_head,
    blink::ServiceWorkerStatusCode* out_service_worker_status,
    network::URLLoaderCompletionStatus* out_completion_status,
    std::string* out_error_message);

bool ShouldBypassCacheDueToUpdateViaCache(
    bool is_main_script,
    blink::mojom::ServiceWorkerUpdateViaCache cache_mode);

bool ShouldValidateBrowserCacheForScript(
    bool is_main_script,
    bool force_bypass_cache,
    blink::mojom::ServiceWorkerUpdateViaCache cache_mode,
    base::TimeDelta time_since_last_check);

}  // namespace service_worker_loader_helpers

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_LOADER_HELPERS_H_
