// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/devtools_download_manager_delegate.h"

#include <memory>

#include "content/browser/download/download_manager_impl.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content::protocol {

namespace {

class TestDownloadManagerDelegate : public DownloadManagerDelegate {
 public:
  void Shutdown() override {}
  bool SupportsHistoryLoading() override { return supports_history_loading_; }
  void set_supports_history_loading(bool supports) {
    supports_history_loading_ = supports;
  }

 private:
  bool supports_history_loading_ = false;
};

}  // namespace

class DevToolsDownloadManagerDelegateTest : public testing::Test {
 public:
  DevToolsDownloadManagerDelegateTest() = default;
  ~DevToolsDownloadManagerDelegateTest() override = default;

  void SetUp() override {
    browser_context_ = std::make_unique<TestBrowserContext>();
    auto download_manager =
        std::make_unique<DownloadManagerImpl>(browser_context_.get());
    test_delegate_ = std::make_unique<TestDownloadManagerDelegate>();
    download_manager->SetDelegate(test_delegate_.get());
    browser_context_->SetDownloadManagerForTesting(std::move(download_manager));
  }

  void TearDown() override { browser_context_.reset(); }

 protected:
  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<TestDownloadManagerDelegate> test_delegate_;
};

TEST_F(DevToolsDownloadManagerDelegateTest, SupportsHistoryLoadingForwarded) {
  test_delegate_->set_supports_history_loading(true);
  auto* devtools_delegate =
      DevToolsDownloadManagerDelegate::GetOrCreateInstance(
          browser_context_.get());
  EXPECT_TRUE(devtools_delegate->SupportsHistoryLoading());

  test_delegate_->set_supports_history_loading(false);
  EXPECT_FALSE(devtools_delegate->SupportsHistoryLoading());
}

TEST_F(DevToolsDownloadManagerDelegateTest,
       SupportsHistoryLoadingWithoutOriginalDelegate) {
  auto browser_context = std::make_unique<TestBrowserContext>();
  auto download_manager =
      std::make_unique<DownloadManagerImpl>(browser_context.get());
  browser_context->SetDownloadManagerForTesting(std::move(download_manager));

  auto* devtools_delegate =
      DevToolsDownloadManagerDelegate::GetOrCreateInstance(
          browser_context.get());
  EXPECT_FALSE(devtools_delegate->SupportsHistoryLoading());
}

}  // namespace content::protocol
