// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/auction_v8_helper.h"

#include <limits>
#include <string>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/services/auction_worklet/worklet_devtools_debug_test_util.h"
#include "content/services/auction_worklet/worklet_v8_debug_test_util.h"
#include "gin/converter.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-forward.h"

using testing::ElementsAre;
using testing::HasSubstr;
using testing::StartsWith;

namespace auction_worklet {

class AuctionV8HelperTest : public testing::Test {
 public:
  explicit AuctionV8HelperTest(
      base::test::TaskEnvironment::TimeSource time_mode =
          base::test::TaskEnvironment::TimeSource::SYSTEM_TIME)
      : task_environment_(time_mode) {
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

  void CompileAndRunScriptOnV8Thread(
      scoped_refptr<AuctionV8Helper::DebugId> debug_id,
      const std::string& function_name,
      const GURL& url,
      const std::string& body,
      bool expect_success = true,
      base::OnceClosure done = base::OnceClosure(),
      int* result_out = nullptr) {
    DCHECK(debug_id);
    helper_->v8_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](scoped_refptr<AuctionV8Helper> helper,
               scoped_refptr<AuctionV8Helper::DebugId> debug_id,
               std::string function_name, GURL url, std::string body,
               bool expect_success, base::OnceClosure done, int* result_out) {
              AuctionV8Helper::FullIsolateScope isolate_scope(helper.get());
              v8::Local<v8::UnboundScript> script;
              {
                v8::Context::Scope ctx(helper->scratch_context());
                absl::optional<std::string> error_msg;
                ASSERT_TRUE(
                    helper->Compile(body, url, debug_id.get(), error_msg)
                        .ToLocal(&script));
                EXPECT_FALSE(error_msg.has_value());
              }
              v8::Local<v8::Context> context = helper->CreateContext();
              std::vector<std::string> error_msgs;
              v8::Context::Scope ctx(context);
              v8::Local<v8::Value> result;
              // This is here since it needs to be before RunScript() ---
              // doing it before Compile() doesn't work.
              helper->MaybeTriggerInstrumentationBreakpoint(*debug_id, "start");
              helper->MaybeTriggerInstrumentationBreakpoint(*debug_id,
                                                            "start2");
              bool success = helper
                                 ->RunScript(context, script, debug_id.get(),
                                             function_name,
                                             base::span<v8::Local<v8::Value>>(),
                                             error_msgs)
                                 .ToLocal(&result);
              EXPECT_EQ(expect_success, success);
              if (result_out) {
                // If the caller wants to look at *result_out (including to see
                // if it's unchanged), the done callback must be used to be
                // sure that the read is performed after this sequence is
                // complete.
                CHECK(!done.is_null());

                if (success) {
                  ASSERT_TRUE(gin::ConvertFromV8(helper->isolate(), result,
                                                 result_out));
                }
              }
              if (!done.is_null())
                std::move(done).Run();
            },
            helper_, std::move(debug_id), function_name, url, body,
            expect_success, std::move(done), result_out));
  }

  void ConnectToDevToolsAgent(
      scoped_refptr<AuctionV8Helper::DebugId> debug_id,
      mojo::PendingReceiver<blink::mojom::DevToolsAgent> agent_receiver) {
    DCHECK(debug_id);
    helper_->v8_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](scoped_refptr<AuctionV8Helper> helper,
               mojo::PendingReceiver<blink::mojom::DevToolsAgent>
                   agent_receiver,
               scoped_refptr<base::SequencedTaskRunner> mojo_thread,
               scoped_refptr<AuctionV8Helper::DebugId> debug_id) {
              helper->ConnectDevToolsAgent(std::move(agent_receiver),
                                           std::move(mojo_thread), *debug_id);
            },
            helper_, std::move(agent_receiver),
            base::SequencedTaskRunnerHandle::Get(), std::move(debug_id)));
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
    ASSERT_TRUE(helper_
                    ->Compile("function foo() { return 1;}",
                              GURL("https://foo.test/"),
                              /*debug_id=*/nullptr, error_msg)
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
                                /*debug_id=*/nullptr, "foo",
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

  // Use a shorter timeout so test runs faster.
  const base::TimeDelta kScriptTimeout = base::Milliseconds(20);
  helper_->set_script_timeout_for_testing(kScriptTimeout);

  for (const HangingScript& hanging_script : kHangingScripts) {
    base::TimeTicks start_time = base::TimeTicks::Now();
    v8::Local<v8::Context> context = helper_->CreateContext();
    v8::Context::Scope context_scope(context);

    v8::Local<v8::UnboundScript> script;
    absl::optional<std::string> compile_error;
    ASSERT_TRUE(helper_
                    ->Compile(hanging_script.script, GURL("https://foo.test/"),
                              /*debug_id=*/nullptr, compile_error)
                    .ToLocal(&script));
    EXPECT_FALSE(compile_error.has_value());

    std::vector<std::string> error_msgs;
    v8::MaybeLocal<v8::Value> result =
        helper_->RunScript(context, script, /*debug_id=*/nullptr, "foo",
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
              kScriptTimeout - base::Milliseconds(10));
  }

  // Make sure it's still possible to run a script with the isolate after the
  // timeouts.
  v8::Local<v8::Context> context = helper_->CreateContext();
  v8::Context::Scope context_scope(context);
  v8::Local<v8::UnboundScript> script;
  absl::optional<std::string> compile_error;
  ASSERT_TRUE(helper_
                  ->Compile("function foo() { return 1;}",
                            GURL("https://foo.test/"),
                            /*debug_id=*/nullptr, compile_error)
                  .ToLocal(&script));
  EXPECT_FALSE(compile_error.has_value());

  std::vector<std::string> error_msgs;
  v8::Local<v8::Value> result;
  ASSERT_TRUE(helper_
                  ->RunScript(context, script,
                              /*debug_id=*/nullptr, "foo",
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
                            /*debug_id=*/nullptr, compile_error)
                  .ToLocal(&script));
  EXPECT_FALSE(compile_error.has_value());
  std::vector<std::string> error_msgs;
  EXPECT_TRUE(helper_
                  ->RunScript(context, script,
                              /*debug_id=*/nullptr, "foo",
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
                             /*debug_id=*/nullptr, error_msg)
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
                              /*debug_id=*/nullptr, error_msg)
                    .ToLocal(&script));
    EXPECT_FALSE(error_msg.has_value());
  }

  v8::Local<v8::Context> context = helper_->CreateContext();
  std::vector<std::string> error_msgs;
  v8::Context::Scope ctx(context);
  v8::Local<v8::Value> result;
  ASSERT_FALSE(helper_
                   ->RunScript(context, script,
                               /*debug_id=*/nullptr, "foo",
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
                              GURL("https://foo.test/"),
                              /*debug_id=*/nullptr, error_msg)
                    .ToLocal(&script));
    EXPECT_FALSE(error_msg.has_value());
  }

  v8::Local<v8::Context> context = helper_->CreateContext();

  std::vector<std::string> error_msgs;
  v8::Context::Scope ctx(context);
  v8::Local<v8::Value> result;
  ASSERT_FALSE(helper_
                   ->RunScript(context, script,
                               /*debug_id=*/nullptr, "bar",
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
                              /*debug_id=*/nullptr, error_msg)
                    .ToLocal(&script));
    EXPECT_FALSE(error_msg.has_value());
  }

  v8::Local<v8::Context> context = helper_->CreateContext();

  std::vector<std::string> error_msgs;
  v8::Context::Scope ctx(context);
  v8::Local<v8::Value> result;
  ASSERT_FALSE(helper_
                   ->RunScript(context, script,
                               /*debug_id=*/nullptr, "foo",
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
                              /*debug_id=*/nullptr, error_msg)
                    .ToLocal(&script));
    EXPECT_FALSE(error_msg.has_value());
  }

  v8::Local<v8::Context> context = helper_->CreateContext();

  std::vector<std::string> error_msgs;
  v8::Context::Scope ctx(context);
  v8::Local<v8::Value> result;
  ASSERT_FALSE(helper_
                   ->RunScript(context, script,
                               /*debug_id=*/nullptr, "foo",
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
                            /*debug_id=*/nullptr, error_msg)
                  .ToLocal(&script));
  EXPECT_EQ("https://foo.test:8443/foo.js?v=3",
            helper_->FormatScriptName(script));
}

