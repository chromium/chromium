// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/blocklist_check.h"

#include "base/task/single_thread_task_runner.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "extensions/browser/blocklist.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/preload_check.h"
#include "extensions/browser/preload_check_test_util.h"
#include "extensions/browser/test_blocklist.h"
#include "extensions/browser/test_extension_prefs.h"
#include "extensions/browser/test_extensions_browser_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

class BlocklistCheckTest : public testing::Test {
 public:
  BlocklistCheckTest()
      : test_prefs_(base::SingleThreadTaskRunner::GetCurrentDefault(),
                    std::make_unique<content::TestBrowserContext>()) {
    ExtensionsBrowserClient::Set(&extensions_browser_client_);
    extensions_browser_client_.SetMainContext(test_prefs_.browser_context());
    blocklist_ = std::make_unique<Blocklist>(test_prefs_.browser_context());
    test_blocklist_ = std::make_unique<TestBlocklist>();
  }

 protected:
  void SetUp() override {
    test_blocklist_->Attach(blocklist_.get());
    extension_ = test_prefs_.AddExtension("foo");
  }

  void TearDown() override {
    test_blocklist_.reset();
    blocklist_.reset();
    BrowserContextDependencyManager::GetInstance()
        ->DestroyBrowserContextServices(test_prefs_.browser_context());

    testing::Test::TearDown();
  }

  void SetBlocklistState(BlocklistState state) {
    test_blocklist_->SetBlocklistState(extension_->id(), state,
                                       /*notify=*/true);
  }

  Blocklist* blocklist() { return blocklist_.get(); }
  scoped_refptr<Extension> extension_;
  PreloadCheckRunner runner_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestExtensionPrefs test_prefs_;
  TestExtensionsBrowserClient extensions_browser_client_;
  std::unique_ptr<Blocklist> blocklist_;
  std::unique_ptr<TestBlocklist> test_blocklist_;
};

}  // namespace

// Tests that the blocklist check identifies a blocklisted extension.
TEST_F(BlocklistCheckTest, BlocklistedMalware) {
  SetBlocklistState(BLOCKLISTED_MALWARE);

  BlocklistCheck check(blocklist(), extension_);
  runner_.RunUntilComplete(&check);

  EXPECT_THAT(runner_.errors(), testing::UnorderedElementsAre(
                                    PreloadCheck::Error::kBlocklistedId));
  EXPECT_TRUE(check.GetErrorMessage().empty());
}

// Tests that the blocklist check ignores a non-blocklisted extension.
TEST_F(BlocklistCheckTest, Pass) {
  SetBlocklistState(NOT_BLOCKLISTED);

  BlocklistCheck check(blocklist(), extension_);
  runner_.RunUntilComplete(&check);

  EXPECT_EQ(0u, runner_.errors().size());
  EXPECT_TRUE(check.GetErrorMessage().empty());
}

// Tests that destroying the check after starting it does not cause errors.
TEST_F(BlocklistCheckTest, ResetCheck) {
  SetBlocklistState(BLOCKLISTED_MALWARE);

  {
    BlocklistCheck check(blocklist(), extension_);
    runner_.Run(&check);
  }

  runner_.WaitForIdle();
  EXPECT_FALSE(runner_.called());
}

}  // namespace extensions
