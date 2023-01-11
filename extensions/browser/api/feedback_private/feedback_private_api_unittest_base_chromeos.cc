// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/feedback_private/feedback_private_api_unittest_base_chromeos.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/api/feedback_private/log_source_access_manager.h"
#include "extensions/browser/api/feedback_private/log_source_resource.h"
#include "extensions/common/api/feedback_private.h"
#include "extensions/shell/browser/api/feedback_private/shell_feedback_private_delegate.h"
#include "extensions/shell/browser/shell_extensions_api_client.h"

namespace extensions {

namespace {

using api::feedback_private::LogSource;
using system_logs::SystemLogsResponse;
using system_logs::SystemLogsSource;

// A fake MAC address used to test redaction.
const char kDummyMacAddress[] = "11:22:33:44:55:66";

std::unique_ptr<KeyedService> ApiResourceManagerTestFactory(
    content::BrowserContext* context) {
  return std::make_unique<ApiResourceManager<LogSourceResource>>(context);
}

// A dummy SystemLogsSource that does not require real system logs to be
// available during testing.
class TestSingleLogSource : public SystemLogsSource {
 public:
  explicit TestSingleLogSource(LogSource type)
      : SystemLogsSource(ToString(type)) {}

  TestSingleLogSource(const TestSingleLogSource&) = delete;
  TestSingleLogSource& operator=(const TestSingleLogSource&) = delete;

  ~TestSingleLogSource() override = default;

  // Fetch() will return a single different string each time, in the following
  // sequence: "a", " bb", "  ccc", until 25 spaces followed by 26 z's. After
  // that, it returns |kDummyMacAddress| before repeating the entire process.
  // It will never return an empty result.
  void Fetch(system_logs::SysLogsSourceCallback callback) override {
    std::string result = GetNextLogResult();
    DCHECK_GT(result.size(), 0U);

    auto result_map = std::make_unique<SystemLogsResponse>();
    result_map->emplace("", result);

    // Do not directly pass the result to the callback, because that's not how
    // log sources actually work. Instead, simulate the asynchronous operation
    // of a SystemLogsSource by invoking the callback separately.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(result_map)));
  }

 private:
  std::string GetNextLogResult() {
    if (call_count_ == kNumCharsToIterate) {
      call_count_ = 0;
      return kDummyMacAddress;
    }
    std::string result =
        std::string(call_count_, ' ') +
        std::string(call_count_ + 1, kInitialChar + call_count_);
    ++call_count_;
    return result;
  }

  // Iterate over the whole lowercase alphabet, starting from 'a'.
  const int kNumCharsToIterate = 26;
  const char kInitialChar = 'a';

  // Keep track of how many times Fetch() has been called, in order to determine
  // its behavior each time.
  int call_count_ = 0;
};

class TestFeedbackPrivateDelegate : public ShellFeedbackPrivateDelegate {
 public:
  TestFeedbackPrivateDelegate() = default;

  TestFeedbackPrivateDelegate(const TestFeedbackPrivateDelegate&) = delete;
  TestFeedbackPrivateDelegate& operator=(const TestFeedbackPrivateDelegate&) =
      delete;

  ~TestFeedbackPrivateDelegate() override = default;

  std::unique_ptr<system_logs::SystemLogsSource> CreateSingleLogSource(
      api::feedback_private::LogSource source_type) const override {
    return std::make_unique<TestSingleLogSource>(source_type);
  }
};

class TestExtensionsAPIClient : public ShellExtensionsAPIClient {
 public:
  TestExtensionsAPIClient() = default;

  TestExtensionsAPIClient(const TestExtensionsAPIClient&) = delete;
  TestExtensionsAPIClient& operator=(const TestExtensionsAPIClient&) = delete;

  ~TestExtensionsAPIClient() override = default;

  // ShellExtensionsApiClient implementation:
  FeedbackPrivateDelegate* GetFeedbackPrivateDelegate() override {
    if (!feedback_private_delegate_) {
      feedback_private_delegate_ =
          std::make_unique<TestFeedbackPrivateDelegate>();
    }
    return feedback_private_delegate_.get();
  }

 private:
  std::unique_ptr<FeedbackPrivateDelegate> feedback_private_delegate_;
};

}  // namespace

FeedbackPrivateApiUnittestBase::FeedbackPrivateApiUnittestBase() = default;
FeedbackPrivateApiUnittestBase::~FeedbackPrivateApiUnittestBase() = default;

void FeedbackPrivateApiUnittestBase::SetUp() {
  extensions_api_client_ = std::make_unique<TestExtensionsAPIClient>();

  ApiUnitTest::SetUp();

  // The ApiResourceManager used for LogSourceResource is destroyed every time
  // a unit test finishes, during TearDown(). There is no way to re-create it
  // normally. The below code forces it to be re-created during SetUp(), so
  // that there is always a valid ApiResourceManager<LogSourceResource> when
  // subsequent unit tests are running.
  ApiResourceManager<LogSourceResource>::GetFactoryInstance()
      ->SetTestingFactoryAndUse(
          browser_context(),
          base::BindRepeating(&ApiResourceManagerTestFactory));
}

void FeedbackPrivateApiUnittestBase::TearDown() {
  // Clear the rate limit override that tests may have set.
  LogSourceAccessManager::SetRateLimitingTimeoutForTesting(nullptr);

  ApiUnitTest::TearDown();
}

}  // namespace extensions