TEST_F(AuctionV8HelperTest, ContextIDs) {
  int resume_callback_invocations = 0;
  base::RepeatingClosure count_resume_callback_invocation =
      base::BindLambdaForTesting([&]() { ++resume_callback_invocations; });

  auto id1 = base::MakeRefCounted<AuctionV8Helper::DebugId>(helper_.get());
  id1->SetResumeCallback(count_resume_callback_invocation);
  EXPECT_GT(id1->context_group_id(), 0);
  ASSERT_EQ(0, resume_callback_invocations);

  // Invoking resume the first time invokes the callback.
  helper_->Resume(id1->context_group_id());
  ASSERT_EQ(1, resume_callback_invocations);

  // Later invocations don't do anything.
  helper_->Resume(id1->context_group_id());
  ASSERT_EQ(1, resume_callback_invocations);

  // ... including after free.
  int save_id1 = id1->context_group_id();
  id1->AbortDebuggerPauses();
  id1.reset();
  helper_->Resume(save_id1);
  ASSERT_EQ(1, resume_callback_invocations);

  // Or before allocation.
  helper_->Resume(save_id1 + 1);
  ASSERT_EQ(1, resume_callback_invocations);

  // Try with free before Resume call, too.
  auto id2 = base::MakeRefCounted<AuctionV8Helper::DebugId>(helper_.get());
  id2->SetResumeCallback(count_resume_callback_invocation);
  EXPECT_GT(id2->context_group_id(), 0);
  ASSERT_EQ(1, resume_callback_invocations);
  int save_id2 = id2->context_group_id();
  id2->AbortDebuggerPauses();
  id2.reset();
  helper_->Resume(save_id2);
  ASSERT_EQ(1, resume_callback_invocations);

  // Rudimentary test that two live IDs aren't the same.
  auto id3 = base::MakeRefCounted<AuctionV8Helper::DebugId>(helper_.get());
  id3->SetResumeCallback(count_resume_callback_invocation);
  auto id4 = base::MakeRefCounted<AuctionV8Helper::DebugId>(helper_.get());
  id4->SetResumeCallback(count_resume_callback_invocation);
  int save_id3 = id3->context_group_id();
  int save_id4 = id4->context_group_id();
  EXPECT_GT(save_id3, 0);
  EXPECT_GT(save_id4, 0);
  EXPECT_NE(save_id3, save_id4);
  helper_->Resume(save_id4);
  ASSERT_EQ(2, resume_callback_invocations);
  helper_->Resume(save_id3);
  ASSERT_EQ(3, resume_callback_invocations);

  id3->AbortDebuggerPauses();
  id4->AbortDebuggerPauses();
}

