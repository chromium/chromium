// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_NAVIGATION_PARAMS_UTILS_H_
#define CONTENT_COMMON_NAVIGATION_PARAMS_UTILS_H_

#include "content/common/navigation_params.mojom.h"

namespace content {

class NavigationTypeUtils {
 public:
  static bool IsReload(mojom::NavigationType value) {
    return value == mojom::NavigationType::RELOAD ||
           value == mojom::NavigationType::RELOAD_BYPASSING_CACHE ||
           value == mojom::NavigationType::RELOAD_ORIGINAL_REQUEST_URL;
  }

  static bool IsSameDocument(mojom::NavigationType value) {
    return value == mojom::NavigationType::SAME_DOCUMENT ||
           value == mojom::NavigationType::HISTORY_SAME_DOCUMENT;
  }

  static bool IsHistory(mojom::NavigationType value) {
    return value == mojom::NavigationType::HISTORY_SAME_DOCUMENT ||
           value == mojom::NavigationType::HISTORY_DIFFERENT_DOCUMENT;
  }
};

}  // namespace content

#endif  // CONTENT_COMMON_NAVIGATION_PARAMS_UTILS_H_
