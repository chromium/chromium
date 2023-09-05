// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COOKIE_DEPRECATION_LABEL_COOKIE_DEPRECATION_LABEL_MANAGER_H_
#define CONTENT_BROWSER_COOKIE_DEPRECATION_LABEL_COOKIE_DEPRECATION_LABEL_MANAGER_H_

#include <string>

#include "base/memory/raw_ref.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace url {
class Origin;
}  // namespace url

namespace content {

class BrowserContext;

// Maintaining a per-partition label to be used for 3PCD experimentation.
class CONTENT_EXPORT CookieDeprecationLabelManager {
 public:
  explicit CookieDeprecationLabelManager(BrowserContext* browser_context);
  CookieDeprecationLabelManager(const CookieDeprecationLabelManager&) = delete;
  CookieDeprecationLabelManager(CookieDeprecationLabelManager&&) = delete;
  CookieDeprecationLabelManager& operator=(
      const CookieDeprecationLabelManager&) = delete;
  CookieDeprecationLabelManager& operator=(CookieDeprecationLabelManager&&) =
      delete;
  ~CookieDeprecationLabelManager();

  absl::optional<std::string> GetValue();

  absl::optional<std::string> GetValue(const url::Origin& top_frame_origin,
                                       const url::Origin& context_origin);

 private:
  std::string GetValueInternal();

  // `this` is owned by the `StoragePartitionImpl`, which itself is
  // owned by `browser_context_`.
  raw_ref<BrowserContext> browser_context_;

  // The label isn't expected to change, therefore caching the value for future
  // use to improve performance.
  absl::optional<std::string> label_value_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_COOKIE_DEPRECATION_LABEL_COOKIE_DEPRECATION_LABEL_MANAGER_H_
