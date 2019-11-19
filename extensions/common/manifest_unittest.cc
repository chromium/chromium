// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest.h"

#include <memory>
#include <utility>

#include "extensions/common/install_warning.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

using ManifestTest = testing::Test;

namespace extensions {

TEST(ManifestTest, ValidateWarnsOnDiffFingerprintKeyUnpacked) {
  std::string error;
  std::vector<InstallWarning> warnings;
  Manifest(Manifest::UNPACKED,
           DictionaryBuilder()
               .Set(manifest_keys::kDifferentialFingerprint, "")
               .Build())
      .ValidateManifest(&error, &warnings);
  EXPECT_EQ("", error);
  EXPECT_EQ(1uL, warnings.size());
  EXPECT_EQ(manifest_errors::kHasDifferentialFingerprint, warnings[0].message);
}

TEST(ManifestTest, ValidateWarnsOnDiffFingerprintKeyCommandLine) {
  std::string error;
  std::vector<InstallWarning> warnings;
  Manifest(Manifest::COMMAND_LINE,
           DictionaryBuilder()
               .Set(manifest_keys::kDifferentialFingerprint, "")
               .Build())
      .ValidateManifest(&error, &warnings);
  EXPECT_EQ("", error);
  EXPECT_EQ(1uL, warnings.size());
  EXPECT_EQ(manifest_errors::kHasDifferentialFingerprint, warnings[0].message);
}

TEST(ManifestTest, ValidateSilentOnDiffFingerprintKeyInternal) {
  std::string error;
  std::vector<InstallWarning> warnings;
  Manifest(Manifest::INTERNAL,
           DictionaryBuilder()
               .Set(manifest_keys::kDifferentialFingerprint, "")
               .Build())
      .ValidateManifest(&error, &warnings);
  EXPECT_EQ("", error);
  EXPECT_EQ(0uL, warnings.size());
}

TEST(ManifestTest, ValidateSilentOnNoDiffFingerprintKeyUnpacked) {
  std::string error;
  std::vector<InstallWarning> warnings;
  Manifest(Manifest::UNPACKED, DictionaryBuilder().Build())
      .ValidateManifest(&error, &warnings);
  EXPECT_EQ("", error);
  EXPECT_EQ(0uL, warnings.size());
}

TEST(ManifestTest, ValidateSilentOnNoDiffFingerprintKeyInternal) {
  std::string error;
  std::vector<InstallWarning> warnings;
  Manifest(Manifest::INTERNAL, DictionaryBuilder().Build())
      .ValidateManifest(&error, &warnings);
  EXPECT_EQ("", error);
  EXPECT_EQ(0uL, warnings.size());
}

}  // namespace extensions
