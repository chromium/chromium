// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/features_test_utils.h"

namespace extensions::features_test_utils {

std::vector<const char*> GetExpectedDelegatedFeaturesForTest() {
  return {
      "chromeWebViewInternal",
      "guestViewInternal",
      "webRequestInternal",
      "webViewInternal",
  };
}

}  // namespace extensions::features_test_utils
