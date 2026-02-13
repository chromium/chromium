// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "components/version_info/channel.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/message_serialization_info.h"
#include "extensions/common/manifest_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using MessageSerializationManifestTest = ManifestTest;

TEST_F(MessageSerializationManifestTest, MessageSerialization) {
  ScopedCurrentChannel channel(version_info::Channel::CANARY);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      extensions_features::kStructuredCloningForMessaging);

  // Test valid values.
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("message_serialization_valid_json.json");
  ASSERT_TRUE(extension);
  EXPECT_FALSE(MessageSerializationInfo::UsesStructuredClone(extension.get()));

  extension = LoadAndExpectSuccess(
      "message_serialization_valid_structured_cloning.json");
  ASSERT_TRUE(extension);
  EXPECT_TRUE(MessageSerializationInfo::UsesStructuredClone(extension.get()));

  // Test invalid values.
  LoadAndExpectError("message_serialization_invalid.json",
                     manifest_errors::kInvalidMessageSerialization);
  LoadAndExpectError("message_serialization_invalid_format.json",
                     manifest_errors::kInvalidMessageSerialization);
}

TEST_F(MessageSerializationManifestTest, MessageSerializationFeatureDisabled) {
  ScopedCurrentChannel channel(version_info::Channel::CANARY);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      extensions_features::kStructuredCloningForMessaging);

  scoped_refptr<Extension> extension = LoadAndExpectSuccess(
      "message_serialization_valid_structured_cloning.json");
  ASSERT_TRUE(extension);
  EXPECT_FALSE(MessageSerializationInfo::UsesStructuredClone(extension.get()));
}

}  // namespace extensions