TEST_F(AuctionV8HelperTest, AllocWrap) {
  // Check what the ID allocator does when numbers wrap around and collide.
  auto id1 = base::MakeRefCounted<AuctionV8Helper::DebugId>(helper_.get());
  EXPECT_GT(id1->context_group_id(), 0);
  helper_->SetLastContextGroupIdForTesting(std::numeric_limits<int>::max());
  auto id2 = base::MakeRefCounted<AuctionV8Helper::DebugId>(helper_.get());
  // `id2` should be positive and distinct from `id1`.
  EXPECT_GT(id2->context_group_id(), 0);
  EXPECT_NE(id1->context_group_id(), id2->context_group_id());

  id1->AbortDebuggerPauses();
  id2->AbortDebuggerPauses();
}

TEST_F(AuctionV8HelperTest, DebuggerBasics) {
  const char kScriptSrc[] = "function someFunction() { return 493043; }";
  const char kFunctionName[] = "someFunction";
  const char kURL[] = "https://foo.test/script.js";

  // Need to use a separate thread for debugger stuff.
  v8_scope_.reset();
  helper_ = AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner());
  ScopedInspectorSupport inspector_support(helper_.get());

  auto id = base::MakeRefCounted<AuctionV8Helper::DebugId>(helper_.get());
  TestChannel* channel =
      inspector_support.ConnectDebuggerSession(id->context_group_id());
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

  id->AbortDebuggerPauses();
}

