// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/auction_v8_logger.h"

#include <string>
#include <vector>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/worklet_v8_debug_test_util.h"
#include "gin/converter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-external.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-maybe.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"

namespace auction_worklet {

// Class to add "warn1" and "warn2" properties to the passed in object, which
// trigger console warnings of "Warning 1" and "Warning 2" when first accessed.
class TestLazyFiller {
 public:
  TestLazyFiller(AuctionV8Helper* v8_helper,
                 v8::Local<v8::Object> object,
                 AuctionV8Logger* v8_logger)
      : v8_helper_(v8_helper), v8_logger_(v8_logger) {
    v8::Isolate* isolate = v8_helper->isolate();

    v8::Maybe<bool> success = object->SetLazyDataProperty(
        isolate->GetCurrentContext(), gin::StringToSymbol(isolate, "warn1"),
        &TestLazyFiller::Warn1, v8::External::New(isolate, this),
        /*attributes=*/v8::None,
        /*getter_side_effect_type=*/v8::SideEffectType::kHasNoSideEffect,
        /*setter_side_effect_type=*/v8::SideEffectType::kHasSideEffect);
    CHECK(success.IsJust() && success.FromJust());

    success = object->SetLazyDataProperty(
        isolate->GetCurrentContext(), gin::StringToSymbol(isolate, "warn2"),
        &TestLazyFiller::Warn2, v8::External::New(isolate, this),
        /*attributes=*/v8::None,
        /*getter_side_effect_type=*/v8::SideEffectType::kHasNoSideEffect,
        /*setter_side_effect_type=*/v8::SideEffectType::kHasSideEffect);
    CHECK(success.IsJust() && success.FromJust());
  }

  ~TestLazyFiller() = default;

 private:
  static void Warn1(v8::Local<v8::Name> name,
                    const v8::PropertyCallbackInfo<v8::Value>& info) {
    TestLazyFiller* self =
        static_cast<TestLazyFiller*>(v8::External::Cast(*info.Data())->Value());
    self->v8_logger_->LogConsoleWarning("Warning 1");
    info.GetReturnValue().Set(
        v8::Boolean::New(self->v8_helper_->isolate(), true));
  }

  static void Warn2(v8::Local<v8::Name> name,
                    const v8::PropertyCallbackInfo<v8::Value>& info) {
    TestLazyFiller* self =
        static_cast<TestLazyFiller*>(v8::External::Cast(*info.Data())->Value());
    self->v8_logger_->LogConsoleWarning("Warning 2");
    info.GetReturnValue().Set(
        v8::Boolean::New(self->v8_helper_->isolate(), false));
  }

  const raw_ptr<AuctionV8Helper> v8_helper_;
  const raw_ptr<AuctionV8Logger> v8_logger_;
};

class AuctionV8LoggerTest : public testing::Test {
 public:
  explicit AuctionV8LoggerTest() {
    helper_ = AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner());
  }

  // Compiles the passed in script on the V8 thread, uses TestLazyFiller to add
  // to Globals, and then runs the `function_name` function from the passed in
  // script.
  void CompileAndRunScriptOnV8Thread(
      scoped_refptr<AuctionV8Helper::DebugId> debug_id,
      const std::string& function_name,
      const std::string& body) {
    DCHECK(debug_id);
    base::RunLoop run_loop;
    helper_->v8_runner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          AuctionV8Helper::FullIsolateScope isolate_scope(helper_.get());

          v8::Local<v8::UnboundScript> script;
          {
            v8::Context::Scope ctx(helper_->scratch_context());
            std::optional<std::string> error_msg;
            ASSERT_TRUE(helper_->Compile(body, url_, debug_id.get(), error_msg)
                            .ToLocal(&script));
            EXPECT_FALSE(error_msg.has_value());
          }
          v8::Local<v8::Context> context = helper_->CreateContext();
          std::vector<std::string> error_msgs;
          v8::Context::Scope ctx(context);
          AuctionV8Logger v8_logger(helper_.get(), context);
          TestLazyFiller test_lazy_filler(helper_.get(), context->Global(),
                                          &v8_logger);

          auto timeout =
              helper_->CreateTimeLimit(/*script_timeout=*/std::nullopt);
          bool success = helper_->RunScript(context, script, debug_id.get(),
                                            timeout.get(), error_msgs) ==
                         AuctionV8Helper::Result::kSuccess;
          if (success) {
            v8::Local<v8::Value> result;
            v8::MaybeLocal<v8::Value> maybe_result;
            success =
                helper_->CallFunction(
                    context, debug_id.get(), helper_->FormatScriptName(script),
                    function_name, base::span<v8::Local<v8::Value>>(),
                    timeout.get(), maybe_result,
                    error_msgs) == AuctionV8Helper::Result::kSuccess &&
                maybe_result.ToLocal(&result);
          }
          EXPECT_TRUE(success);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<AuctionV8Helper> helper_;
  const GURL url_{"https://foo.test/"};
};

// Runs a script that should trigger two warnings, and makes sure those warnings
// are received when a debugger is attached.
TEST_F(AuctionV8LoggerTest, LogConsoleWarning) {
  ScopedInspectorSupport inspector_support(helper_.get());

  auto id = base::MakeRefCounted<AuctionV8Helper::DebugId>(helper_.get());
  TestChannel* channel =
      inspector_support.ConnectDebuggerSessionAndRuntimeEnable(
          id->context_group_id());

  const char kScript[] = R"(
    function foo() {
      let x = warn1;
      let y = warn2;
    }
  )";

  CompileAndRunScriptOnV8Thread(id, "foo", kScript);
  channel->WaitForAndValidateConsoleMessage(
      "warning", /*json_args=*/R"([{"type":"string", "value":"Warning 1"}])",
      /*stack_trace_size=*/1, /*function=*/"foo", /*url=*/url_,
      /*line_number=*/2);
  channel->WaitForAndValidateConsoleMessage(
      "warning", /*json_args=*/R"([{"type":"string", "value":"Warning 2"}])",
      /*stack_trace_size=*/1, /*function=*/"foo", /*url=*/url_,
      /*line_number=*/3);
  channel->ExpectNoMoreConsoleEvents();
  id->AbortDebuggerPauses();
}

}  // namespace auction_worklet
