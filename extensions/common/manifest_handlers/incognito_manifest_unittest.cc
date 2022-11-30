// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/manifest_test.h"

namespace extensions {

using IncognitoManifestTest = ManifestTest;

TEST_F(IncognitoManifestTest, IncognitoNotAllowed) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("incognito_not_allowed.json"));
  EXPECT_FALSE(IncognitoInfo::IsIncognitoAllowed(extension.get()));
  EXPECT_FALSE(IncognitoInfo::IsSplitMode(extension.get()));
  EXPECT_FALSE(util::CanBeIncognitoEnabled(extension.get()));
}

TEST_F(IncognitoManifestTest, IncognitoSpanning) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("incognito_spanning.json"));
  EXPECT_TRUE(IncognitoInfo::IsIncognitoAllowed(extension.get()));
  EXPECT_FALSE(IncognitoInfo::IsSplitMode(extension.get()));
  EXPECT_TRUE(util::CanBeIncognitoEnabled(extension.get()));
}

TEST_F(IncognitoManifestTest, IncognitoSplit) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("incognito_split.json"));
  EXPECT_TRUE(IncognitoInfo::IsIncognitoAllowed(extension.get()));
  EXPECT_TRUE(IncognitoInfo::IsSplitMode(extension.get()));
  EXPECT_TRUE(util::CanBeIncognitoEnabled(extension.get()));
}

TEST_F(IncognitoManifestTest, IncognitoUnspecified) {
  scoped_refptr<Extension> extension(LoadAndExpectSuccess("minimal.json"));
  EXPECT_TRUE(IncognitoInfo::IsIncognitoAllowed(extension.get()));
  EXPECT_FALSE(IncognitoInfo::IsSplitMode(extension.get()));
  EXPECT_TRUE(util::CanBeIncognitoEnabled(extension.get()));
}

}  // namespace extensions
