// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/auction_v8_helper.h"

#include <stdint.h>

#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/worklet_devtools_debug_test_util.h"
#include "content/services/auction_worklet/worklet_v8_debug_test_util.h"
#include "gin/converter.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/interest_group/ad_display_size.h"
#include "url/gurl.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-wasm.h"

using testing::ElementsAre;
using testing::HasSubstr;
using testing::StartsWith;

namespace auction_worklet {

// The bytes of a minimal WebAssembly module, courtesy of
// v8/test/cctest/test-api-wasm.cc
const char kMinimalWasmModuleBytes[] = {0x00, 0x61, 0x73, 0x6d,
                                        0x01, 0x00, 0x00, 0x00};

// ConnectDevToolsAgent takes an associated interface, which normally needs to
// be passed through a different pipe to be usable.  The usual way of testing
// this is by using BindNewEndpointAndPassDedicatedReceiver to force creation
// of a new pipe.  Unfortunately, this doesn't appear to be compatible with how
// our threads are setup.  So instead, we emulate how this would normally be
// used: by call to a worklet, and just have a mock implementation that only
// supports ConnectDevToolsAgent.
class DebugConnector : public auction_worklet::mojom::BidderWorklet {
 public:
  // Expected to be run on V8 thread.
  static void Create(
      scoped_refptr<AuctionV8Helper> auction_v8_helper,
      scoped_refptr<base::SequencedTaskRunner> mojo_thread,
      scoped_refptr<AuctionV8Helper::DebugId> debug_id,
      mojo::PendingReceiver<auction_worklet::mojom::BidderWorklet>
          pending_receiver) {
    DCHECK(auction_v8_helper->v8_runner()->RunsTasksInCurrentSequence());
    auto instance = base::WrapUnique(
        new DebugConnector(std::move(auction_v8_helper), std::move(mojo_thread),
                           std::move(debug_id)));
    mojo::MakeSelfOwnedReceiver(std::move(instance),
                                std::move(pending_receiver));
  }

  void BeginGenerateBid(
      auction_worklet::mojom::BidderWorkletNonSharedParamsPtr
          bidder_worklet_non_shared_params,
      auction_worklet::mojom::KAnonymityBidMode kanon_mode,
      const url::Origin& interest_group_join_origin,
      const std::optional<GURL>& direct_from_seller_per_buyer_signals,
      const std::optional<GURL>& direct_from_seller_auction_signals,
      const url::Origin& browser_signal_seller_origin,
      const std::optional<url::Origin>& browser_signal_top_level_seller_origin,
      const base::TimeDelta browser_signal_recency,
      blink::mojom::BiddingBrowserSignalsPtr bidding_browser_signals,
      base::Time auction_start_time,
      const std::optional<blink::AdSize>& requested_ad_size,
      uint16_t multi_bid_limit,
      uint64_t trace_id,
      mojo::PendingAssociatedRemote<mojom::GenerateBidClient>
          generate_bid_client,
      mojo::PendingAssociatedReceiver<mojom::GenerateBidFinalizer>
          bid_finalizer) override {
    ADD_FAILURE() << "GenerateBid shouldn't be called on DebugConnector";
  }

  void ReportWin(
      bool is_for_additional_bid,
      const std::optional<std::string>& interest_group_name_reporting_id,
      const std::optional<std::string>& buyer_reporting_id,
      const std::optional<std::string>& buyer_and_seller_reporting_id,
      const std::optional<std::string>& selected_buyer_and_seller_reporting_id,
      const std::optional<std::string>& auction_signals_json,
      const std::optional<std::string>& per_buyer_signals_json,
      const std::optional<GURL>& direct_from_seller_per_buyer_signals,
      const std::optional<std::string>&
          direct_from_seller_per_buyer_signals_header_ad_slot,
      const std::optional<GURL>& direct_from_seller_auction_signals,
      const std::optional<std::string>&
          direct_from_seller_auction_signals_header_ad_slot,
      const std::string& seller_signals_json,
      mojom::KAnonymityBidMode kanon_mode,
      bool bid_is_kanon,
      const GURL& browser_signal_render_url,
      double browser_signal_bid,
      const std::optional<blink::AdCurrency>& browser_signal_bid_currency,
      double browser_signal_highest_scoring_other_bid,
      const std::optional<blink::AdCurrency>&
          browser_signal_highest_scoring_other_bid_currency,
      bool browser_signal_made_highest_scoring_other_bid,
      std::optional<double> browser_signal_ad_cost,
      std::optional<uint16_t> browser_signal_modeling_signals,
      uint8_t browser_signal_join_count,
      uint8_t browser_signal_recency,
      const url::Origin& browser_signal_seller_origin,
      const std::optional<url::Origin>& browser_signal_top_level_seller_origin,
      const std::optional<base::TimeDelta> browser_signal_reporting_timeout,
      std::optional<uint32_t> bidding_data_version,
      uint64_t trace_id,
      ReportWinCallback report_win_callback) override {
    ADD_FAILURE() << "ReportWin shouldn't be called on DebugConnector";
  }

  void SendPendingSignalsRequests() override {
    ADD_FAILURE()
        << "SendPendingSignalsRequests shouldn't be called on DebugConnector";
  }

  void ConnectDevToolsAgent(mojo::PendingAssociatedReceiver<
                                blink::mojom::DevToolsAgent> agent_receiver,
                            uint32_t thread_index) override {
    auction_v8_helper_->ConnectDevToolsAgent(std::move(agent_receiver),
                                             mojo_thread_, *debug_id_);
  }

