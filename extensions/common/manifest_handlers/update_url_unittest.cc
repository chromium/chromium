// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_test.h"
#include "extensions/common/manifest_url_handlers.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::Extension;
using extensions::mojom::ManifestLocation;

namespace errors = extensions::manifest_errors;

using UpdateURLManifestTest = extensions::ManifestTest;

TEST_F(UpdateURLManifestTest, UpdateUrls) {
  // Test several valid update urls
  Testcase testcases[] = {
      Testcase("update_url_valid_1.json", ManifestLocation::kInternal,
               Extension::NO_FLAGS),
      Testcase("update_url_valid_2.json", ManifestLocation::kInternal,
               Extension::NO_FLAGS),
      Testcase("update_url_valid_3.json", ManifestLocation::kInternal,
               Extension::NO_FLAGS),
      Testcase("update_url_valid_4.json", ManifestLocation::kInternal,
               Extension::NO_FLAGS)};
  RunTestcases(testcases, std::size(testcases), EXPECT_TYPE_SUCCESS);

  // Test some invalid update urls
  Testcase testcases2[] = {
      Testcase("update_url_invalid_1.json", errors::kInvalidUpdateURL,
               ManifestLocation::kInternal, Extension::NO_FLAGS),
      Testcase("update_url_invalid_2.json", errors::kInvalidUpdateURL,
               ManifestLocation::kInternal, Extension::NO_FLAGS),
      Testcase("update_url_invalid_3.json", errors::kInvalidUpdateURL,
               ManifestLocation::kInternal, Extension::NO_FLAGS)};
  RunTestcases(testcases2, std::size(testcases2), EXPECT_TYPE_ERROR);
}
