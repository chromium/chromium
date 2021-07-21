// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/auction_v8_helper.h"

#include <string>
#include <vector>

#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "gin/converter.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "v8/include/v8.h"

using testing::ElementsAre;
using testing::HasSubstr;
using testing::StartsWith;

namespace auction_worklet {

class AuctionV8HelperTest : public testing::Test {
 public:
  AuctionV8HelperTest() {
    helper_ = AuctionV8Helper::Create(base::ThreadTaskRunnerHandle::Get());
    // Here since we're using the same thread for everything, we need to spin
    // the event loop to let AuctionV8Helper finish initializing "off-thread";
    // normally PostTask semantics will ensure that anything that uses it on its
    // thread would happen after such initialization.
    base::RunLoop().RunUntilIdle();
    v8_scope_ =
        std::make_unique<AuctionV8Helper::FullIsolateScope>(helper_.get());
  }
  ~AuctionV8HelperTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<AuctionV8Helper> helper_;
  std::unique_ptr<AuctionV8Helper::FullIsolateScope> v8_scope_;
};

// Compile a script with the scratch context, and then run it in two different
// contexts.
TEST_F(AuctionV8HelperTest, Basic) {
  v8::Local<v8::UnboundScript> script;
  {
    v8::Context::Scope ctx(helper_->scratch_context());
    absl::optional<std::string> error_msg;
    ASSERT_TRUE(helper_
                    ->Compile("function foo() { return 1;}",
                              GURL("https://foo.test/"), error_msg)
                    .ToLocal(&script));
    EXPECT_FALSE(error_msg.has_value());
  }

  for (v8::Local<v8::Context> context :
       {helper_->scratch_context(), helper_->CreateContext()}) {
    std::vector<std::string> error_msgs;
    v8::Context::Scope ctx(context);
    v8::Local<v8::Value> result;
    ASSERT_TRUE(helper_
                    ->RunScript(context, script, "foo",
                                base::span<v8::Local<v8::Value>>(), error_msgs)
                    .ToLocal(&result));
    int int_result = 0;
    ASSERT_TRUE(gin::ConvertFromV8(helper_->isolate(), result, &int_result));
    EXPECT_EQ(1, int_result);
    EXPECT_TRUE(error_msgs.empty());
  }
}

// Check that timing out scripts works.
TEST_F(AuctionV8HelperTest, Timeout) {
  struct HangingScript {
    const char* script;
    bool top_level_hangs;
  };

  const HangingScript kHangingScripts[] = {
      // Script that times out when run. Its foo() method returns 1, but should
      // never be called.
      {R"(function foo() { return 1;}
        while(1);)",
       true},

      // Script that times out when foo() is called.
      {"function foo() {while (1);}", false},

      // Script that times out when run and when foo is called.
      {R"(function foo() {while (1);}
        while(1);)",
       true}};

  // Use a sorter timeout so test runs faster.
  const base::TimeDelta kScriptTimeout = base::TimeDelta::FromMilliseconds(20);
  helper_->set_script_timeout_for_testing(kScriptTimeout);

  for (const HangingScript& hanging_script : kHangingScripts) {
    base::TimeTicks start_time = base::TimeTicks::Now();
    v8::Local<v8::Context> context = helper_->CreateContext();
    v8::Context::Scope context_scope(context);

    v8::Local<v8::UnboundScript> script;
    absl::optional<std::string> compile_error;
    ASSERT_TRUE(helper_
                    ->Compile(hanging_script.script, GURL("https://foo.test/"),
                              compile_error)
                    .ToLocal(&script));
    EXPECT_FALSE(compile_error.has_value());

    std::vector<std::string> error_msgs;
    v8::MaybeLocal<v8::Value> result = helper_->RunScript(
        context, script, "foo", base::span<v8::Local<v8::Value>>(), error_msgs);
    EXPECT_TRUE(result.IsEmpty());
    EXPECT_THAT(
        error_msgs,
        ElementsAre(hanging_script.top_level_hangs
                        ? "https://foo.test/ top-level execution timed out."
                        : "https://foo.test/ execution of `foo` timed out."));

    // Make sure at least `kScriptTimeout` has passed, allowing for some time
    // skew between change in base::TimeTicks::Now() and the timeout. This
    // mostly serves to make sure the script timed out, instead of immediately
    // terminating.
    EXPECT_GE(base::TimeTicks::Now() - start_time,
              kScriptTimeout - base::TimeDelta::FromMilliseconds(10));
  }

  // Make sure it's still possible to run a script with the isolate after the
  // timeouts.
  v8::Local<v8::Context> context = helper_->CreateContext();
  v8::Context::Scope context_scope(context);
  v8::Local<v8::UnboundScript> script;
  absl::optional<std::string> compile_error;
  ASSERT_TRUE(helper_
                  ->Compile("function foo() { return 1;}",
                            GURL("https://foo.test/"), compile_error)
                  .ToLocal(&script));
  EXPECT_FALSE(compile_error.has_value());

  std::vector<std::string> error_msgs;
  v8::Local<v8::Value> result;
  ASSERT_TRUE(helper_
                  ->RunScript(context, script, "foo",
                              base::span<v8::Local<v8::Value>>(), error_msgs)
                  .ToLocal(&result));
  EXPECT_TRUE(error_msgs.empty());
  int int_result = 0;
  ASSERT_TRUE(gin::ConvertFromV8(helper_->isolate(), result, &int_result));
  EXPECT_EQ(1, int_result);
}

