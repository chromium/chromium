// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_COOKIE_DEPRECATION_LABEL_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_COOKIE_DEPRECATION_LABEL_MANAGER_H_

#include <string>

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

class CookieDeprecationLabelManager {
 public:
  virtual ~CookieDeprecationLabelManager() = default;

  virtual absl::optional<std::string> GetValue() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_COOKIE_DEPRECATION_LABEL_MANAGER_H_