 private:
  DebugConnector(scoped_refptr<AuctionV8Helper> auction_v8_helper,
                 scoped_refptr<base::SequencedTaskRunner> mojo_thread,
                 scoped_refptr<AuctionV8Helper::DebugId> debug_id)
      : auction_v8_helper_(std::move(auction_v8_helper)),
        mojo_thread_(std::move(mojo_thread)),
        debug_id_(std::move(debug_id)) {}

  scoped_refptr<AuctionV8Helper> auction_v8_helper_;
  scoped_refptr<base::SequencedTaskRunner> mojo_thread_;
  scoped_refptr<AuctionV8Helper::DebugId> debug_id_;
};

class AuctionV8HelperTest : public testing::Test {
 public:
  explicit AuctionV8HelperTest(
      base::test::TaskEnvironment::TimeSource time_mode =
          base::test::TaskEnvironment::TimeSource::SYSTEM_TIME)
      : task_environment_(time_mode) {
    helper_ = AuctionV8Helper::Create(
        base::SingleThreadTaskRunner::GetCurrentDefault());
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
                std::optional<std::string> error_msg;
                ASSERT_TRUE(
                    helper->Compile(body, url, debug_id.get(), error_msg)
                        .ToLocal(&script));
                EXPECT_FALSE(error_msg.has_value());
              }
              v8::Local<v8::Context> context = helper->CreateContext();
              std::vector<std::string> error_msgs;
              v8::Context::Scope ctx(context);
              v8::Local<v8::Value> result;

              auto timeout =
                  helper->CreateTimeLimit(/*script_timeout=*/std::nullopt);
              bool success = helper->RunScript(context, script, debug_id.get(),
                                               timeout.get(), error_msgs) ==
                             AuctionV8Helper::Result::kSuccess;
              if (success) {
                // This is here since it needs to be before CallFunction() ---
                // doing it before Compile() doesn't work.
                helper->MaybeTriggerInstrumentationBreakpoint(*debug_id,
                                                              "start");
                helper->MaybeTriggerInstrumentationBreakpoint(*debug_id,
                                                              "start2");
                v8::MaybeLocal<v8::Value> maybe_result;
                if (helper->CallFunction(
                        context, debug_id.get(),
                        helper->FormatScriptName(script), function_name,
                        base::span<v8::Local<v8::Value>>(), timeout.get(),
                        maybe_result,
                        error_msgs) == AuctionV8Helper::Result::kSuccess) {
                  success = true;
                  result = maybe_result.ToLocalChecked();
                } else {
                  success = false;
                  EXPECT_TRUE(maybe_result.IsEmpty());
                }
              }
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

  bool CompileWasmOnV8ThreadAndWait(
      scoped_refptr<AuctionV8Helper::DebugId> debug_id,
      const GURL& url,
      const std::string& body,
      std::optional<std::string>* error_out) {
    bool success = false;
    base::RunLoop run_loop;
    helper_->v8_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](scoped_refptr<AuctionV8Helper> helper,
               scoped_refptr<AuctionV8Helper::DebugId> debug_id, GURL url,
               std::string body, bool* success_out,
               std::optional<std::string>* error_out, base::OnceClosure done) {
              AuctionV8Helper::FullIsolateScope isolate_scope(helper.get());
              v8::Context::Scope ctx(helper->scratch_context());
              *success_out =
                  !helper->CompileWasm(body, url, debug_id.get(), *error_out)
                       .IsEmpty();
              std::move(done).Run();
            },
            helper_, std::move(debug_id), url, body, &success, error_out,
            run_loop.QuitClosure()));
    run_loop.Run();
    return success;
  }

  mojo::Remote<auction_worklet::mojom::BidderWorklet> ConnectToDevToolsAgent(
      scoped_refptr<AuctionV8Helper::DebugId> debug_id,
      mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent>
          agent_receiver) {
    DCHECK(debug_id);
    mojo::Remote<auction_worklet::mojom::BidderWorklet> connector_pipe;

    helper_->v8_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&DebugConnector::Create, helper_,
                       base::SequencedTaskRunner::GetCurrentDefault(),
                       std::move(debug_id),
                       connector_pipe.BindNewPipeAndPassReceiver()));
    connector_pipe->ConnectDevToolsAgent(std::move(agent_receiver),
                                         /*thread_index=*/0);
    return connector_pipe;
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
    std::optional<std::string> error_msg;
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
    v8::MaybeLocal<v8::Value> maybe_result;
    ASSERT_TRUE(helper_->RunScript(context, script,
                                   /*debug_id=*/nullptr,
                                   /*script_timeout=*/nullptr, error_msgs) ==
                    AuctionV8Helper::Result::kSuccess &&
                helper_->CallFunction(context, /*debug_id=*/nullptr,
                                      helper_->FormatScriptName(script), "foo",
                                      base::span<v8::Local<v8::Value>>(),
                                      /*script_timeout=*/nullptr, maybe_result,
                                      error_msgs) ==
                    AuctionV8Helper::Result::kSuccess &&
                maybe_result.ToLocal(&result));
    int int_result = 0;
    ASSERT_TRUE(gin::ConvertFromV8(helper_->isolate(), result, &int_result));
    EXPECT_EQ(1, int_result);
    EXPECT_TRUE(error_msgs.empty());
  }
}