TEST_F(AuctionV8HelperTest, DebugCompileError) {
  const char kScriptSrc[] = "fuction someFunction() { return 493043; }";
  const char kURL[] = "https://foo.test/script.js";

  // Need to use a separate thread for debugger stuff.
  v8_scope_.reset();
  helper_ = AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner());
  ScopedInspectorSupport inspector_support(helper_.get());

  auto id = base::MakeRefCounted<AuctionV8Helper::DebugId>(helper_.get());
  TestChannel* channel =
      inspector_support.ConnectDebuggerSession(id->context_group_id());
  channel->RunCommandAndWaitForResult(
      1, "Runtime.enable", R"({"id":1,"method":"Runtime.enable","params":{}})");
  channel->RunCommandAndWaitForResult(
      2, "Debugger.enable",
      R"({"id":2,"method":"Debugger.enable","params":{}})");

  helper_->v8_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<AuctionV8Helper> helper,
             scoped_refptr<AuctionV8Helper::DebugId> debug_id, std::string url,
             std::string body) {
            AuctionV8Helper::FullIsolateScope isolate_scope(helper.get());
            v8::Local<v8::UnboundScript> script;
            {
              v8::Context::Scope ctx(helper->scratch_context());
              absl::optional<std::string> error_msg;
              ASSERT_FALSE(
                  helper->Compile(body, GURL(url), debug_id.get(), error_msg)
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

  id->AbortDebuggerPauses();
}

TEST_F(AuctionV8HelperTest, DevToolsDebuggerBasics) {
  const char kSession[] = "123-456";
  const char kScript[] = R"(
    var multiplier = 2;
    function compute() {
      return multiplier * 3;
    }
  )";

  for (bool use_binary_protocol : {false, true}) {
    SCOPED_TRACE(use_binary_protocol);
    // Need to use a separate thread for debugger stuff.
    v8_scope_.reset();
    helper_ = AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner());

    auto id = base::MakeRefCounted<AuctionV8Helper::DebugId>(helper_.get());

    mojo::Remote<blink::mojom::DevToolsAgent> agent_remote;
    ConnectToDevToolsAgent(id, agent_remote.BindNewPipeAndPassReceiver());

    TestDevToolsAgentClient debug_client(std::move(agent_remote), kSession,
                                         use_binary_protocol);
    debug_client.RunCommandAndWaitForResult(
        TestDevToolsAgentClient::Channel::kMain, 1, "Runtime.enable",
        R"({"id":1,"method":"Runtime.enable","params":{}})");
    debug_client.RunCommandAndWaitForResult(
        TestDevToolsAgentClient::Channel::kMain, 2, "Debugger.enable",
        R"({"id":2,"method":"Debugger.enable","params":{}})");

    const char kBreakpointCommand[] = R"({
          "id":3,
          "method":"Debugger.setBreakpointByUrl",
          "params": {
            "lineNumber": 2,
            "url": "https://example.com/test.js",
            "columnNumber": 0,
            "condition": ""
          }})";

    debug_client.RunCommandAndWaitForResult(
        TestDevToolsAgentClient::Channel::kMain, 3,
        "Debugger.setBreakpointByUrl", kBreakpointCommand);

    int result = -1;
    base::RunLoop result_run_loop;
    CompileAndRunScriptOnV8Thread(
        id, "compute", GURL("https://example.com/test.js"), kScript,
        /*expect_success=*/true, result_run_loop.QuitClosure(), &result);

    // Eat completion from parsing.
    debug_client.WaitForMethodNotification("Runtime.executionContextDestroyed");

    TestDevToolsAgentClient::Event script_parsed =
        debug_client.WaitForMethodNotification("Debugger.scriptParsed");
    const std::string* url = script_parsed.value.FindStringPath("params.url");
    ASSERT_TRUE(url);
    EXPECT_EQ(*url, "https://example.com/test.js");
    absl::optional<int> context_id =
        script_parsed.value.FindIntPath("params.executionContextId");
    ASSERT_TRUE(context_id.has_value());

    // Wait for breakpoint to hit.
    TestDevToolsAgentClient::Event breakpoint_hit =
        debug_client.WaitForMethodNotification("Debugger.paused");

    base::Value* hit_breakpoints =
        breakpoint_hit.value.FindListPath("params.hitBreakpoints");
    ASSERT_TRUE(hit_breakpoints);
    base::Value::ConstListView hit_breakpoints_list =
        hit_breakpoints->GetList();
    ASSERT_EQ(1u, hit_breakpoints_list.size());
    ASSERT_TRUE(hit_breakpoints_list[0].is_string());
    EXPECT_EQ("1:2:0:https://example.com/test.js",
              hit_breakpoints_list[0].GetString());

    const char kCommandTemplate[] = R"({
      "id": 4,
      "method": "Runtime.evaluate",
      "params": {
        "expression": "multiplier = 10",
        "contextId": %d
      }
    })";

    // Change the state before resuming.
    // Post-breakpoint params must be run on IO pipe, any main thread commands
    // won't do things yet.
    debug_client.RunCommandAndWaitForResult(
        TestDevToolsAgentClient::Channel::kIO, 4, "Runtime.evaluate",
        base::StringPrintf(kCommandTemplate, context_id.value()));

    // Resume.
    debug_client.RunCommandAndWaitForResult(
        TestDevToolsAgentClient::Channel::kIO, 10, "Debugger.resume",
        R"({"id":10,"method":"Debugger.resume","params":{}})");

    // Wait for actual completion.
    debug_client.WaitForMethodNotification("Runtime.executionContextDestroyed");

    // Produced value changed by the write to `multiplier`.
    result_run_loop.Run();
    EXPECT_EQ(30, result);

    id->AbortDebuggerPauses();
  }
}

