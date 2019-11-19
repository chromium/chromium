// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_UTILS_H_
#define CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_UTILS_H_

#include <sstream>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/resource_type.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_response.h"
#include "third_party/blink/public/common/fetch/fetch_api_request_headers_map.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerUtils {
 public:
  static bool IsMainResourceType(ResourceType type);

  // Returns true if |scope| matches |url|.
  CONTENT_EXPORT static bool ScopeMatches(const GURL& scope, const GURL& url);

  // Returns true if the script at |script_url| is allowed to control |scope|
  // according to Service Worker's path restriction policy. If
  // |service_worker_allowed| is not null, it points to the
  // Service-Worker-Allowed header value.
  CONTENT_EXPORT static bool IsPathRestrictionSatisfied(
      const GURL& scope,
      const GURL& script_url,
      const std::string* service_worker_allowed_header_value,
      std::string* error_message);

  // Same as above IsPathRestrictionSatisfied, but without considering
  // 'Service-Worker-Allowed' header.
  CONTENT_EXPORT static bool IsPathRestrictionSatisfiedWithoutHeader(
      const GURL& scope,
      const GURL& script_url,
      std::string* error_message);

  static bool ContainsDisallowedCharacter(const GURL& scope,
                                          const GURL& script_url,
                                          std::string* error_message);

  // Returns true if all members of |urls| have the same origin, and
  // OriginCanAccessServiceWorkers is true for this origin.
  // If --disable-web-security is enabled, the same origin check is
  // not performed.
  CONTENT_EXPORT static bool AllOriginsMatchAndCanAccessServiceWorkers(
      const std::vector<GURL>& urls);

  template <typename T>
  static std::string MojoEnumToString(T mojo_enum) {
    std::ostringstream oss;
    oss << mojo_enum;
    return oss.str();
  }

  // Converts an enum defined in net/base/load_flags.h to
  // blink::mojom::FetchCacheMode.
  CONTENT_EXPORT static blink::mojom::FetchCacheMode GetCacheModeFromLoadFlags(
      int load_flags);

  CONTENT_EXPORT static const char* FetchResponseSourceToSuffix(
      network::mojom::FetchResponseSource source);

  struct CONTENT_EXPORT ResourceResponseHeadAndMetadata {
    ResourceResponseHeadAndMetadata(network::ResourceResponseHead head,
                                    std::vector<uint8_t> metadata);
    ResourceResponseHeadAndMetadata(ResourceResponseHeadAndMetadata&& other);
    ResourceResponseHeadAndMetadata(
        const ResourceResponseHeadAndMetadata& other) = delete;
    ~ResourceResponseHeadAndMetadata();

    network::ResourceResponseHead head;
    std::vector<uint8_t> metadata;
  };

  CONTENT_EXPORT static ResourceResponseHeadAndMetadata
  CreateResourceResponseHeadAndMetadata(const net::HttpResponseInfo* http_info,
                                        uint32_t options,
                                        base::TimeTicks request_start_time,
                                        base::TimeTicks response_start_time,
                                        int response_data_size);

 private:
  static bool IsPathRestrictionSatisfiedInternal(
      const GURL& scope,
      const GURL& script_url,
      bool service_worker_allowed_header_supported,
      const std::string* service_worker_allowed_header_value,
      std::string* error_message);
};

class CONTENT_EXPORT LongestScopeMatcher {
 public:
  explicit LongestScopeMatcher(const GURL& url) : url_(url) {}
  virtual ~LongestScopeMatcher() {}

  // Returns true if |scope| matches |url_| longer than |match_|.
  bool MatchLongest(const GURL& scope);

 private:
  const GURL url_;
  GURL match_;

  DISALLOW_COPY_AND_ASSIGN(LongestScopeMatcher);
};

}  // namespace content

#endif  // CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_UTILS_H_
