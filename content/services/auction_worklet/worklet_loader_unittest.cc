// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/worklet_loader.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/worklet_test_util.h"
#include "net/http/http_status_code.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "v8/include/v8.h"

using testing::HasSubstr;
using testing::StartsWith;

namespace auction_worklet {
namespace {

const char kValidScript[] = "function foo() {}";
const char kInvalidScript[] = "Invalid Script";

// None of these tests make sure the right script is compiled, these tests
// merely check success/failure of trying to load a worklet.
class WorkletLoaderTest : public testing::Test {
 public:
  WorkletLoaderTest() = default;
  ~WorkletLoaderTest() override = default;

  void LoadWorkletCallback(
      std::unique_ptr<v8::Global<v8::UnboundScript>> worklet_script,
      absl::optional<std::string> error_msg) {
    load_succeeded_ = !!worklet_script;
    error_msg_ = std::move(error_msg);
    EXPECT_EQ(load_succeeded_, !error_msg_.has_value());
    run_loop_.Quit();
  }

  std::string last_error_msg() const {
    return error_msg_.value_or("Not an error");
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  network::TestURLLoaderFactory url_loader_factory_;
  AuctionV8Helper v8_helper_;
  GURL url_ = GURL("https://foo.test/");
  base::RunLoop run_loop_;
  bool load_succeeded_ = false;
  absl::optional<std::string> error_msg_;
};

TEST_F(WorkletLoaderTest, NetworkError) {
  // Make this look like a valid response in all ways except the response code.
  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, absl::nullopt,
              kValidScript, kAllowFledgeHeader, net::HTTP_NOT_FOUND);
  WorkletLoader worklet_loader(
      &url_loader_factory_, url_, &v8_helper_,
      base::BindOnce(&WorkletLoaderTest::LoadWorkletCallback,
                     base::Unretained(this)));
  run_loop_.Run();
  EXPECT_FALSE(load_succeeded_);
  EXPECT_EQ("Failed to load https://foo.test/ HTTP status = 404 Not Found.",
            last_error_msg());
}

TEST_F(WorkletLoaderTest, CompileError) {
  AddJavascriptResponse(&url_loader_factory_, url_, kInvalidScript);
  WorkletLoader worklet_loader(
      &url_loader_factory_, url_, &v8_helper_,
      base::BindOnce(&WorkletLoaderTest::LoadWorkletCallback,
                     base::Unretained(this)));
  run_loop_.Run();
  EXPECT_FALSE(load_succeeded_);
  EXPECT_THAT(last_error_msg(), StartsWith("https://foo.test/:1 "));
  EXPECT_THAT(last_error_msg(), HasSubstr("SyntaxError"));
}

TEST_F(WorkletLoaderTest, Success) {
  AddJavascriptResponse(&url_loader_factory_, url_, kValidScript);
  WorkletLoader worklet_loader(
      &url_loader_factory_, url_, &v8_helper_,
      base::BindOnce(&WorkletLoaderTest::LoadWorkletCallback,
                     base::Unretained(this)));
  run_loop_.Run();
  EXPECT_TRUE(load_succeeded_);
}

// Make sure the V8 isolate is released before the callback is invoked on
// success, so that the loader and helper can be torn down without crashing
// during the callback.
TEST_F(WorkletLoaderTest, DeleteDuringCallbackSuccess) {
  AddJavascriptResponse(&url_loader_factory_, url_, kValidScript);
  auto v8_helper = std::make_unique<AuctionV8Helper>();
  base::RunLoop run_loop;
  std::unique_ptr<WorkletLoader> worklet_loader =
      std::make_unique<WorkletLoader>(
          &url_loader_factory_, url_, v8_helper.get(),
          base::BindLambdaForTesting(
              [&](std::unique_ptr<v8::Global<v8::UnboundScript>> worklet_script,
                  absl::optional<std::string> error_msg) {
                EXPECT_TRUE(worklet_script);
                EXPECT_FALSE(error_msg.has_value());
                worklet_script.reset();
                worklet_loader.reset();
                v8_helper.reset();
                run_loop.Quit();
              }));
  run_loop.Run();
}

// Make sure the V8 isolate is released before the callback is invoked on
// compile failure, so that the loader and helper can be torn down without
// crashing during the callback.
TEST_F(WorkletLoaderTest, DeleteDuringCallbackCompileError) {
  AddJavascriptResponse(&url_loader_factory_, url_, kInvalidScript);
  auto v8_helper = std::make_unique<AuctionV8Helper>();
  base::RunLoop run_loop;
  std::unique_ptr<WorkletLoader> worklet_loader =
      std::make_unique<WorkletLoader>(
          &url_loader_factory_, url_, v8_helper.get(),
          base::BindLambdaForTesting(
              [&](std::unique_ptr<v8::Global<v8::UnboundScript>> worklet_script,
                  absl::optional<std::string> error_msg) {
                EXPECT_FALSE(worklet_script);
                ASSERT_TRUE(error_msg.has_value());
                EXPECT_THAT(error_msg.value(),
                            StartsWith("https://foo.test/:1 "));
                EXPECT_THAT(error_msg.value(), HasSubstr("SyntaxError"));
                worklet_loader.reset();
                v8_helper.reset();
                run_loop.Quit();
              }));
  run_loop.Run();
}

}  // namespace
}  // namespace auction_worklet