// Check that timing out scripts works.
TEST_F(AuctionV8HelperTest, Timeout) {
  struct Timeouts {
    std::optional<base::TimeDelta> script_timeout;
    base::TimeDelta default_timeout;
    bool test_default_timeout;
  };

  const Timeouts kTimeouts[] = {
      // Test default timeout. Use a shorter default timeout so test runs
      // faster.
      {std::nullopt, base::Milliseconds(20), true},

      // Test `script_timeout` parameter of AuctionV8Helper::RunScript(). Use a
      // very long default timeout, so that we know the parameter worked if the
      // script timed out.
      {base::Milliseconds(20), base::Days(100), false}};

  for (const Timeouts& timeout : kTimeouts) {
    helper_->set_script_timeout_for_testing(timeout.default_timeout);
    base::TimeDelta time_passed = timeout.test_default_timeout
                                      ? timeout.default_timeout
                                      : timeout.script_timeout.value();

    // Test top-level hang
    {
      base::TimeTicks start_time = base::TimeTicks::Now();
      v8::Local<v8::Context> context = helper_->CreateContext();
      v8::Context::Scope context_scope(context);

      v8::Local<v8::UnboundScript> script;
      std::optional<std::string> compile_error;
      ASSERT_TRUE(helper_
                      ->Compile(R"(
                        function foo() { return 1;}
                        while(1);)",
                                GURL("https://foo.test/"),
                                /*debug_id=*/nullptr, compile_error)
                      .ToLocal(&script));
      EXPECT_EQ(compile_error, std::nullopt);

      std::vector<std::string> error_msgs;
      auto time_limit = helper_->CreateTimeLimit(timeout.script_timeout);
      EXPECT_EQ(helper_->RunScript(context, script,
                                   /*debug_id=*/nullptr, time_limit.get(),
                                   error_msgs),
                AuctionV8Helper::Result::kTimeout);
      EXPECT_THAT(
          error_msgs,
          ElementsAre("https://foo.test/ top-level execution timed out."));

      // Make sure at least `time_passed` has passed, allowing for some time
      // skew between change in base::TimeTicks::Now() and the timeout. This
      // mostly serves to make sure the script timed out, instead of immediately
      // terminating.
      EXPECT_GE(base::TimeTicks::Now() - start_time,
                time_passed - base::Milliseconds(10));
    }

    // function hangs
    {
      base::TimeTicks start_time = base::TimeTicks::Now();
      v8::Local<v8::Context> context = helper_->CreateContext();
      v8::Context::Scope context_scope(context);

      v8::Local<v8::UnboundScript> script;
      std::optional<std::string> compile_error;
      ASSERT_TRUE(helper_
                      ->Compile(R"(
                        function foo() {while (1);}
                        )",
                                GURL("https://foo.test/"),
                                /*debug_id=*/nullptr, compile_error)
                      .ToLocal(&script));
      EXPECT_EQ(compile_error, std::nullopt);

      std::vector<std::string> error_msgs;
      auto time_limit = helper_->CreateTimeLimit(timeout.script_timeout);
      EXPECT_EQ(helper_->RunScript(context, script,
                                   /*debug_id=*/nullptr, time_limit.get(),
                                   error_msgs),
                AuctionV8Helper::Result::kSuccess);

      v8::MaybeLocal<v8::Value> result;
      EXPECT_EQ(AuctionV8Helper::Result::kTimeout,
                helper_->CallFunction(context, /*debug_id=*/nullptr,
                                      helper_->FormatScriptName(script), "foo",
                                      base::span<v8::Local<v8::Value>>(),
                                      time_limit.get(), result, error_msgs));
      EXPECT_TRUE(result.IsEmpty());
      EXPECT_THAT(
          error_msgs,
          ElementsAre("https://foo.test/ execution of `foo` timed out."));

      // Make sure at least `time_passed` has passed, allowing for some time
      // skew between change in base::TimeTicks::Now() and the timeout. This
      // mostly serves to make sure the script timed out, instead of immediately
      // terminating.
      EXPECT_GE(base::TimeTicks::Now() - start_time,
                time_passed - base::Milliseconds(10));
    }
  }
  // Make sure it's still possible to run a script with the isolate after the
  // timeouts.
  v8::Local<v8::Context> context = helper_->CreateContext();
  v8::Context::Scope context_scope(context);
  v8::Local<v8::UnboundScript> script;
  std::optional<std::string> compile_error;
  ASSERT_TRUE(helper_
                  ->Compile("function foo() { return 1;}",
                            GURL("https://foo.test/"),
                            /*debug_id=*/nullptr, compile_error)
                  .ToLocal(&script));
  EXPECT_EQ(compile_error, std::nullopt);

  std::vector<std::string> error_msgs;
  v8::Local<v8::Value> result;
  v8::MaybeLocal<v8::Value> maybe_result;
  ASSERT_EQ(helper_->RunScript(context, script,
                               /*debug_id=*/nullptr,
                               /*script_timeout=*/nullptr, error_msgs),
            AuctionV8Helper::Result::kSuccess);
  ASSERT_EQ(helper_->CallFunction(context, /*debug_id=*/nullptr,
                                  helper_->FormatScriptName(script), "foo",
                                  base::span<v8::Local<v8::Value>>(),
                                  /*script_timeout=*/nullptr, maybe_result,
                                  error_msgs),
            AuctionV8Helper::Result::kSuccess);
  ASSERT_TRUE(maybe_result.ToLocal(&result));
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
  std::optional<std::string> compile_error;
  ASSERT_TRUE(helper_
                  ->Compile("function foo() { return Date();}",
                            GURL("https://foo.test/"),
                            /*debug_id=*/nullptr, compile_error)
                  .ToLocal(&script));
  EXPECT_FALSE(compile_error.has_value());
  std::vector<std::string> error_msgs;
  v8::MaybeLocal<v8::Value> maybe_result;
  ASSERT_EQ(helper_->RunScript(context, script,
                               /*debug_id=*/nullptr,
                               /*script_timeout=*/nullptr, error_msgs),
            AuctionV8Helper::Result::kSuccess);
  ASSERT_EQ(helper_->CallFunction(context, /*debug_id=*/nullptr,
                                  helper_->FormatScriptName(script), "foo",
                                  base::span<v8::Local<v8::Value>>(),
                                  /*script_timeout=*/nullptr, maybe_result,
                                  error_msgs),
            AuctionV8Helper::Result::kFailure);
  EXPECT_TRUE(maybe_result.IsEmpty());
  ASSERT_EQ(1u, error_msgs.size());
  EXPECT_THAT(error_msgs[0], StartsWith("https://foo.test/:1"));
  EXPECT_THAT(error_msgs[0], HasSubstr("ReferenceError"));
  EXPECT_THAT(error_msgs[0], HasSubstr("Date"));
}

