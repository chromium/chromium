// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/policy_check.h"

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/test/test_browser_context.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/mock_extension_system.h"
#include "extensions/browser/preload_check.h"
#include "extensions/browser/preload_check_test_util.h"
#include "extensions/browser/test_extensions_browser_client.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

const char kDummyPolicyError[] = "Cannot install extension";

class ManagementPolicyMock : public ManagementPolicy::Provider {
 public:
  ManagementPolicyMock(const Extension* extension, bool may_load)
      : extension_(extension), may_load_(may_load) {}

  std::string GetDebugPolicyProviderName() const override {
    return "ManagementPolicyMock";
  }

  bool UserMayLoad(const Extension* extension,
                   base::string16* error) const override {
    EXPECT_EQ(extension_, extension);
    if (!may_load_)
      *error = base::ASCIIToUTF16(kDummyPolicyError);
    return may_load_;
  }

 private:
  const Extension* extension_;
  bool may_load_;
};

class TestExtensionSystem : public MockExtensionSystem {
 public:
  explicit TestExtensionSystem(content::BrowserContext* context)
      : MockExtensionSystem(context) {}
  ~TestExtensionSystem() override {}

  ManagementPolicy* management_policy() override { return &management_policy_; }

 private:
  ManagementPolicy management_policy_;
};

}  // namespace

class PolicyCheckTest : public ExtensionsTest {
 public:
  PolicyCheckTest() {}

  ~PolicyCheckTest() override {}

  void SetUp() override {
    ExtensionsTest::SetUp();

    // Replace the MockExtensionSystemFactory set by ExtensionsTest.
    extensions_browser_client()->set_extension_system_factory(&factory_);

    extension_ = ExtensionBuilder("dummy name").Build();
  }

 protected:
  scoped_refptr<const Extension> extension_;
  PreloadCheckRunner runner_;

 private:
  MockExtensionSystemFactory<TestExtensionSystem> factory_;
};

// Tests an allowed extension.
TEST_F(PolicyCheckTest, PolicySuccess) {
  PolicyCheck policy_check(browser_context(), extension_);
  runner_.Run(&policy_check);
  EXPECT_TRUE(runner_.called());
  EXPECT_EQ(0u, runner_.errors().size());
  EXPECT_TRUE(policy_check.GetErrorMessage().empty());
}

// Tests a disallowed extension.
TEST_F(PolicyCheckTest, PolicyFailure) {
  ManagementPolicyMock policy(extension_.get(), false);
  ExtensionSystem::Get(browser_context())
      ->management_policy()
      ->RegisterProvider(&policy);

  PolicyCheck policy_check(browser_context(), extension_);
  runner_.Run(&policy_check);
  EXPECT_TRUE(runner_.called());
  EXPECT_THAT(runner_.errors(), testing::UnorderedElementsAre(
                                    PreloadCheck::DISALLOWED_BY_POLICY));
  EXPECT_EQ(base::ASCIIToUTF16(kDummyPolicyError),
            policy_check.GetErrorMessage());
}

}  // namespace extensions
