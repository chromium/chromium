// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_test.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::mojom::ManifestLocation;

namespace extensions {

using ManifestV3PermissionsTest = ManifestTest;

TEST_F(ManifestV3PermissionsTest, WebRequestBlockingPermissionsTest) {
  const std::string kPermissionRequiresV2OrLower =
      "'webRequestBlocking' requires manifest version of 2 or lower.";
  {
    // Manifest V3 extension that is not policy installed. This should trigger a
    // warning that manifest V3 is not currently supported and that the
    // webRequestBlocking permission requires a lower manifest version.
    scoped_refptr<Extension> extension(LoadAndExpectWarning(
        "web_request_blocking_v3.json", kPermissionRequiresV2OrLower,
        ManifestLocation::kUnpacked));
    ASSERT_TRUE(extension);
  }
  {
    // Manifest V3 extension that is policy extension. This should only trigger
    // a warning that manifest V3 is not supported currently.
    scoped_refptr<Extension> extension(LoadAndExpectSuccess(
        "web_request_blocking_v3.json", ManifestLocation::kExternalPolicy));
    ASSERT_TRUE(extension);
  }
  {
    // Manifest V2 extension that is not policy installed. This should not
    // trigger any warnings.
    scoped_refptr<Extension> extension(LoadAndExpectSuccess(
        "web_request_blocking_v2.json", ManifestLocation::kUnpacked));
    ASSERT_TRUE(extension);
  }
}

// Legacy manifest key "nacl_modules" is not supported, not recognized,
// and produces the same warning for all manifest versions.
TEST_F(ManifestV3PermissionsTest, DisallowNaClTest) {
  constexpr char kUnrecognizedKeyNaClModules[] =
      "Unrecognized manifest key 'nacl_modules'.";
  {
    scoped_refptr<Extension> extension(
        LoadAndExpectWarning("nacl_module_v3.json", kUnrecognizedKeyNaClModules,
                             ManifestLocation::kUnpacked));
    ASSERT_TRUE(extension);
  }
  {
    // Manifest V2 extension should produce warning about manifest version.
    scoped_refptr<Extension> extension(
        LoadAndExpectWarnings("nacl_module_v2.json",
                              {manifest_errors::kManifestV2IsDeprecatedWarning,
                               kUnrecognizedKeyNaClModules},
                              ManifestLocation::kUnpacked));
    ASSERT_TRUE(extension);
  }
}

}  // namespace extensions
