// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/icons_handler.h"
#include "base/strings/stringprintf.h"
#include "base/test/values_test_util.h"
#include "extensions/common/manifest_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class ProductIconManifestTest : public ManifestTest {
 public:
  ProductIconManifestTest() = default;

  ProductIconManifestTest(const ProductIconManifestTest&) = delete;
  ProductIconManifestTest& operator=(const ProductIconManifestTest&) = delete;

 protected:
  base::Value::Dict CreateManifest(const std::string& extra_icons) {
    constexpr const char kManifest[] = R"({
      "name": "test",
      "version": "0.1",
      "manifest_version": 2,
      "icons": {
        %s
        "16": "icon1.png",
        "32": "icon2.png"
      }
    })";
    base::Value manifest = base::test::ParseJson(
        base::StringPrintf(kManifest, extra_icons.c_str()));
    EXPECT_TRUE(manifest.is_dict());
    return std::move(manifest).TakeDict();
  }
};

TEST_F(ProductIconManifestTest, Sizes) {
  // Too big.
  {
    ManifestData manifest(CreateManifest(R"("100000": "icon3.png",)"));
    LoadAndExpectError(manifest, "Invalid key in icons: \"100000\".");
  }
  // Too small.
  {
    ManifestData manifest(CreateManifest(R"("0": "icon3.png",)"));
    LoadAndExpectError(manifest, "Invalid key in icons: \"0\".");
  }
  // NaN.
  {
    ManifestData manifest(CreateManifest(R"("sixteen": "icon3.png",)"));
    LoadAndExpectError(manifest, "Invalid key in icons: \"sixteen\".");
  }
  // Just right.
  {
    ManifestData manifest(CreateManifest(R"("512": "icon3.png",)"));
    scoped_refptr<extensions::Extension> extension =
        LoadAndExpectSuccess(manifest);
  }
}

}  // namespace extensions