// A script that doesn't compile.
TEST_F(AuctionV8HelperTest, CompileError) {
  v8::Local<v8::UnboundScript> script;
  v8::Context::Scope ctx(helper_->scratch_context());
  std::optional<std::string> error_msg;
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
    std::optional<std::string> error_msg;
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
  EXPECT_EQ(helper_->RunScript(context, script,
                               /*debug_id=*/nullptr,
                               /*script_timeout=*/nullptr, error_msgs),
            AuctionV8Helper::Result::kFailure);
  EXPECT_THAT(
      error_msgs,
      ElementsAre("https://foo.test/:3 Uncaught Error: I am an error."));
}

// Test for when desired function isn't found
TEST_F(AuctionV8HelperTest, TargetFunctionNotFound) {
  v8::Local<v8::UnboundScript> script;
  {
    v8::Context::Scope ctx(helper_->scratch_context());
    std::optional<std::string> error_msg;
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
  v8::MaybeLocal<v8::Value> maybe_result;
  v8::Local<v8::Value> result;
  ASSERT_EQ(helper_->RunScript(context, script,
                               /*debug_id=*/nullptr,
                               /*script_timeout=*/nullptr, error_msgs),
            AuctionV8Helper::Result::kSuccess);
  ASSERT_EQ(helper_->CallFunction(context, /*debug_id=*/nullptr,
                                  helper_->FormatScriptName(script), "bar",
                                  base::span<v8::Local<v8::Value>>(),
                                  /*script_timeout=*/nullptr, maybe_result,
                                  error_msgs),
            AuctionV8Helper::Result::kFailure);
  ASSERT_FALSE(maybe_result.ToLocal(&result));

  // This "not a function" and not "not found" since the lookup successfully
  // returns `undefined`.
  EXPECT_THAT(error_msgs,
              ElementsAre("https://foo.test/ `bar` is not a function."));
}

TEST_F(AuctionV8HelperTest, TargetFunctionError) {
  v8::Local<v8::UnboundScript> script;
  {
    v8::Context::Scope ctx(helper_->scratch_context());
    std::optional<std::string> error_msg;
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
  ASSERT_EQ(helper_->RunScript(context, script,
                               /*debug_id=*/nullptr,
                               /*script_timeout=*/nullptr, error_msgs),
            AuctionV8Helper::Result::kSuccess);
  v8::MaybeLocal<v8::Value> maybe_result;
  ASSERT_EQ(helper_->CallFunction(context, /*debug_id=*/nullptr,
                                  helper_->FormatScriptName(script), "foo",
                                  base::span<v8::Local<v8::Value>>(),
                                  /*script_timeout=*/nullptr, maybe_result,
                                  error_msgs),
            AuctionV8Helper::Result::kFailure);
  ASSERT_FALSE(maybe_result.ToLocal(&result));
  ASSERT_EQ(1u, error_msgs.size());

  EXPECT_THAT(error_msgs[0], StartsWith("https://foo.test/:1 "));
  EXPECT_THAT(error_msgs[0], HasSubstr("ReferenceError"));
  EXPECT_THAT(error_msgs[0], HasSubstr("notfound"));
}

TEST_F(AuctionV8HelperTest, ConsoleLog) {
  // Console log output is reported by V8 via debugging channels.
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

  const char kScript[] = R"(
    console.debug('debug is there');

    function foo() {
      console.log('can', 'log', 'multiple', 'things', true);
      console.table('even table!');
    }
  )";

  base::RunLoop run_loop;
  CompileAndRunScriptOnV8Thread(id, "foo", GURL("https://foo.test/"), kScript,
                                /*expect_success=*/true,
                                run_loop.QuitClosure());
  run_loop.Run();

  {
    TestChannel::Event message =
        channel->WaitForMethodNotification("Runtime.consoleAPICalled");
    const std::string* type =
        message.value.GetDict().FindStringByDottedPath("params.type");
    ASSERT_TRUE(type);
    EXPECT_EQ("debug", *type);
    const base::Value::List* args =
        message.value.GetDict().FindListByDottedPath("params.args");
    ASSERT_TRUE(args);
    ASSERT_EQ(1u, args->size());
    const base::Value::Dict* args_dict = (*args)[0].GetIfDict();
    ASSERT_TRUE(args_dict);
    EXPECT_EQ("string", *args_dict->FindString("type"));
    EXPECT_EQ("debug is there", *args_dict->FindString("value"));
    const base::Value::List* stack_trace =
        message.value.GetDict().FindListByDottedPath(
            "params.stackTrace.callFrames");
    ASSERT_TRUE(stack_trace);
    ASSERT_EQ(1u, stack_trace->size());
    const base::Value::Dict* trace_dict = (*stack_trace)[0].GetIfDict();
    ASSERT_TRUE(trace_dict);
    EXPECT_EQ("", *trace_dict->FindString("functionName"));
    EXPECT_EQ("https://foo.test/", *trace_dict->FindString("url"));
    EXPECT_EQ(1, trace_dict->FindInt("lineNumber"));
  }

  {
    TestChannel::Event message =
        channel->WaitForMethodNotification("Runtime.consoleAPICalled");
    const std::string* type =
        message.value.GetDict().FindStringByDottedPath("params.type");
    ASSERT_TRUE(type);
    EXPECT_EQ("log", *type);
    const base::Value::List* args =
        message.value.GetDict().FindListByDottedPath("params.args");
    ASSERT_TRUE(args);
    ASSERT_EQ(5u, args->size());
    EXPECT_EQ("string", *(*args)[0].GetDict().FindString("type"));
    EXPECT_EQ("can", *(*args)[0].GetDict().FindString("value"));
    EXPECT_EQ("string", *(*args)[1].GetDict().FindString("type"));
    EXPECT_EQ("log", *(*args)[1].GetDict().FindString("value"));
    EXPECT_EQ("string", *(*args)[2].GetDict().FindString("type"));
    EXPECT_EQ("multiple", *(*args)[2].GetDict().FindString("value"));
    EXPECT_EQ("string", *(*args)[3].GetDict().FindString("type"));
    EXPECT_EQ("things", *(*args)[3].GetDict().FindString("value"));
    EXPECT_EQ("boolean", *(*args)[4].GetDict().FindString("type"));
    EXPECT_EQ(true, (*args)[4].GetDict().FindBool("value"));

    const base::Value::List* stack_trace =
        message.value.GetDict().FindListByDottedPath(
            "params.stackTrace.callFrames");
    ASSERT_TRUE(stack_trace);
    ASSERT_EQ(1u, stack_trace->size());
    const base::Value::Dict* stack_trace_dict = (*stack_trace)[0].GetIfDict();
    ASSERT_TRUE(stack_trace_dict);
    EXPECT_EQ("foo", *stack_trace_dict->FindString("functionName"));
    EXPECT_EQ("https://foo.test/", *stack_trace_dict->FindString("url"));
    EXPECT_EQ(4, stack_trace_dict->FindInt("lineNumber"));
  }

  {
    TestChannel::Event message =
        channel->WaitForMethodNotification("Runtime.consoleAPICalled");
    const std::string* type =
        message.value.GetDict().FindStringByDottedPath("params.type");
    ASSERT_TRUE(type);
    EXPECT_EQ("table", *type);
    const base::Value::List* args =
        message.value.GetDict().FindListByDottedPath("params.args");
    ASSERT_TRUE(args);
    ASSERT_EQ(1u, args->size());
    const base::Value::Dict* args_dict = (*args)[0].GetIfDict();
    ASSERT_TRUE(args_dict);
    EXPECT_EQ("string", *args_dict->FindString("type"));
    EXPECT_EQ("even table!", *args_dict->FindString("value"));
    const base::Value::List* stack_trace =
        message.value.GetDict().FindListByDottedPath(
            "params.stackTrace.callFrames");
    ASSERT_TRUE(stack_trace);
    ASSERT_EQ(1u, stack_trace->size());
    const base::Value::Dict* stack_trace_dict = (*stack_trace)[0].GetIfDict();
    ASSERT_TRUE(stack_trace_dict);
    EXPECT_EQ("foo", *stack_trace_dict->FindString("functionName"));
    EXPECT_EQ("https://foo.test/", *stack_trace_dict->FindString("url"));
    EXPECT_EQ(5, stack_trace_dict->FindInt("lineNumber"));
  }

  id->AbortDebuggerPauses();
}

TEST_F(AuctionV8HelperTest, FormatScriptName) {
  v8::Local<v8::UnboundScript> script;
  v8::Context::Scope ctx(helper_->scratch_context());
  std::optional<std::string> error_msg;
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
      context_created_event.value.GetDict().FindStringByDottedPath(
          "params.context.name");
  ASSERT_TRUE(name);
  EXPECT_EQ(kURL, *name);

  TestChannel::Event context_destroyed_event =
      channel->WaitForMethodNotification("Runtime.executionContextDestroyed");

  TestChannel::Event context_created2_event =
      channel->WaitForMethodNotification("Runtime.executionContextCreated");
  const std::string* name2 =
      context_created2_event.value.GetDict().FindStringByDottedPath(
          "params.context.name");
  ASSERT_TRUE(name2);
  EXPECT_EQ(kURL, *name2);

  TestChannel::Event script_parsed_event =
      channel->WaitForMethodNotification("Debugger.scriptParsed");
  const std::string* url =
      script_parsed_event.value.GetDict().FindStringByDottedPath("params.url");
  ASSERT_TRUE(url);
  EXPECT_EQ(kURL, *url);
  const std::string* script_id =
      script_parsed_event.value.GetDict().FindStringByDottedPath(
          "params.scriptId");
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
      source_response.value.GetDict().FindStringByDottedPath(
          "result.scriptSource");
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
              std::optional<std::string> error_msg;
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

    mojo::AssociatedRemote<blink::mojom::DevToolsAgent> agent_remote;
    auto connector = ConnectToDevToolsAgent(
        id, agent_remote.BindNewEndpointAndPassReceiver());

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

    TestDevToolsAgentClient::Event script_parsed =
        debug_client.WaitForMethodNotification("Debugger.scriptParsed");
    const std::string* url =
        script_parsed.value.GetDict().FindStringByDottedPath("params.url");
    ASSERT_TRUE(url);
    EXPECT_EQ(*url, "https://example.com/test.js");

    // Wait for breakpoint to hit.
    TestDevToolsAgentClient::Event breakpoint_hit =
        debug_client.WaitForMethodNotification("Debugger.paused");

    const base::Value::List* hit_breakpoints =
        breakpoint_hit.value.GetDict().FindListByDottedPath(
            "params.hitBreakpoints");
    ASSERT_TRUE(hit_breakpoints);
    ASSERT_EQ(1u, hit_breakpoints->size());
    ASSERT_TRUE((*hit_breakpoints)[0].is_string());
    EXPECT_EQ("1:2:0:https://example.com/test.js",
              (*hit_breakpoints)[0].GetString());
    std::string* callframe_id = breakpoint_hit.value.GetDict()
                                    .FindDict("params")
                                    ->FindList("callFrames")
                                    ->front()
                                    .GetDict()
                                    .FindString("callFrameId");

    const char kCommandTemplate[] = R"({
      "id": 4,
      "method": "Debugger.evaluateOnCallFrame",
      "params": {
        "callFrameId": "%s",
        "expression": "multiplier = 10"
      }
    })";

    // Change the state before resuming.
    // Post-breakpoint params must be run on IO pipe, any main thread commands
    // won't do things yet.
    debug_client.RunCommandAndWaitForResult(
        TestDevToolsAgentClient::Channel::kIO, 4,
        "Debugger.evaluateOnCallFrame",
        base::StringPrintf(kCommandTemplate, callframe_id->c_str()));

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

      mojo::AssociatedRemote<blink::mojom::DevToolsAgent> agent_remote;
      auto connector = ConnectToDevToolsAgent(
          id, agent_remote.BindNewEndpointAndPassReceiver());

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
            breakpoint_hit.value.GetDict().FindStringByDottedPath(
                "params.reason");
        ASSERT_TRUE(reason);
        EXPECT_EQ("ambiguous", *reason);

        const base::Value::List* reasons =
            breakpoint_hit.value.GetDict().FindListByDottedPath(
                "params.data.reasons");
        ASSERT_TRUE(reasons);
        ASSERT_EQ(2u, reasons->size());
        ASSERT_TRUE((*reasons)[0].is_dict());
        ASSERT_TRUE((*reasons)[1].is_dict());
        const std::string* ev1 =
            (*reasons)[0].GetDict().FindStringByDottedPath("auxData.eventName");
        const std::string* ev2 =
            (*reasons)[1].GetDict().FindStringByDottedPath("auxData.eventName");
        const std::string* r1 =
            (*reasons)[0].GetDict().FindStringByDottedPath("reason");
        const std::string* r2 =
            (*reasons)[1].GetDict().FindStringByDottedPath("reason");
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
        EXPECT_FALSE(breakpoint_hit.value.GetDict().FindStringByDottedPath(
            "params.data.reasons"));
        const std::string* reason =
            breakpoint_hit.value.GetDict().FindStringByDottedPath(
                "params.reason");
        ASSERT_TRUE(reason);
        EXPECT_EQ("EventListener", *reason);

        const std::string* event_name =
            breakpoint_hit.value.GetDict().FindStringByDottedPath(
                "params.data.eventName");
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

    mojo::AssociatedRemote<blink::mojom::DevToolsAgent> agent_remote;
    auto connector = ConnectToDevToolsAgent(
        id, agent_remote.BindNewEndpointAndPassReceiver());

    TestDevToolsAgentClient debug_client(std::move(agent_remote), kSession,
                                         use_binary_protocol);
    TestDevToolsAgentClient::Event result =
        debug_client.RunCommandAndWaitForResult(
            TestDevToolsAgentClient::Channel::kMain, 1, "NoSuchThing.enable",
            R"({"id":1,"method":"NoSuchThing.enable","params":{}})");
    ASSERT_TRUE(result.value.is_dict());
    EXPECT_TRUE(result.value.GetDict().FindDict("error"));

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

  mojo::AssociatedRemote<blink::mojom::DevToolsAgent> agent_remote;
  auto connector =
      ConnectToDevToolsAgent(id, agent_remote.BindNewEndpointAndPassReceiver());

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
  mojo::AssociatedRemote<blink::mojom::DevToolsAgent> agent_remote;
  auto connector =
      ConnectToDevToolsAgent(id, agent_remote.BindNewEndpointAndPassReceiver());

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
  mojo::AssociatedRemote<blink::mojom::DevToolsAgent> agent_remote;
  auto connector =
      ConnectToDevToolsAgent(id, agent_remote.BindNewEndpointAndPassReceiver());

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

