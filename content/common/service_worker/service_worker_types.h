// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_TYPES_H_
#define CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_TYPES_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/common/referrer.h"
#include "services/network/public/mojom/request_context_frame_type.mojom.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_client.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_state.mojom.h"
#include "third_party/blink/public/platform/modules/fetch/fetch_api_request.mojom.h"
#include "url/gurl.h"

// This file is to have common definitions that are to be shared by
// browser and child process.

namespace storage {
class BlobHandle;
}

namespace content {

// Indicates the document main thread ID in the child process. This is used for
// messaging between the browser process and the child process.
static const int kDocumentMainThreadId = 0;

// Constants for error messages.
extern const char kServiceWorkerRegisterErrorPrefix[];
extern const char kServiceWorkerUpdateErrorPrefix[];
extern const char kServiceWorkerUnregisterErrorPrefix[];
extern const char kServiceWorkerGetRegistrationErrorPrefix[];
extern const char kServiceWorkerGetRegistrationsErrorPrefix[];
extern const char kServiceWorkerFetchScriptError[];
extern const char kServiceWorkerBadHTTPResponseError[];
extern const char kServiceWorkerSSLError[];
extern const char kServiceWorkerBadMIMEError[];
extern const char kServiceWorkerNoMIMEError[];
extern const char kServiceWorkerRedirectError[];
extern const char kServiceWorkerAllowed[];

// Constants for invalid identifiers.
static const int kInvalidEmbeddedWorkerThreadId = -1;
static const int kInvalidServiceWorkerProviderId = -1;
static const int64_t kInvalidServiceWorkerResourceId = -1;

// The HTTP cache is bypassed for Service Worker scripts if the last network
// fetch occurred over 24 hours ago.
static constexpr base::TimeDelta kServiceWorkerScriptMaxCacheAge =
    base::TimeDelta::FromHours(24);

struct ServiceWorkerCaseInsensitiveCompare {
  bool operator()(const std::string& lhs, const std::string& rhs) const {
    return base::CompareCaseInsensitiveASCII(lhs, rhs) < 0;
  }
};

using ServiceWorkerHeaderMap =
    std::map<std::string, std::string, ServiceWorkerCaseInsensitiveCompare>;

using ServiceWorkerHeaderList = std::vector<std::string>;

// Roughly corresponds to Fetch API's Request type. This struct is no longer
// used by the core Service Worker API. Background Fetch and Cache Storage APIs
// use it.
// TODO(falken): Move this out of service_worker_types.h and rename it.
struct CONTENT_EXPORT ServiceWorkerFetchRequest {
  ServiceWorkerFetchRequest();
  ServiceWorkerFetchRequest(const GURL& url,
                            const std::string& method,
                            const ServiceWorkerHeaderMap& headers,
                            const Referrer& referrer,
                            bool is_reload);
  ServiceWorkerFetchRequest(const ServiceWorkerFetchRequest& other);
  ServiceWorkerFetchRequest& operator=(const ServiceWorkerFetchRequest& other);
  ~ServiceWorkerFetchRequest();
  size_t EstimatedStructSize();

  // Be sure to update EstimatedStructSize(), Serialize(), and ParseFromString()
  // when adding members.
  network::mojom::FetchRequestMode mode =
      network::mojom::FetchRequestMode::kNoCORS;
  bool is_main_resource_load = false;
  blink::mojom::RequestContextType request_context_type =
      blink::mojom::RequestContextType::UNSPECIFIED;
  network::mojom::RequestContextFrameType frame_type =
      network::mojom::RequestContextFrameType::kNone;
  GURL url;
  std::string method;
  ServiceWorkerHeaderMap headers;
  Referrer referrer;
  network::mojom::FetchCredentialsMode credentials_mode =
      network::mojom::FetchCredentialsMode::kOmit;
  blink::mojom::FetchCacheMode cache_mode =
      blink::mojom::FetchCacheMode::kDefault;
  network::mojom::FetchRedirectMode redirect_mode =
      network::mojom::FetchRedirectMode::kFollow;
  std::string integrity;
  bool keepalive = false;
  std::string client_id;
  bool is_reload = false;
  bool is_history_navigation = false;
};

}  // namespace content

#endif  // CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_TYPES_H_
