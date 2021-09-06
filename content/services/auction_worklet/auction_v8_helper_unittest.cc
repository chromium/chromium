// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/auction_v8_helper.h"

#include <limits>
#include <string>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/services/auction_worklet/worklet_v8_debug_test_util.h"
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

  void CompileAndRunScriptOnV8Thread(int context_group_id,
                                     const std::string& function_name,
                                     const GURL& url,
                                     const std::string& body) {
    helper_->v8_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](scoped_refptr<AuctionV8Helper> helper, int context_group_id,
               std::string function_name, GURL url, std::string body) {
              AuctionV8Helper::FullIsolateScope isolate_scope(helper.get());
              v8::Local<v8::UnboundScript> script;
              {
                v8::Context::Scope ctx(helper->scratch_context());
                absl::optional<std::string> error_msg;
                ASSERT_TRUE(
                    helper->Compile(body, url, context_group_id, error_msg)
                        .ToLocal(&script));
                EXPECT_FALSE(error_msg.has_value());
              }
              v8::Local<v8::Context> context = helper->CreateContext();
              std::vector<std::string> error_msgs;
              v8::Context::Scope ctx(context);
              v8::Local<v8::Value> result;
              ASSERT_TRUE(helper
                              ->RunScript(context, script, context_group_id,
                                          function_name,
                                          base::span<v8::Local<v8::Value>>(),
                                          error_msgs)
                              .ToLocal(&result));
            },
            helper_, context_group_id, function_name, url, body));
  }

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
    ASSERT_TRUE(
        helper_
            ->Compile("function foo() { return 1;}", GURL("https://foo.test/"),
                      AuctionV8Helper::kNoDebugContextGroupId, error_msg)
            .ToLocal(&script));
    EXPECT_FALSE(error_msg.has_value());
  }

  for (v8::Local<v8::Context> context :
       {helper_->scratch_context(), helper_->CreateContext()}) {
    std::vector<std::string> error_msgs;
    v8::Context::Scope ctx(context);
    v8::Local<v8::Value> result;
    ASSERT_TRUE(helper_
                    ->RunScript(context, script,
                                AuctionV8Helper::kNoDebugContextGroupId, "foo",
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
                              AuctionV8Helper::kNoDebugContextGroupId,
                              compile_error)
                    .ToLocal(&script));
    EXPECT_FALSE(compile_error.has_value());

    std::vector<std::string> error_msgs;
    v8::MaybeLocal<v8::Value> result = helper_->RunScript(
        context, script, AuctionV8Helper::kNoDebugContextGroupId, "foo",
        base::span<v8::Local<v8::Value>>(), error_msgs);
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
  ASSERT_TRUE(
      helper_
          ->Compile("function foo() { return 1;}", GURL("https://foo.test/"),
                    AuctionV8Helper::kNoDebugContextGroupId, compile_error)
          .ToLocal(&script));
  EXPECT_FALSE(compile_error.has_value());

  std::vector<std::string> error_msgs;
  v8::Local<v8::Value> result;
  ASSERT_TRUE(helper_
                  ->RunScript(context, script,
                              AuctionV8Helper::kNoDebugContextGroupId, "foo",
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
                            GURL("https://foo.test/"),
                            AuctionV8Helper::kNoDebugContextGroupId,
                            compile_error)
                  .ToLocal(&script));
  EXPECT_FALSE(compile_error.has_value());
  std::vector<std::string> error_msgs;
  EXPECT_TRUE(helper_
                  ->RunScript(context, script,
                              AuctionV8Helper::kNoDebugContextGroupId, "foo",
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
  ASSERT_FALSE(helper_
                   ->Compile("function foo() { ", GURL("https://foo.test/"),
                             AuctionV8Helper::kNoDebugContextGroupId, error_msg)
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
                              GURL("https://foo.test/"),
                              AuctionV8Helper::kNoDebugContextGroupId,
                              error_msg)
                    .ToLocal(&script));
    EXPECT_FALSE(error_msg.has_value());
  }

  v8::Local<v8::Context> context = helper_->CreateContext();
  std::vector<std::string> error_msgs;
  v8::Context::Scope ctx(context);
  v8::Local<v8::Value> result;
  ASSERT_FALSE(helper_
                   ->RunScript(context, script,
                               AuctionV8Helper::kNoDebugContextGroupId, "foo",
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
    ASSERT_TRUE(
        helper_
            ->Compile("function foo() { return 1;}", GURL("https://foo.test/"),
                      AuctionV8Helper::kNoDebugContextGroupId, error_msg)
            .ToLocal(&script));
    EXPECT_FALSE(error_msg.has_value());
  }

  v8::Local<v8::Context> context = helper_->CreateContext();

  std::vector<std::string> error_msgs;
  v8::Context::Scope ctx(context);
  v8::Local<v8::Value> result;
  ASSERT_FALSE(helper_
                   ->RunScript(context, script,
                               AuctionV8Helper::kNoDebugContextGroupId, "bar",
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
                              GURL("https://foo.test/"),
                              AuctionV8Helper::kNoDebugContextGroupId,
                              error_msg)
                    .ToLocal(&script));
    EXPECT_FALSE(error_msg.has_value());
  }

  v8::Local<v8::Context> context = helper_->CreateContext();

  std::vector<std::string> error_msgs;
  v8::Context::Scope ctx(context);
  v8::Local<v8::Value> result;
  ASSERT_FALSE(helper_
                   ->RunScript(context, script,
                               AuctionV8Helper::kNoDebugContextGroupId, "foo",
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
    ASSERT_TRUE(helper_
                    ->Compile(kScript, GURL("https://foo.test/"),
                              AuctionV8Helper::kNoDebugContextGroupId,
                              error_msg)
                    .ToLocal(&script));
    EXPECT_FALSE(error_msg.has_value());
  }

  v8::Local<v8::Context> context = helper_->CreateContext();

  std::vector<std::string> error_msgs;
  v8::Context::Scope ctx(context);
  v8::Local<v8::Value> result;
  ASSERT_FALSE(helper_
                   ->RunScript(context, script,
                               AuctionV8Helper::kNoDebugContextGroupId, "foo",
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

TEST_F(AuctionV8HelperTest, FormatScriptName) {
  v8::Local<v8::UnboundScript> script;
  v8::Context::Scope ctx(helper_->scratch_context());
  absl::optional<std::string> error_msg;
  ASSERT_TRUE(helper_
                  ->Compile("function foo() { return 1;}",
                            GURL("https://foo.test:8443/foo.js?v=3"),
                            AuctionV8Helper::kNoDebugContextGroupId, error_msg)
                  .ToLocal(&script));
  EXPECT_EQ("https://foo.test:8443/foo.js?v=3",
            helper_->FormatScriptName(script));
}

TEST_F(AuctionV8HelperTest, ContextIDs) {
  int resume_callback_invocations = 0;
  base::RepeatingClosure count_resume_callback_invocation =
      base::BindLambdaForTesting([&]() { ++resume_callback_invocations; });

  int id1 = helper_->AllocContextGroupIdAndSetResumeCallback(
      count_resume_callback_invocation);
  EXPECT_NE(AuctionV8Helper::kNoDebugContextGroupId, id1);
  ASSERT_EQ(0, resume_callback_invocations);

  // Invoking resume the first time invokes the callback.
  helper_->Resume(id1);
  ASSERT_EQ(1, resume_callback_invocations);

  // Later invocations don't do anything.
  helper_->Resume(id1);
  ASSERT_EQ(1, resume_callback_invocations);

  helper_->FreeContextGroupId(id1);
  // ... including after free.
  helper_->Resume(id1);
  ASSERT_EQ(1, resume_callback_invocations);

  // Or before allocation.
  helper_->Resume(id1 + 1);
  ASSERT_EQ(1, resume_callback_invocations);

  // Try with free before Resume call, too.
  int id2 = helper_->AllocContextGroupIdAndSetResumeCallback(
      count_resume_callback_invocation);
  EXPECT_NE(AuctionV8Helper::kNoDebugContextGroupId, id2);
  ASSERT_EQ(1, resume_callback_invocations);
  helper_->FreeContextGroupId(id2);
  helper_->Resume(id2);
  ASSERT_EQ(1, resume_callback_invocations);

  // Rudimentary test that two live IDs aren't the same.
  int id3 = helper_->AllocContextGroupIdAndSetResumeCallback(
      count_resume_callback_invocation);
  int id4 = helper_->AllocContextGroupIdAndSetResumeCallback(
      count_resume_callback_invocation);
  EXPECT_NE(AuctionV8Helper::kNoDebugContextGroupId, id3);
  EXPECT_NE(AuctionV8Helper::kNoDebugContextGroupId, id4);
  EXPECT_NE(id3, id4);
  helper_->Resume(id4);
  ASSERT_EQ(2, resume_callback_invocations);
  helper_->Resume(id3);
  ASSERT_EQ(3, resume_callback_invocations);
  helper_->FreeContextGroupId(id3);
  helper_->FreeContextGroupId(id4);
}

TEST_F(AuctionV8HelperTest, AllocWrap) {
  // Check what the ID allocator does when numbers wrap around and collide.
  int id1 =
      helper_->AllocContextGroupIdAndSetResumeCallback(base::OnceClosure());
  EXPECT_GT(id1, 0);
  helper_->SetLastContextGroupIdForTesting(std::numeric_limits<int>::max());
  int id2 =
      helper_->AllocContextGroupIdAndSetResumeCallback(base::OnceClosure());
  // `id2` should be positive and distinct from `id1`.
  EXPECT_GT(id2, 0);
  EXPECT_NE(id1, id2);

  helper_->FreeContextGroupId(id1);
  helper_->FreeContextGroupId(id2);
}

TEST_F(AuctionV8HelperTest, DebuggerBasics) {
  const char kScriptSrc[] = "function someFunction() { return 493043; }";
  const char kFunctionName[] = "someFunction";
  const char kURL[] = "https://foo.test/script.js";

  // Need to use a separate thread for debugger stuff.
  v8_scope_.reset();
  helper_ = AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner());
  ScopedInspectorSupport inspector_support(helper_.get());

  int id = AllocContextGroupIdAndWait(helper_);
  TestChannel* channel = inspector_support.ConnectDebuggerSession(id);
  channel->RunCommandAndWaitForResult(
      1, "Runtime.enable", R"({"id":1,"method":"Runtime.enable","params":{}})");
  channel->RunCommandAndWaitForResult(
      2, "Debugger.enable",
      R"({"id":2,"method":"Debugger.enable","params":{}})");

  CompileAndRunScriptOnV8Thread(id, kFunctionName, GURL(kURL), kScriptSrc);

  // Running a script in an ephemeral context produces a bunch of events.
  // The first pair of context_created/destroyed is for the compilation.
  TestChannel::Event context_created_event =
      channel->WaitForMethodNotification("Runtime.executionContextCreated");
  const std::string* name =
      context_created_event.value.FindStringPath("params.context.name");
  ASSERT_TRUE(name);
  EXPECT_EQ(kURL, *name);

  TestChannel::Event context_destroyed_event =
      channel->WaitForMethodNotification("Runtime.executionContextDestroyed");

  TestChannel::Event context_created2_event =
      channel->WaitForMethodNotification("Runtime.executionContextCreated");
  const std::string* name2 =
      context_created2_event.value.FindStringPath("params.context.name");
  ASSERT_TRUE(name2);
  EXPECT_EQ(kURL, *name2);

  TestChannel::Event script_parsed_event =
      channel->WaitForMethodNotification("Debugger.scriptParsed");
  const std::string* url =
      script_parsed_event.value.FindStringPath("params.url");
  ASSERT_TRUE(url);
  EXPECT_EQ(kURL, *url);
  const std::string* script_id =
      script_parsed_event.value.FindStringPath("params.scriptId");
  ASSERT_TRUE(script_id);

  TestChannel::Event context_destroyed2_event =
      channel->WaitForMethodNotification("Runtime.executionContextDestroyed");

  // Can fetch the source code for a debugger using the ID from the scriptParsed
  // command.
  const char kGetScriptSourceTemplate[] = R"({
    "id":3,
    "method":"Debugger.getScriptSource",
    "params":{"scriptId":"%s"}})";
  TestChannel::Event source_response = channel->RunCommandAndWaitForResult(
      3, "Debugger.getScriptSource",
      base::StringPrintf(kGetScriptSourceTemplate, script_id->c_str()));
  const std::string* parsed_src =
      source_response.value.FindStringPath("result.scriptSource");
  ASSERT_TRUE(parsed_src);
  EXPECT_EQ(kScriptSrc, *parsed_src);

  FreeContextGroupIdAndWait(helper_, id);
}

TEST_F(AuctionV8HelperTest, DebugCompileError) {
  const char kScriptSrc[] = "fuction someFunction() { return 493043; }";
  const char kURL[] = "https://foo.test/script.js";

  // Need to use a separate thread for debugger stuff.
  v8_scope_.reset();
  helper_ = AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner());
  ScopedInspectorSupport inspector_support(helper_.get());

  int id = AllocContextGroupIdAndWait(helper_);
  TestChannel* channel = inspector_support.ConnectDebuggerSession(id);
  channel->RunCommandAndWaitForResult(
      1, "Runtime.enable", R"({"id":1,"method":"Runtime.enable","params":{}})");
  channel->RunCommandAndWaitForResult(
      2, "Debugger.enable",
      R"({"id":2,"method":"Debugger.enable","params":{}})");

  helper_->v8_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<AuctionV8Helper> helper, int context_group_id,
             std::string url, std::string body) {
            AuctionV8Helper::FullIsolateScope isolate_scope(helper.get());
            v8::Local<v8::UnboundScript> script;
            {
              v8::Context::Scope ctx(helper->scratch_context());
              absl::optional<std::string> error_msg;
              ASSERT_FALSE(
                  helper->Compile(body, GURL(url), context_group_id, error_msg)
                      .ToLocal(&script));
            }
          },
          helper_, id, kURL, kScriptSrc));

  // Get events for context and error.
  TestChannel::Event context_created_event =
      channel->WaitForMethodNotification("Runtime.executionContextCreated");

  TestChannel::Event parse_error_event =
      channel->WaitForMethodNotification("Debugger.scriptFailedToParse");

  TestChannel::Event context_destroyed_event =
      channel->WaitForMethodNotification("Runtime.executionContextDestroyed");

  FreeContextGroupIdAndWait(helper_, id);
}

}  // namespace auction_worklet
