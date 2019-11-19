// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/test/task_environment.h"
#include "extensions/browser/preload_check_group.h"
#include "extensions/browser/preload_check_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {
PreloadCheck::Error kDummyError1 = PreloadCheck::DISALLOWED_BY_POLICY;
PreloadCheck::Error kDummyError2 = PreloadCheck::BLACKLISTED_ID;
PreloadCheck::Error kDummyError3 = PreloadCheck::BLACKLISTED_UNKNOWN;
}

class PreloadCheckGroupTest : public testing::Test {
 public:
  PreloadCheckGroupTest()
      : check_group_(std::make_unique<PreloadCheckGroup>()) {}
  ~PreloadCheckGroupTest() override {}

 protected:
  // Adds a check to |check_group_|, storing its unique_ptr in |checks_|.
  void AddCheck(PreloadCheck::Errors errors, bool is_async = false) {
    auto check_stub = std::make_unique<PreloadCheckStub>(errors);
    check_stub->set_is_async(is_async);
    check_group_->AddCheck(check_stub.get());
    checks_.push_back(std::move(check_stub));
  }

  // Convenience method for add an async check.
  void AddAsyncCheck(PreloadCheck::Errors errors) {
    AddCheck(errors, /*is_async=*/true);
  }

  // Verifies that all checks have started.
  void ExpectStarted() {
    for (const auto& check : checks_)
      EXPECT_TRUE(check->started());
  }

  PreloadCheckRunner runner_;
  std::vector<std::unique_ptr<PreloadCheckStub>> checks_;
  std::unique_ptr<PreloadCheckGroup> check_group_;

 private:
  // Required for the asynchronous tests.
  base::test::SingleThreadTaskEnvironment task_environment_;
};

// Tests multiple succeeding checks.
TEST_F(PreloadCheckGroupTest, Succeed) {
  for (int i = 0; i < 3; i++)
    AddCheck(PreloadCheck::Errors());
  runner_.Run(check_group_.get());

  ExpectStarted();
  EXPECT_EQ(0u, runner_.errors().size());
}

// Tests multiple succeeding sync and async checks.
TEST_F(PreloadCheckGroupTest, SucceedAsync) {
  for (int i = 0; i < 2; i++) {
    AddCheck(PreloadCheck::Errors());
    AddAsyncCheck(PreloadCheck::Errors());
  }

  runner_.RunUntilComplete(check_group_.get());
  ExpectStarted();
  EXPECT_EQ(0u, runner_.errors().size());
}

// Tests failing checks.
TEST_F(PreloadCheckGroupTest, Fail) {
  AddCheck(PreloadCheck::Errors());
  AddAsyncCheck({kDummyError1, kDummyError2});
  AddCheck({kDummyError3});
  runner_.Run(check_group_.get());

  ExpectStarted();
  EXPECT_FALSE(runner_.called());

  // The runner is called with all errors.
  runner_.WaitForComplete();
  EXPECT_EQ(3u, runner_.errors().size());
}

// Tests failing synchronous checks with stop_on_first_error.
TEST_F(PreloadCheckGroupTest, FailFast) {
  check_group_->set_stop_on_first_error(true);

  AddCheck({kDummyError1, kDummyError2});
  AddCheck({kDummyError3});
  runner_.Run(check_group_.get());

  // After the first check fails, the remaining checks should not be started.
  EXPECT_TRUE(runner_.called());
  EXPECT_TRUE(checks_[0]->started());
  EXPECT_FALSE(checks_[1]->started());
  EXPECT_THAT(runner_.errors(),
              testing::UnorderedElementsAre(kDummyError1, kDummyError2));
}

// Tests failing asynchronous checks with stop_on_first_error.
TEST_F(PreloadCheckGroupTest, FailFastAsync) {
  check_group_->set_stop_on_first_error(true);

  AddCheck(PreloadCheck::Errors());
  AddAsyncCheck(PreloadCheck::Errors());
  AddAsyncCheck({kDummyError1});
  AddAsyncCheck({kDummyError2});
  runner_.Run(check_group_.get());

  // All checks were started, because the sync check passes.
  ExpectStarted();
  EXPECT_FALSE(runner_.called());
  runner_.WaitForComplete();

  // The first async check should have failed, triggering fail fast. The
  // second async check's failure should be ignored.
  EXPECT_THAT(runner_.errors(), testing::UnorderedElementsAre(kDummyError1));
}

// Tests we don't crash when the PreloadCheckGroup is destroyed prematurely.
TEST_F(PreloadCheckGroupTest, DestroyPreloadCheckGroup) {
  AddAsyncCheck({kDummyError1});
  AddAsyncCheck({kDummyError2});
  runner_.Run(check_group_.get());

  check_group_.reset();

  // Checks should have been started, but the runner is never called.
  ExpectStarted();
  runner_.WaitForIdle();
  EXPECT_FALSE(runner_.called());
}

}  // namespace extensions
