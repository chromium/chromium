// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_UTILS_H_
#define CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_UTILS_H_

#include <string>

#include "url/gurl.h"

namespace content {

class ServiceWorkerUtils {
 public:
  static bool ContainsDisallowedCharacter(const GURL& scope,
                                          const GURL& script_url,
                                          std::string* error_message);

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