// Make sure the when CreateContext() is used, there's no access to the time,
// which mitigates Specter-style attacks.
TEST_F(AuctionV8HelperTest, NoTime) {
  v8::Local<v8::Context> context = helper_->CreateContext();
  v8::Context::Scope context_scope(context);

  // Make sure Date() is not accessible.
  v8::Local<v8::UnboundScript> script;
  absl::optional<std::string> compile_error;
  ASSERT_TRUE(helper_
                  ->Compile("function foo() { return Date();}",
                            GURL("https://foo.test/"), compile_error)
                  .ToLocal(&script));
  EXPECT_FALSE(compile_error.has_value());
  std::vector<std::string> error_msgs;
  EXPECT_TRUE(helper_
                  ->RunScript(context, script, "foo",
                              base::span<v8::Local<v8::Value>>(), error_msgs)
                  .IsEmpty());
  ASSERT_EQ(1u, error_msgs.size());
  EXPECT_THAT(error_msgs[0], StartsWith("https://foo.test/:1"));
  EXPECT_THAT(error_msgs[0], HasSubstr("ReferenceError"));
  EXPECT_THAT(error_msgs[0], HasSubstr("Date"));
}

// A script that doesn't compile.
TEST_F(AuctionV8HelperTest, CompileError) {
  v8::Local<v8::UnboundScript> script;
  v8::Context::Scope ctx(helper_->scratch_context());
  absl::optional<std::string> error_msg;
  ASSERT_FALSE(
      helper_
          ->Compile("function foo() { ", GURL("https://foo.test/"), error_msg)
          .ToLocal(&script));
  ASSERT_TRUE(error_msg.has_value());
  EXPECT_THAT(error_msg.value(), StartsWith("https://foo.test/:1 "));
  EXPECT_THAT(error_msg.value(), HasSubstr("SyntaxError"));
}

// Test for exception at runtime at top-level.
TEST_F(AuctionV8HelperTest, RunErrorTopLevel) {
  v8::Local<v8::UnboundScript> script;
  {
    v8::Context::Scope ctx(helper_->scratch_context());
    absl::optional<std::string> error_msg;
    ASSERT_TRUE(helper_
                    ->Compile("\n\nthrow new Error('I am an error');",
                              GURL("https://foo.test/"), error_msg)
                    .ToLocal(&script));
    EXPECT_FALSE(error_msg.has_value());
  }

  v8::Local<v8::Context> context = helper_->CreateContext();
  std::vector<std::string> error_msgs;
  v8::Context::Scope ctx(context);
  v8::Local<v8::Value> result;
  ASSERT_FALSE(helper_
                   ->RunScript(context, script, "foo",
                               base::span<v8::Local<v8::Value>>(), error_msgs)
                   .ToLocal(&result));
  EXPECT_THAT(
      error_msgs,
      ElementsAre("https://foo.test/:3 Uncaught Error: I am an error."));
}