TEST_F(AuctionV8HelperTest, DevToolsAgentDebuggerInstrumentationBreakpoint) {
  const char kSession[] = "123-456";
  const char kScript[] = R"(
    function compute() {
      return 42;
    }
  )";

  for (bool use_binary_protocol : {false, true}) {
    for (bool use_multiple_breakpoints : {false, true}) {
      std::string test_name =
          std::string(use_binary_protocol ? "Binary " : "JSON ") +
          (use_multiple_breakpoints ? "Multi" : "Single");
      SCOPED_TRACE(test_name);
      // Need to use a separate thread for debugger stuff.
      v8_scope_.reset();
      helper_ = AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner());

      auto id = base::MakeRefCounted<AuctionV8Helper::DebugId>(helper_.get());

      mojo::Remote<blink::mojom::DevToolsAgent> agent_remote;
      ConnectToDevToolsAgent(id, agent_remote.BindNewPipeAndPassReceiver());

      TestDevToolsAgentClient debug_client(std::move(agent_remote), kSession,
                                           use_binary_protocol);
      debug_client.RunCommandAndWaitForResult(
          TestDevToolsAgentClient::Channel::kMain, 1, "Runtime.enable",
          R"({"id":1,"method":"Runtime.enable","params":{}})");
      debug_client.RunCommandAndWaitForResult(
          TestDevToolsAgentClient::Channel::kMain, 2, "Debugger.enable",
          R"({"id":2,"method":"Debugger.enable","params":{}})");

      debug_client.RunCommandAndWaitForResult(
          TestDevToolsAgentClient::Channel::kMain, 3,
          "EventBreakpoints.setInstrumentationBreakpoint",
          MakeInstrumentationBreakpointCommand(3, "set", "start"));
      debug_client.RunCommandAndWaitForResult(
          TestDevToolsAgentClient::Channel::kMain, 4,
          "EventBreakpoints.setInstrumentationBreakpoint",
          MakeInstrumentationBreakpointCommand(4, "set", "start2"));
      debug_client.RunCommandAndWaitForResult(
          TestDevToolsAgentClient::Channel::kMain, 5,
          "EventBreakpoints.setInstrumentationBreakpoint",
          MakeInstrumentationBreakpointCommand(5, "set", "start3"));
      if (!use_multiple_breakpoints) {
        debug_client.RunCommandAndWaitForResult(
            TestDevToolsAgentClient::Channel::kMain, 6,
            "EventBreakpoints.removeInstrumentationBreakpoint",
            MakeInstrumentationBreakpointCommand(6, "remove", "start2"));
      }

      int result = -1;
      base::RunLoop result_run_loop;
      CompileAndRunScriptOnV8Thread(
          id, "compute", GURL("https://example.com/test.js"), kScript,
          /*expect_success=*/true, result_run_loop.QuitClosure(), &result);

      // Wait for the pause.
      TestDevToolsAgentClient::Event breakpoint_hit =
          debug_client.WaitForMethodNotification("Debugger.paused");

      // Make sure we identify the event the way DevTools frontend expects.
      if (use_multiple_breakpoints) {
        // Expect both 'start' and 'start2' to hit, so the event will list both
        // inside the 'data.reasons' list, and top-level 'reason' field to say
        // 'ambiguous' to reflect it.
        const std::string* reason =
            breakpoint_hit.value.FindStringPath("params.reason");
        ASSERT_TRUE(reason);
        EXPECT_EQ("ambiguous", *reason);

        const base::Value* reasons =
            breakpoint_hit.value.FindListPath("params.data.reasons");
        ASSERT_TRUE(reasons);
        base::Value::ConstListView reasons_list = reasons->GetList();
        ASSERT_EQ(2u, reasons_list.size());
        ASSERT_TRUE(reasons_list[0].is_dict());
        ASSERT_TRUE(reasons_list[1].is_dict());
        const std::string* ev1 =
            reasons_list[0].FindStringPath("auxData.eventName");
        const std::string* ev2 =
            reasons_list[1].FindStringPath("auxData.eventName");
        const std::string* r1 = reasons_list[0].FindStringPath("reason");
        const std::string* r2 = reasons_list[1].FindStringPath("reason");
        ASSERT_TRUE(ev1);
        ASSERT_TRUE(ev2);
        ASSERT_TRUE(r1);
        ASSERT_TRUE(r2);
        EXPECT_EQ("instrumentation:start", *ev1);
        EXPECT_EQ("instrumentation:start2", *ev2);
        EXPECT_EQ("EventListener", *r1);
        EXPECT_EQ("EventListener", *r2);
      } else {
        // Here we expect 'start' to be the only event, since we remove
        // 'start2', and 'start3' isn't checked by
        // CompileAndRunScriptOnV8Thread.
        EXPECT_FALSE(breakpoint_hit.value.FindPath("params.data.reasons"));
        const std::string* reason =
            breakpoint_hit.value.FindStringPath("params.reason");
        ASSERT_TRUE(reason);
        EXPECT_EQ("EventListener", *reason);

        const std::string* event_name =
            breakpoint_hit.value.FindStringPath("params.data.eventName");
        ASSERT_TRUE(event_name);
        EXPECT_EQ("instrumentation:start", *event_name);
      }

      // Resume.
      debug_client.RunCommandAndWaitForResult(
          TestDevToolsAgentClient::Channel::kIO, 10, "Debugger.resume",
          R"({"id":10,"method":"Debugger.resume","params":{}})");

      // Wait for result.
      result_run_loop.Run();
      EXPECT_EQ(42, result);

      id->AbortDebuggerPauses();
    }
  }
}

