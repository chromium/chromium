// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_FEATURES_TEST_UTILS_H_
#define EXTENSIONS_COMMON_FEATURES_FEATURES_TEST_UTILS_H_

#include <vector>

namespace extensions::features_test_utils {

std::vector<const char*> GetExpectedDelegatedFeaturesForTest();

}  // namespace extensions::features_test_utils

#endif  // EXTENSIONS_COMMON_FEATURES_FEATURES_TEST_UTILS_H_
