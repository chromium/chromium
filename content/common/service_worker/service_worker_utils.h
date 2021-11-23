// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_UTILS_H_
#define CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_UTILS_H_

#include <sstream>
#include <string>

#include "content/common/content_export.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "third_party/blink/public/common/fetch/fetch_api_request_headers_map.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerUtils {
 public:
  static bool IsMainRequestDestination(
      network::mojom::RequestDestination destination);

  static bool ContainsDisallowedCharacter(const GURL& scope,
                                          const GURL& script_url,
                                          std::string* error_message);

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

 private:
  static bool IsPathRestrictionSatisfiedInternal(
      const GURL& scope,
      const GURL& script_url,
      bool service_worker_allowed_header_supported,
      const std::string* service_worker_allowed_header_value,
      std::string* error_message);
};

}  // namespace content

#endif  // CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_UTILS_H_