TEST_F(AuctionV8HelperTest, DevToolsDebuggerInvalidCommand) {
  const char kSession[] = "ABCD-EFGH";
  for (bool use_binary_protocol : {false, true}) {
    SCOPED_TRACE(use_binary_protocol);
    // Need to use a separate thread for debugger stuff.
    v8_scope_.reset();
    helper_ = AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner());

    auto id = base::MakeRefCounted<AuctionV8Helper::DebugId>(helper_.get());

    mojo::Remote<blink::mojom::DevToolsAgent> agent_remote;
    ConnectToDevToolsAgent(id, agent_remote.BindNewPipeAndPassReceiver());

    TestDevToolsAgentClient debug_client(std::move(agent_remote), kSession,
                                         use_binary_protocol);
    TestDevToolsAgentClient::Event result =
        debug_client.RunCommandAndWaitForResult(
            TestDevToolsAgentClient::Channel::kMain, 1, "NoSuchThing.enable",
            R"({"id":1,"method":"NoSuchThing.enable","params":{}})");
    EXPECT_TRUE(result.value.FindDictKey("error"));

    id->AbortDebuggerPauses();
  }
}

TEST_F(AuctionV8HelperTest, DevToolsDeleteSessionPipeLate) {
  // Test that deleting session pipe after the agent is fine.
  const char kSession[] = "ABCD-EFGH";
  const bool use_binary_protocol = true;

  v8_scope_.reset();
  helper_ = AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner());

  auto id = base::MakeRefCounted<AuctionV8Helper::DebugId>(helper_.get());

  mojo::Remote<blink::mojom::DevToolsAgent> agent_remote;
  ConnectToDevToolsAgent(id, agent_remote.BindNewPipeAndPassReceiver());

  TestDevToolsAgentClient debug_client(std::move(agent_remote), kSession,
                                       use_binary_protocol);
  task_environment_.RunUntilIdle();

  id->AbortDebuggerPauses();
  id.reset();
  helper_.reset();
  task_environment_.RunUntilIdle();
}

