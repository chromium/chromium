// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/runtime_data.h"

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

// Creates a very simple extension with a background page.
scoped_refptr<const Extension> CreateExtensionWithBackgroundPage() {
  return ExtensionBuilder("test")
      .SetBackgroundContext(
          ExtensionBuilder::BackgroundContext::BACKGROUND_PAGE)
      .SetID("id2")
      .Build();
}

class RuntimeDataTest : public testing::Test {
 public:
  RuntimeDataTest() : registry_(nullptr), runtime_data_(&registry_) {}
  ~RuntimeDataTest() override {}

 protected:
  ExtensionRegistry registry_;
  RuntimeData runtime_data_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RuntimeDataTest);
};

TEST_F(RuntimeDataTest, IsBackgroundPageReady) {
  // An extension without a background page is always considered ready.
  scoped_refptr<const Extension> no_background =
      ExtensionBuilder("Test").Build();
  EXPECT_TRUE(runtime_data_.IsBackgroundPageReady(no_background.get()));

  // An extension with a background page is not ready until the flag is set.
  scoped_refptr<const Extension> with_background =
      CreateExtensionWithBackgroundPage();
  EXPECT_FALSE(runtime_data_.IsBackgroundPageReady(with_background.get()));

  // The flag can be toggled.
  runtime_data_.SetBackgroundPageReady(with_background->id(), true);
  EXPECT_TRUE(runtime_data_.IsBackgroundPageReady(with_background.get()));
  runtime_data_.SetBackgroundPageReady(with_background->id(), false);
  EXPECT_FALSE(runtime_data_.IsBackgroundPageReady(with_background.get()));
}

TEST_F(RuntimeDataTest, IsBeingUpgraded) {
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();

  // An extension is not being upgraded until the flag is set.
  EXPECT_FALSE(runtime_data_.IsBeingUpgraded(extension->id()));

  // The flag can be toggled.
  runtime_data_.SetBeingUpgraded(extension->id(), true);
  EXPECT_TRUE(runtime_data_.IsBeingUpgraded(extension->id()));
  runtime_data_.SetBeingUpgraded(extension->id(), false);
  EXPECT_FALSE(runtime_data_.IsBeingUpgraded(extension->id()));
}

// Unloading an extension erases any data that shouldn't explicitly be kept
// across loads.
TEST_F(RuntimeDataTest, OnExtensionUnloaded) {
  scoped_refptr<const Extension> extension =
      CreateExtensionWithBackgroundPage();
  runtime_data_.SetBackgroundPageReady(extension->id(), true);
  ASSERT_TRUE(runtime_data_.HasExtensionForTesting(extension->id()));
  runtime_data_.SetBeingUpgraded(extension->id(), true);

  runtime_data_.OnExtensionUnloaded(nullptr, extension.get(),
                                    UnloadedExtensionReason::DISABLE);
  EXPECT_TRUE(runtime_data_.HasExtensionForTesting(extension->id()));
  EXPECT_FALSE(runtime_data_.IsBackgroundPageReady(extension.get()));
  EXPECT_TRUE(runtime_data_.IsBeingUpgraded(extension->id()));
}

}  // namespace
}  // namespace extensions