// Test for when desired function isn't found
TEST_F(AuctionV8HelperTest, TargetFunctionNotFound) {
  v8::Local<v8::UnboundScript> script;
  {
    v8::Context::Scope ctx(helper_->scratch_context());
    absl::optional<std::string> error_msg;
    ASSERT_TRUE(helper_
                    ->Compile("function foo() { return 1;}",
                              GURL("https://foo.test/"), error_msg)
                    .ToLocal(&script));
    EXPECT_FALSE(error_msg.has_value());
  }

  v8::Local<v8::Context> context = helper_->CreateContext();

  std::vector<std::string> error_msgs;
  v8::Context::Scope ctx(context);
  v8::Local<v8::Value> result;
  ASSERT_FALSE(helper_
                   ->RunScript(context, script, "bar",
                               base::span<v8::Local<v8::Value>>(), error_msgs)
                   .ToLocal(&result));

  // This "not a function" and not "not found" since the lookup successfully
  // returns `undefined`.
  EXPECT_THAT(error_msgs,
              ElementsAre("https://foo.test/ `bar` is not a function."));
}

TEST_F(AuctionV8HelperTest, TargetFunctionError) {
  v8::Local<v8::UnboundScript> script;
  {
    v8::Context::Scope ctx(helper_->scratch_context());
    absl::optional<std::string> error_msg;
    ASSERT_TRUE(helper_
                    ->Compile("function foo() { return notfound;}",
                              GURL("https://foo.test/"), error_msg)
                    .ToLocal(&script));
    EXPECT_FALSE(error_msg.has_value());
  }

  v8::Local<v8::Context> context = helper_->CreateContext();

  std::vector<std::string> error_msgs;
  v8::Context::Scope ctx(context);
  v8::Local<v8::Value> result;
  ASSERT_FALSE(helper_
                   ->RunScript(context, script, "foo",
                               base::span<v8::Local<v8::Value>>(), error_msgs)
                   .ToLocal(&result));
  ASSERT_EQ(1u, error_msgs.size());

  EXPECT_THAT(error_msgs[0], StartsWith("https://foo.test/:1 "));
  EXPECT_THAT(error_msgs[0], HasSubstr("ReferenceError"));
  EXPECT_THAT(error_msgs[0], HasSubstr("notfound"));
}

TEST_F(AuctionV8HelperTest, LogThenError) {
  const char kScript[] = R"(
    console.debug('debug is there');
    console.error('error is also there');

    function foo() {
      console.info('info too');
      console.log('can', 'log', 'multiple', 'things');
      console.warn('conversions?', true);
      console.table('not the fancier stuff, though');
    }
  )";

  v8::Local<v8::UnboundScript> script;
  {
    v8::Context::Scope ctx(helper_->scratch_context());
    absl::optional<std::string> error_msg;
    ASSERT_TRUE(helper_->Compile(kScript, GURL("https://foo.test/"), error_msg)
                    .ToLocal(&script));
    EXPECT_FALSE(error_msg.has_value());
  }

  v8::Local<v8::Context> context = helper_->CreateContext();

  std::vector<std::string> error_msgs;
  v8::Context::Scope ctx(context);
  v8::Local<v8::Value> result;
  ASSERT_FALSE(helper_
                   ->RunScript(context, script, "foo",
                               base::span<v8::Local<v8::Value>>(), error_msgs)
                   .ToLocal(&result));
  ASSERT_EQ(error_msgs.size(), 6u);
  EXPECT_EQ("https://foo.test/ [Debug]: debug is there", error_msgs[0]);
  EXPECT_EQ("https://foo.test/ [Error]: error is also there", error_msgs[1]);
  EXPECT_EQ("https://foo.test/ [Info]: info too", error_msgs[2]);
  EXPECT_EQ("https://foo.test/ [Log]: can log multiple things", error_msgs[3]);
  EXPECT_EQ("https://foo.test/ [Warn]: conversions? true", error_msgs[4]);
  EXPECT_THAT(error_msgs[5], StartsWith("https://foo.test/:9"));
  EXPECT_THAT(error_msgs[5], HasSubstr("TypeError"));
  EXPECT_THAT(error_msgs[5], HasSubstr("table"));
}

}  // namespace auction_worklet