class MockTimeAuctionV8HelperTest : public AuctionV8HelperTest {
 public:
  MockTimeAuctionV8HelperTest()
      : AuctionV8HelperTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void Wait(base::TimeDelta wait_time) {
    // We can't use TaskEnvironment::FastForwardBy since the v8 thread is
    // blocked in debugger, so instead we post a task on the timer thread
    // which then can be reasoned about with respect to the timeout.
    base::RunLoop run_loop;
    helper_->GetTimeoutTimerRunnerForTesting()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), wait_time);
    task_environment_.AdvanceClock(wait_time);
    run_loop.Run();
  }
};

TEST_F(MockTimeAuctionV8HelperTest, TimelimitDebug) {
  // Test that being paused on a breakpoint for a while doesn't trigger the
  // execution time limit.

  const char kSession[] = "123-456";
  const char kScript[] = R"(
    function compute() {
      return 3;
    }
  )";

  // Need to use a separate thread for debugger stuff.
  v8_scope_.reset();
  helper_ = AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner());

  auto id = base::MakeRefCounted<AuctionV8Helper::DebugId>(helper_.get());
  mojo::Remote<blink::mojom::DevToolsAgent> agent_remote;
  ConnectToDevToolsAgent(id, agent_remote.BindNewPipeAndPassReceiver());

  TestDevToolsAgentClient debug_client(std::move(agent_remote), kSession,
                                       /*use_binary_protocol=*/true);
  debug_client.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 1, "Runtime.enable",
      R"({"id":1,"method":"Runtime.enable","params":{}})");
  debug_client.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 2, "Debugger.enable",
      R"({"id":2,"method":"Debugger.enable","params":{}})");

  const char kBreakpointCommand[] = R"({
        "id":3,
        "method":"Debugger.setBreakpointByUrl",
        "params": {
          "lineNumber": 0,
          "url": "https://example.com/test.js",
          "columnNumber": 0,
          "condition": ""
        }})";

  debug_client.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 3, "Debugger.setBreakpointByUrl",
      kBreakpointCommand);

  int result = -1;
  base::RunLoop result_run_loop;
  CompileAndRunScriptOnV8Thread(
      id, "compute", GURL("https://example.com/test.js"), kScript,
      /*expect_success=*/true, result_run_loop.QuitClosure(), &result);
  // Wait for breakpoint to hit.
  TestDevToolsAgentClient::Event breakpoint_hit =
      debug_client.WaitForMethodNotification("Debugger.paused");

  // Make sure more time has happened than the timeout.
  Wait(2 * AuctionV8Helper::kScriptTimeout);

  // Resume the script, it should still finish.
  debug_client.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kIO, 10, "Debugger.resume",
      R"({"id":10,"method":"Debugger.resume","params":{}})");

  result_run_loop.Run();
  EXPECT_EQ(3, result);

  id->AbortDebuggerPauses();
}