TEST_F(AuctionV8HelperTest, CompileWasm) {
  v8::Local<v8::Context> context = helper_->CreateContext();
  v8::Context::Scope context_scope(context);

  v8::Local<v8::WasmModuleObject> wasm_module;
  std::optional<std::string> compile_error;
  ASSERT_TRUE(helper_
                  ->CompileWasm(std::string(kMinimalWasmModuleBytes,
                                            std::size(kMinimalWasmModuleBytes)),
                                GURL("https://foo.test/"),
                                /*debug_id=*/nullptr, compile_error)
                  .ToLocal(&wasm_module));
  EXPECT_FALSE(compile_error.has_value());
}

TEST_F(AuctionV8HelperTest, CompileWasmError) {
  v8::Local<v8::Context> context = helper_->CreateContext();
  v8::Context::Scope context_scope(context);

  v8::Local<v8::WasmModuleObject> wasm_module;
  std::optional<std::string> compile_error;
  EXPECT_FALSE(helper_
                   ->CompileWasm("not wasm", GURL("https://foo.test/"),
                                 /*debug_id=*/nullptr, compile_error)
                   .ToLocal(&wasm_module));
  ASSERT_TRUE(compile_error.has_value());
  EXPECT_THAT(compile_error.value(), StartsWith("https://foo.test/ "));
  EXPECT_THAT(compile_error.value(),
              HasSubstr("Uncaught CompileError: WasmModuleObject::Compile"));
}

