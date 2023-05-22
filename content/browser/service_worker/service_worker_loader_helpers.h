// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_LOADER_HELPERS_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_LOADER_HELPERS_H_

#include <memory>
#include <string>

#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/content_export.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/script/script_type.mojom.h"

class GURL;

namespace base {
class TimeDelta;
}  // namespace base

namespace blink {
namespace mojom {
enum class ServiceWorkerUpdateViaCache;
}
}  // namespace blink

namespace content {

class BrowserContext;

namespace service_worker_loader_helpers {

// Check if |response_head| is a valid response for a service worker script
// (e.g. bad mime type). Status codes and error message will be set when the
// response is invalid.
bool CheckResponseHead(
    const network::mojom::URLResponseHead& response_head,
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

#if DCHECK_IS_ON()
// Checks the consistency between the status of the service worker version and
// the script type to be fetched by the loaders.
void CheckVersionStatusBeforeWorkerScriptLoad(
    ServiceWorkerVersion::Status status,
    bool is_main_script,
    blink::mojom::ScriptType script_type);
#endif  // DCHECK_IS_ON()

network::ResourceRequest CreateRequestForServiceWorkerScript(
    const GURL& script_url,
    const blink::StorageKey& storage_key,
    bool is_main_script,
    blink::mojom::ScriptType worker_script_type,
    const blink::mojom::FetchClientSettingsObject& fetch_client_settings_object,
    BrowserContext& browser_context);

// Returns true if the script at |script_url| is allowed to control |scope|
// according to Service Worker's path restriction policy. If
// |service_worker_allowed| is not null, it points to the
// Service-Worker-Allowed header value.
CONTENT_EXPORT bool IsPathRestrictionSatisfied(
    const GURL& scope,
    const GURL& script_url,
    const std::string* service_worker_allowed_header_value,
    std::string* error_message);

// Same as above IsPathRestrictionSatisfied, but without considering
// 'Service-Worker-Allowed' header.
CONTENT_EXPORT bool IsPathRestrictionSatisfiedWithoutHeader(
    const GURL& scope,
    const GURL& script_url,
    std::string* error_message);

// Returns the set of hash strings of fetch handlers which can be bypassed.
const base::flat_set<std::string> FetchHandlerBypassedHashStrings();

}  // namespace service_worker_loader_helpers

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_LOADER_HELPERS_H_
