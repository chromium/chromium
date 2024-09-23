// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_test.h"
#include "extensions/common/manifest_url_handlers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace errors = manifest_errors;

using AboutPageManifestTest = ManifestTest;

TEST_F(AboutPageManifestTest, AboutPageInSharedModules) {
  scoped_refptr<Extension> extension;
  extension = LoadAndExpectSuccess("shared_module_about.json");
  EXPECT_EQ(GURL("chrome-extension://" + extension->id() + "/about.html"),
            ManifestURL::GetAboutPage(extension.get()));

  Testcase testcases[] = {
      // Forbid data types other than strings.
      Testcase("shared_module_about_invalid_type.json",
               errors::kInvalidAboutPage),

      // Forbid absolute URLs.
      Testcase("shared_module_about_absolute.json",
               errors::kInvalidAboutPageExpectRelativePath)};
  RunTestcases(testcases, std::size(testcases), EXPECT_TYPE_ERROR);
}

}  // namespace extensions