TEST_F(AuctionV8HelperTest, DebugTimeout) {
  // Test that timeout still works after pausing in the debugger and resuming.

  // Use a shorter timeout so test runs faster.
  const base::TimeDelta kScriptTimeout = base::Milliseconds(20);
  helper_->set_script_timeout_for_testing(kScriptTimeout);

  const char kSession[] = "123-456";
  const char kScript[] = R"(
    var a = 42;
    function compute() {
      while (true) {}
    }
  )";
  // Need to use a separate thread for debugger stuff.
  v8_scope_.reset();
  helper_ = AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner());

  auto id = base::MakeRefCounted<AuctionV8Helper::DebugId>(helper_.get());
  mojo::Remote<blink::mojom::DevToolsAgent> agent_remote;
  ConnectToDevToolsAgent(id, agent_remote.BindNewPipeAndPassReceiver());

  TestDevToolsAgentClient debug_client(std::move(agent_remote), kSession,
                                       /*use_binary_protocol=*/false);
  debug_client.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 1, "Runtime.enable",
      R"({"id":1,"method":"Runtime.enable","params":{}})");
  debug_client.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 2, "Debugger.enable",
      R"({"id":2,"method":"Debugger.enable","params":{}})");

  const char kBreakpointCommand[] = R"({
        "id":3,
        "method":"Debugger.setBreakpointByUrl",
        "params": {
          "lineNumber": 0,
          "url": "https://example.com/test.js",
          "columnNumber": 0,
          "condition": ""
        }})";

  debug_client.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 3, "Debugger.setBreakpointByUrl",
      kBreakpointCommand);

  int result = -1;
  base::RunLoop result_run_loop;
  CompileAndRunScriptOnV8Thread(
      id, "compute", GURL("https://example.com/test.js"), kScript,
      /*expect_success=*/false, result_run_loop.QuitClosure(), &result);
  // Wait for breakpoint to hit.
  TestDevToolsAgentClient::Event breakpoint_hit =
      debug_client.WaitForMethodNotification("Debugger.paused");
  EXPECT_FALSE(result_run_loop.AnyQuitCalled());

  // Resume the script, it should still timeout.
  debug_client.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kIO, 10, "Debugger.resume",
      R"({"id":10,"method":"Debugger.resume","params":{}})");

  result_run_loop.Run();
  EXPECT_EQ(-1, result);
  id->AbortDebuggerPauses();
}

}  // namespace auction_worklet
