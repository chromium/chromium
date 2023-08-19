// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_NAVIGATION_PARAMS_UTILS_H_
#define CONTENT_COMMON_NAVIGATION_PARAMS_UTILS_H_

#include "third_party/blink/public/mojom/navigation/navigation_params.mojom-shared.h"

namespace content {

class NavigationTypeUtils {
 public:
  static bool IsReload(blink::mojom::NavigationType value) {
    return value == blink::mojom::NavigationType::RELOAD ||
           value == blink::mojom::NavigationType::RELOAD_BYPASSING_CACHE;
  }

  static bool IsSameDocument(blink::mojom::NavigationType value) {
    return value == blink::mojom::NavigationType::SAME_DOCUMENT ||
           value == blink::mojom::NavigationType::HISTORY_SAME_DOCUMENT;
  }

  static bool IsHistory(blink::mojom::NavigationType value) {
    return value == blink::mojom::NavigationType::HISTORY_SAME_DOCUMENT ||
           value == blink::mojom::NavigationType::HISTORY_DIFFERENT_DOCUMENT ||
           IsRestore(value);
  }

  static bool IsRestore(blink::mojom::NavigationType value) {
    return value == blink::mojom::NavigationType::RESTORE ||
           value == blink::mojom::NavigationType::RESTORE_WITH_POST;
  }
};

}  // namespace content

#endif  // CONTENT_COMMON_NAVIGATION_PARAMS_UTILS_H_