TEST_F(AuctionV8HelperTest, CompileWasmDebug) {
  const char kSession[] = "123-456";
  // Need to use a separate thread for debugger stuff.
  v8_scope_.reset();
  helper_ = AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner());

  auto id = base::MakeRefCounted<AuctionV8Helper::DebugId>(helper_.get());

  mojo::AssociatedRemote<blink::mojom::DevToolsAgent> agent_remote;
  auto connector =
      ConnectToDevToolsAgent(id, agent_remote.BindNewEndpointAndPassReceiver());

  TestDevToolsAgentClient debug_client(std::move(agent_remote), kSession,
                                       /*use_binary_protocol=*/false);

  debug_client.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 1, "Runtime.enable",
      R"({"id":1,"method":"Runtime.enable","params":{}})");
  debug_client.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 2, "Debugger.enable",
      R"({"id":2,"method":"Debugger.enable","params":{}})");

  std::optional<std::string> error_out;
  EXPECT_TRUE(CompileWasmOnV8ThreadAndWait(
      id, GURL("https://example.com"),
      std::string(kMinimalWasmModuleBytes, std::size(kMinimalWasmModuleBytes)),
      &error_out));
  TestDevToolsAgentClient::Event script_parsed =
      debug_client.WaitForMethodNotification("Debugger.scriptParsed");
  const std::string* lang =
      script_parsed.value.GetDict().FindStringByDottedPath(
          "params.scriptLanguage");
  ASSERT_TRUE(lang);
  EXPECT_EQ(*lang, "WebAssembly");

  debug_client.WaitForMethodNotification("Runtime.executionContextDestroyed");
  id->AbortDebuggerPauses();
}

