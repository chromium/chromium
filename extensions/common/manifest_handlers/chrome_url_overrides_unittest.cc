// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/files/scoped_temp_dir.h"
#include "base/values.h"
#include "build/build_config.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/chrome_url_overrides_handler.h"
#include "extensions/common/manifest_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace errors = manifest_errors;

using URLOverridesManifestTest = ManifestTest;

TEST_F(URLOverridesManifestTest, Override) {
  RunTestcase(
      Testcase("override_newtab_and_history.json", errors::kMultipleOverrides),
      ExpectType::kError);

  scoped_refptr<extensions::Extension> extension;

  extension = LoadAndExpectSuccess("override_new_tab.json");
  EXPECT_EQ(extension->url().spec() + "newtab.html",
            URLOverrides::GetChromeURLOverrides(extension.get())
                .find("newtab")
                ->second.spec());

  extension = LoadAndExpectSuccess("override_history.json");
  EXPECT_EQ(extension->url().spec() + "history.html",
            URLOverrides::GetChromeURLOverrides(extension.get())
                .find("history")
                ->second.spec());

  // An extension which specifies an invalid override should still load for
  // future compatibility.
  extension = LoadAndExpectSuccess("override_invalid_page.json");
  EXPECT_TRUE(URLOverrides::GetChromeURLOverrides(extension.get()).empty());

  // "keyboard" property is only available on ChromeOS Ash.
  extension = LoadAndExpectSuccess("override_keyboard_page.json");
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(extension->url().spec() + "a_page.html",
            extensions::URLOverrides::GetChromeURLOverrides(extension.get())
                .find("keyboard")
                ->second.spec());
#else
  EXPECT_TRUE(URLOverrides::GetChromeURLOverrides(extension.get()).empty());
#endif
}

TEST(ChromeURLOverridesHandlerTest, TestFileMissing) {
  auto manifest = base::Value::Dict()
                      .Set("name", "ntp override")
                      .Set("version", "1.0")
                      .Set("manifest_version", 3)
                      .Set("chrome_url_overrides",
                           base::Value::Dict().Set("newtab", "newtab.html"));
  std::string error;
  std::u16string utf16_error;
  std::vector<InstallWarning> warnings;
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  scoped_refptr<Extension> extension = Extension::Create(
      dir.GetPath(), mojom::ManifestLocation::kInternal, manifest,
      Extension::NO_FLAGS, std::string(), &utf16_error);
  ASSERT_TRUE(extension);
  EXPECT_FALSE(
      file_util::ValidateExtension(extension.get(), &error, &warnings));
  EXPECT_EQ(ErrorUtils::FormatErrorMessage(manifest_errors::kFileNotFound,
                                           "newtab.html"),
            error);
}

}  // namespace extensions
