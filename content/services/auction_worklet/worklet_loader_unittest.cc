// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/worklet_loader.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/worklet_test_util.h"
#include "content/services/auction_worklet/worklet_v8_debug_test_util.h"
#include "net/http/http_status_code.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

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
  WorkletLoaderTest() {
    v8_helper_ = AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner());
  }

  ~WorkletLoaderTest() override { task_environment_.RunUntilIdle(); }

  void LoadWorkletCallback(WorkletLoader::Result worklet_script,
                           absl::optional<std::string> error_msg) {
    load_succeeded_ = worklet_script.success();
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
  scoped_refptr<AuctionV8Helper> v8_helper_;
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
      &url_loader_factory_, url_, v8_helper_,
      scoped_refptr<AuctionV8Helper::DebugId>(),
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
      &url_loader_factory_, url_, v8_helper_,
      scoped_refptr<AuctionV8Helper::DebugId>(),
      base::BindOnce(&WorkletLoaderTest::LoadWorkletCallback,
                     base::Unretained(this)));
  run_loop_.Run();
  EXPECT_FALSE(load_succeeded_);
  EXPECT_THAT(last_error_msg(), StartsWith("https://foo.test/:1 "));
  EXPECT_THAT(last_error_msg(), HasSubstr("SyntaxError"));
}

TEST_F(WorkletLoaderTest, CompileErrorWithDebugger) {
  ScopedInspectorSupport inspector_support(v8_helper_.get());
  auto id = base::MakeRefCounted<AuctionV8Helper::DebugId>(v8_helper_.get());
  TestChannel* channel =
      inspector_support.ConnectDebuggerSession(id->context_group_id());
  channel->RunCommandAndWaitForResult(
      1, "Runtime.enable", R"({"id":1,"method":"Runtime.enable","params":{}})");
  channel->RunCommandAndWaitForResult(
      2, "Debugger.enable",
      R"({"id":2,"method":"Debugger.enable","params":{}})");

  AddJavascriptResponse(&url_loader_factory_, url_, kInvalidScript);
  WorkletLoader worklet_loader(
      &url_loader_factory_, url_, v8_helper_, id,
      base::BindOnce(&WorkletLoaderTest::LoadWorkletCallback,
                     base::Unretained(this)));
  run_loop_.Run();
  EXPECT_FALSE(load_succeeded_);
  channel->WaitForMethodNotification("Debugger.scriptFailedToParse");

  id->AbortDebuggerPauses();
}

TEST_F(WorkletLoaderTest, Success) {
  AddJavascriptResponse(&url_loader_factory_, url_, kValidScript);
  WorkletLoader worklet_loader(
      &url_loader_factory_, url_, v8_helper_,
      scoped_refptr<AuctionV8Helper::DebugId>(),
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
  auto v8_helper = AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner());
  base::RunLoop run_loop;
  std::unique_ptr<WorkletLoader> worklet_loader =
      std::make_unique<WorkletLoader>(
          &url_loader_factory_, url_, v8_helper.get(),
          scoped_refptr<AuctionV8Helper::DebugId>(),
          base::BindLambdaForTesting(
              [&](WorkletLoader::Result worklet_script,
                  absl::optional<std::string> error_msg) {
                EXPECT_TRUE(worklet_script.success());
                EXPECT_FALSE(error_msg.has_value());
                worklet_script = WorkletLoader::Result();
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
  auto v8_helper = AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner());
  base::RunLoop run_loop;
  std::unique_ptr<WorkletLoader> worklet_loader =
      std::make_unique<WorkletLoader>(
          &url_loader_factory_, url_, v8_helper.get(),
          scoped_refptr<AuctionV8Helper::DebugId>(),
          base::BindLambdaForTesting(
              [&](WorkletLoader::Result worklet_script,
                  absl::optional<std::string> error_msg) {
                EXPECT_FALSE(worklet_script.success());
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

// Testcase where the loader is deleted after it queued the parsing of
// the script on V8 thread, but before that parsing completes.
TEST_F(WorkletLoaderTest, DeleteBeforeCallback) {
  // Wedge the V8 thread so we can order loader deletion before script parsing.
  base::WaitableEvent* event_handle = WedgeV8Thread(v8_helper_.get());

  AddJavascriptResponse(&url_loader_factory_, url_, kValidScript);
  auto worklet_loader = std::make_unique<WorkletLoader>(
      &url_loader_factory_, url_, v8_helper_,
      scoped_refptr<AuctionV8Helper::DebugId>(),
      base::BindOnce([](WorkletLoader::Result worklet_script,
                        absl::optional<std::string> error_msg) {
        ADD_FAILURE() << "Callback should not be invoked since loader deleted";
      }));
  run_loop_.RunUntilIdle();
  worklet_loader.reset();
  event_handle->Signal();
}

}  // namespace
}  // namespace auction_worklet