TEST_F(AuctionV8HelperTest, CloneWasmModule) {
  // Test proper CloneWasmModule() usage to prevent state persistence via
  // WASM Module objects.

  const char kScript[] = R"(
    function probe(moduleObject) {
      var result = moduleObject.weirdField ? moduleObject.weirdField : -1;
      moduleObject.weirdField = 5;
      return result;
    }
  )";

  v8::Local<v8::Context> context = helper_->CreateContext();
  v8::Context::Scope context_scope(context);

  // Compile the WASM module...
  v8::Local<v8::WasmModuleObject> wasm_module;
  std::optional<std::string> error_msg;
  ASSERT_TRUE(helper_
                  ->CompileWasm(std::string(kMinimalWasmModuleBytes,
                                            std::size(kMinimalWasmModuleBytes)),
                                GURL("https://foo.test/"),
                                /*debug_id=*/nullptr, error_msg)
                  .ToLocal(&wasm_module));
  EXPECT_FALSE(error_msg.has_value());

  // And the test script.
  v8::Local<v8::UnboundScript> script;
  ASSERT_TRUE(helper_
                  ->Compile(kScript, GURL("https://foo.test/"),
                            /*debug_id=*/nullptr, error_msg)
                  .ToLocal(&script));
  EXPECT_FALSE(error_msg.has_value());

  // Run the script a couple of times passing in the same module.
  v8::LocalVector<v8::Value> args(helper_->isolate());
  args.push_back(wasm_module);
  v8::Local<v8::Value> result;
  v8::MaybeLocal<v8::Value> maybe_result;
  std::vector<std::string> error_msgs;
  ASSERT_EQ(helper_->RunScript(context, script,
                               /*debug_id=*/nullptr,
                               /*script_timeout=*/nullptr, error_msgs),
            AuctionV8Helper::Result::kSuccess);
  ASSERT_EQ(helper_->CallFunction(
                context, /*debug_id=*/nullptr,
                helper_->FormatScriptName(script), "probe", args,
                /*script_timeout=*/nullptr, maybe_result, error_msgs),
            AuctionV8Helper::Result::kSuccess);
  ASSERT_TRUE(maybe_result.ToLocal(&result));
  EXPECT_TRUE(error_msgs.empty());
  int int_result = 0;
  ASSERT_TRUE(gin::ConvertFromV8(helper_->isolate(), result, &int_result));
  EXPECT_EQ(-1, int_result);

  v8::MaybeLocal<v8::Value> maybe_result2;
  ASSERT_EQ(helper_->RunScript(context, script,
                               /*debug_id=*/nullptr,
                               /*script_timeout=*/nullptr, error_msgs),
            AuctionV8Helper::Result::kSuccess);
  ASSERT_EQ(helper_->CallFunction(
                context, /*debug_id=*/nullptr,
                helper_->FormatScriptName(script), "probe", args,
                /*script_timeout=*/nullptr, maybe_result2, error_msgs),
            AuctionV8Helper::Result::kSuccess);
  ASSERT_TRUE(maybe_result2.ToLocal(&result));
  EXPECT_TRUE(error_msgs.empty());
  ASSERT_TRUE(gin::ConvertFromV8(helper_->isolate(), result, &int_result));
  EXPECT_EQ(5, int_result);

  // Nothing stick arounds if CloneWasmModule is consistently used, however.
  args[0] = helper_->CloneWasmModule(wasm_module).ToLocalChecked();
  v8::MaybeLocal<v8::Value> maybe_result3;
  ASSERT_EQ(helper_->RunScript(context, script,
                               /*debug_id=*/nullptr,
                               /*script_timeout=*/nullptr, error_msgs),
            AuctionV8Helper::Result::kSuccess);
  ASSERT_EQ(helper_->CallFunction(
                context, /*debug_id=*/nullptr,
                helper_->FormatScriptName(script), "probe", args,
                /*script_timeout=*/nullptr, maybe_result3, error_msgs),
            AuctionV8Helper::Result::kSuccess);
  ASSERT_TRUE(maybe_result3.ToLocal(&result));
  EXPECT_TRUE(error_msgs.empty());
  ASSERT_TRUE(gin::ConvertFromV8(helper_->isolate(), result, &int_result));
  EXPECT_EQ(-1, int_result);

  args[0] = helper_->CloneWasmModule(wasm_module).ToLocalChecked();
  v8::MaybeLocal<v8::Value> maybe_result4;
  ASSERT_EQ(helper_->RunScript(context, script,
                               /*debug_id=*/nullptr,
                               /*script_timeout=*/nullptr, error_msgs),
            AuctionV8Helper::Result::kSuccess);
  ASSERT_EQ(helper_->CallFunction(
                context, /*debug_id=*/nullptr,
                helper_->FormatScriptName(script), "probe", args,
                /*script_timeout=*/nullptr, maybe_result4, error_msgs),
            AuctionV8Helper::Result::kSuccess);
  ASSERT_TRUE(maybe_result4.ToLocal(&result));
  EXPECT_TRUE(error_msgs.empty());
  ASSERT_TRUE(gin::ConvertFromV8(helper_->isolate(), result, &int_result));
  EXPECT_EQ(-1, int_result);
}

