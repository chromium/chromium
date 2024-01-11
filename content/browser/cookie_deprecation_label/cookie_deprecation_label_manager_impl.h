// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COOKIE_DEPRECATION_LABEL_COOKIE_DEPRECATION_LABEL_MANAGER_IMPL_H_
#define CONTENT_BROWSER_COOKIE_DEPRECATION_LABEL_COOKIE_DEPRECATION_LABEL_MANAGER_IMPL_H_

#include <optional>
#include <string>

#include "base/memory/raw_ref.h"
#include "content/common/content_export.h"
#include "content/public/browser/cookie_deprecation_label_manager.h"

namespace url {
class Origin;
}  // namespace url

namespace content {

class BrowserContext;

// Maintaining a per-partition label to be used for 3PCD experimentation.
class CONTENT_EXPORT CookieDeprecationLabelManagerImpl
    : public CookieDeprecationLabelManager {
 public:
  explicit CookieDeprecationLabelManagerImpl(BrowserContext* browser_context);
  CookieDeprecationLabelManagerImpl(const CookieDeprecationLabelManagerImpl&) =
      delete;
  CookieDeprecationLabelManagerImpl(CookieDeprecationLabelManagerImpl&&) =
      delete;
  CookieDeprecationLabelManagerImpl& operator=(
      const CookieDeprecationLabelManagerImpl&) = delete;
  CookieDeprecationLabelManagerImpl& operator=(
      CookieDeprecationLabelManagerImpl&&) = delete;
  ~CookieDeprecationLabelManagerImpl() override;

  std::optional<std::string> GetValue() override;

  std::optional<std::string> GetValue(const url::Origin& top_frame_origin,
                                      const url::Origin& context_origin);

 private:
  // Sets label_value_ to the feature param and returns. Returns std::nullopt
  // if `kSkipCookieDeprecationLabelForStoragePartitions` is true.
  std::optional<std::string> GetValueInternal();

  // `this` is owned by the `StoragePartitionImpl`, which itself is
  // owned by `browser_context_`.
  raw_ref<BrowserContext> browser_context_;

  // The label isn't expected to change, therefore caching the value for future
  // use to improve performance.
  std::optional<std::string> label_value_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_COOKIE_DEPRECATION_LABEL_COOKIE_DEPRECATION_LABEL_MANAGER_IMPL_H_
