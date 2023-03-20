// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/features_test_utils.h"

namespace extensions::features_test_utils {

// This list should stay in sync with GetControlledFrameFeatureList().
std::vector<const char*> GetExpectedDelegatedFeaturesForTest() {
  return {
      "chromeWebViewInternal", "declarativeWebRequest",
      "guestViewInternal",     "webRequest",
      "webViewInternal",       "webViewRequest",
  };
}

}  // namespace extensions::features_test_utils