TEST_F(AuctionV8HelperTest, SerializeDeserialize) {
  {
    v8::Local<v8::Context> context = helper_->CreateContext();
    v8::Context::Scope context_scope(context);

    v8::MaybeLocal<v8::Value> result =
        helper_->Deserialize(context, AuctionV8Helper::SerializedValue());
    EXPECT_TRUE(result.IsEmpty());
  }

  {
    v8::MaybeLocal<v8::Value> in = helper_->CreateValueFromJson(
        helper_->scratch_context(),
        R"({"a": false, "b": 42, "c": {"d": [1,2,3] } })");
    AuctionV8Helper::SerializedValue serialized =
        helper_->Serialize(helper_->scratch_context(), in.ToLocalChecked());
    EXPECT_TRUE(serialized.IsOK());

    // Decode in a different context from scratch_context... Multiple times.
    for (int run = 0; run < 3; ++run) {
      v8::Local<v8::Context> context = helper_->CreateContext();
      v8::Context::Scope context_scope(context);
      v8::MaybeLocal<v8::Value> deserialized =
          helper_->Deserialize(context, serialized);
      ASSERT_FALSE(deserialized.IsEmpty());
      std::string deserialized_as_json;
      ASSERT_EQ(helper_->ExtractJson(context, deserialized.ToLocalChecked(),
                                     /*script_timeout=*/nullptr,
                                     &deserialized_as_json),
                AuctionV8Helper::Result::kSuccess);
      EXPECT_EQ(R"({"a":false,"b":42,"c":{"d":[1,2,3]}})",
                deserialized_as_json);
    }
  }
}

TEST_F(AuctionV8HelperTest, ExtractJsonTimeout) {
  // Test both with and without a TimeLimitScope already created; to make sure
  // that AuctionV8Helper::ExtractJson makes one.
  for (bool have_external_time_scope : {false, true}) {
    // While it's tempting to use a shorter timeout since this is a
    // non-termination test, that flakes occasionally, and even more so under
    // *SAN, for which the default is auto-adjusted.
    auto time_limit = helper_->CreateTimeLimit(/*script_timeout=*/std::nullopt);

    SCOPED_TRACE(have_external_time_scope);
    std::unique_ptr<AuctionV8Helper::TimeLimitScope> time_limit_scope;
    if (have_external_time_scope) {
      time_limit_scope =
          std::make_unique<AuctionV8Helper::TimeLimitScope>(time_limit.get());
    }

    const char kScript[] = R"(
    function make() {
      return {
        get field() { while(true); }
      }
    }
  )";

    {
      v8::Local<v8::Context> context = helper_->CreateContext();
      v8::Context::Scope context_scope(context);

      v8::Local<v8::UnboundScript> script;
      std::optional<std::string> compile_error;
      ASSERT_TRUE(helper_
                      ->Compile(kScript, GURL("https://foo.test/"),
                                /*debug_id=*/nullptr, compile_error)
                      .ToLocal(&script));
      EXPECT_EQ(compile_error, std::nullopt);

      std::vector<std::string> error_msgs;
      v8::Local<v8::Value> result;
      v8::MaybeLocal<v8::Value> maybe_result;
      ASSERT_TRUE(helper_->RunScript(context, script,
                                     /*debug_id=*/nullptr,
                                     /*script_timeout=*/nullptr, error_msgs) ==
                      AuctionV8Helper::Result::kSuccess &&
                  helper_->CallFunction(
                      context, /*debug_id=*/nullptr,
                      helper_->FormatScriptName(script), "make",
                      base::span<v8::Local<v8::Value>>(),
                      /*script_timeout=*/nullptr, maybe_result,
                      error_msgs) == AuctionV8Helper::Result::kSuccess &&
                  maybe_result.ToLocal(&result));
      EXPECT_TRUE(error_msgs.empty());

      std::string deserialized_as_json;
      ASSERT_EQ(helper_->ExtractJson(context, result, time_limit.get(),
                                     &deserialized_as_json),
                AuctionV8Helper::Result::kTimeout);
    }
  }
}

}  // namespace auction_worklet
