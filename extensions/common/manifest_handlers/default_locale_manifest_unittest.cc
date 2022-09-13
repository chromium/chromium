// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/default_locale_handler.h"
#include "extensions/common/manifest_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using DefaultLocaleManifestTest = ManifestTest;

TEST_F(DefaultLocaleManifestTest, DefaultLocale) {
  LoadAndExpectError("default_locale_invalid.json",
                     manifest_errors::kInvalidDefaultLocale);

  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("default_locale_valid.json"));
  EXPECT_EQ("de-AT", LocaleInfo::GetDefaultLocale(extension.get()));
}

}  // namespace extensions
