// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/context_recycler.h"

#include <stdint.h>

#include <limits>
#include <string>
#include <vector>

#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "content/common/aggregatable_report.mojom-shared.h"
#include "content/common/aggregatable_report.mojom.h"
#include "content/common/private_aggregation_features.h"
#include "content/common/private_aggregation_host.mojom-forward.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/bidder_lazy_filler.h"
#include "content/services/auction_worklet/for_debugging_only_bindings.h"
#include "content/services/auction_worklet/private_aggregation_bindings.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "content/services/auction_worklet/register_ad_beacon_bindings.h"
#include "content/services/auction_worklet/report_bindings.h"
#include "content/services/auction_worklet/set_bid_bindings.h"
#include "content/services/auction_worklet/set_priority_bindings.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-primitive.h"

using testing::ElementsAre;
using testing::Pair;

namespace auction_worklet {

class ContextRecyclerTest : public testing::Test {
 public:
  ContextRecyclerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        helper_(AuctionV8Helper::Create(
            base::SingleThreadTaskRunner::GetCurrentDefault())) {
    // Here since we're using the same thread for everything, we need to spin
    // the event loop to let AuctionV8Helper finish initializing "off-thread";
    // normally PostTask semantics will ensure that anything that uses it on its
    // thread would happen after such initialization.
    base::RunLoop().RunUntilIdle();
    v8_scope_ =
        std::make_unique<AuctionV8Helper::FullIsolateScope>(helper_.get());
  }
  ~ContextRecyclerTest() override = default;

  v8::Local<v8::UnboundScript> Compile(const std::string& code) {
    v8::Local<v8::UnboundScript> script;
    v8::Context::Scope ctx(helper_->scratch_context());
    absl::optional<std::string> error_msg;
    EXPECT_TRUE(helper_
                    ->Compile(code, GURL("https://example.org/script.js"),
                              /*debug_id=*/nullptr, error_msg)
                    .ToLocal(&script));
    EXPECT_FALSE(error_msg.has_value()) << error_msg.value();
    return script;
  }

  // Runs a function with zero or 1 arguments.
  v8::MaybeLocal<v8::Value> Run(
      ContextRecyclerScope& scope,
      v8::Local<v8::UnboundScript> script,
      const std::string& function_name,
      std::vector<std::string>& error_msgs,
      v8::Local<v8::Value> maybe_arg = v8::Local<v8::Value>()) {
    std::vector<v8::Local<v8::Value>> args;
    if (!maybe_arg.IsEmpty())
      args.push_back(maybe_arg);
    return helper_->RunScript(scope.GetContext(), script,
                              /*debug_id=*/nullptr,
                              AuctionV8Helper::ExecMode::kTopLevelAndFunction,
                              function_name, args,
                              /*script_timeout=*/absl::nullopt, error_msgs);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<AuctionV8Helper> helper_;
  std::unique_ptr<AuctionV8Helper::FullIsolateScope> v8_scope_;
};

// Test with no binding objects, just context creation.
TEST_F(ContextRecyclerTest, Basic) {
  v8::Local<v8::UnboundScript> script = Compile("function test() { return 1;}");
  ASSERT_FALSE(script.IsEmpty());

  ContextRecycler context_recycler(helper_.get());
  ContextRecyclerScope scope(context_recycler);

  std::vector<std::string> error_msgs;
  v8::MaybeLocal<v8::Value> maybe_result =
      Run(scope, script, "test", error_msgs);
  v8::Local<v8::Value> result;
  ASSERT_TRUE(maybe_result.ToLocal(&result));
  int int_result = 0;
  ASSERT_TRUE(gin::ConvertFromV8(helper_->isolate(), result, &int_result));
  EXPECT_EQ(1, int_result);
  EXPECT_THAT(error_msgs, ElementsAre());
}

// Exercise ForDebuggingOnlyBindings, and make sure they reset properly.
TEST_F(ContextRecyclerTest, ForDebuggingOnlyBindings) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      blink::features::kBiddingAndScoringDebugReportingAPI);

  const char kScript[] = R"(
    function test(suffix) {
      forDebuggingOnly.reportAdAuctionLoss('https://example.com/loss' + suffix);
      forDebuggingOnly.reportAdAuctionWin('https://example.com/win' + suffix);
    }
  )";

  v8::Local<v8::UnboundScript> script = Compile(kScript);
  ASSERT_FALSE(script.IsEmpty());

  ContextRecycler context_recycler(helper_.get());
  context_recycler.AddForDebuggingOnlyBindings();

  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;
    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), 1));
    EXPECT_THAT(error_msgs, ElementsAre());
    EXPECT_EQ(
        GURL("https://example.com/loss1"),
        context_recycler.for_debugging_only_bindings()->TakeLossReportUrl());
    EXPECT_EQ(
        GURL("https://example.com/win1"),
        context_recycler.for_debugging_only_bindings()->TakeWinReportUrl());
  }

  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;
    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), 3));
    EXPECT_THAT(error_msgs, ElementsAre());
    EXPECT_EQ(
        GURL("https://example.com/loss3"),
        context_recycler.for_debugging_only_bindings()->TakeLossReportUrl());
    EXPECT_EQ(
        GURL("https://example.com/win3"),
        context_recycler.for_debugging_only_bindings()->TakeWinReportUrl());
  }
}

// Exercise RegisterAdBeaconBindings, and make sure they reset properly.
TEST_F(ContextRecyclerTest, RegisterAdBeaconBindings) {
  const char kScript[] = R"(
    function test(num) {
      let obj = {};
      for (let i = num; i < num * 2; ++i) {
        obj['f' + i] = 'https://example/com/' + i;
      }
      registerAdBeacon(obj);
    }
  )";

  v8::Local<v8::UnboundScript> script = Compile(kScript);
  ASSERT_FALSE(script.IsEmpty());

  ContextRecycler context_recycler(helper_.get());
  context_recycler.AddRegisterAdBeaconBindings();

  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;
    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), 1));
    EXPECT_THAT(error_msgs, ElementsAre());
    EXPECT_THAT(
        context_recycler.register_ad_beacon_bindings()->TakeAdBeaconMap(),
        ElementsAre(Pair("f1", GURL("https://example/com/1"))));
  }

  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;
    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), 2));
    EXPECT_THAT(error_msgs, ElementsAre());
    EXPECT_THAT(
        context_recycler.register_ad_beacon_bindings()->TakeAdBeaconMap(),
        ElementsAre(Pair("f2", GURL("https://example/com/2")),
                    Pair("f3", GURL("https://example/com/3"))));
  }
}

// Exercise ReportBindings, and make sure they reset properly.
TEST_F(ContextRecyclerTest, ReportBindings) {
  const char kScript[] = R"(
    function test(url) {
      sendReportTo(url);
    }
  )";

  v8::Local<v8::UnboundScript> script = Compile(kScript);
  ASSERT_FALSE(script.IsEmpty());

  ContextRecycler context_recycler(helper_.get());
  context_recycler.AddReportBindings();

  {
    // Make sure an exception doesn't stick around between executions.
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;
    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), std::string("not-a-url")));
    EXPECT_THAT(
        error_msgs,
        ElementsAre("https://example.org/script.js:3 Uncaught TypeError: "
                    "sendReportTo must be passed a valid HTTPS url."));
  }

  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;
    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(),
                         std::string("https://example.com/a")));
    EXPECT_THAT(error_msgs, ElementsAre());
    ASSERT_TRUE(context_recycler.report_bindings()->report_url().has_value());
    EXPECT_EQ("https://example.com/a",
              context_recycler.report_bindings()->report_url()->spec());
  }

  // Should already be cleared between executions.
  EXPECT_FALSE(context_recycler.report_bindings()->report_url().has_value());

  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;
    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(),
                         std::string("https://example.org/b")));
    EXPECT_THAT(error_msgs, ElementsAre());
    ASSERT_TRUE(context_recycler.report_bindings()->report_url().has_value());
    EXPECT_EQ("https://example.org/b",
              context_recycler.report_bindings()->report_url()->spec());
  }
}

// Exercise SetBidBindings, and make sure they reset properly.
TEST_F(ContextRecyclerTest, SetBidBindings) {
  const char kScript[] = R"(
    function test(bid) {
      setBid(bid);
    }
  )";

  v8::Local<v8::UnboundScript> script = Compile(kScript);
  ASSERT_FALSE(script.IsEmpty());

  ContextRecycler context_recycler(helper_.get());
  context_recycler.AddSetBidBindings();

  {
    ContextRecyclerScope scope(context_recycler);
    mojom::BidderWorkletNonSharedParamsPtr params =
        mojom::BidderWorkletNonSharedParams::New();
    params->ads.emplace();
    params->ads.value().emplace_back(GURL("https://example.com/ad1"),
                                     absl::nullopt);

    context_recycler.set_bid_bindings()->ReInitialize(
        base::TimeTicks::Now(),
        /*has_top_level_seller_origin=*/false, params.get(),
        /*restrict_to_kanon_ads=*/false);

    task_environment_.FastForwardBy(base::Milliseconds(500));

    gin::Dictionary bid_dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    bid_dict.Set("render", std::string("https://example.com/ad1"));
    bid_dict.Set("bid", 10.0);

    std::vector<std::string> error_msgs;
    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), bid_dict));

    EXPECT_THAT(error_msgs, ElementsAre());
    ASSERT_TRUE(context_recycler.set_bid_bindings()->has_bid());
    mojom::BidderWorkletBidPtr bid =
        context_recycler.set_bid_bindings()->TakeBid();
    EXPECT_EQ("https://example.com/ad1", bid->render_url);
    EXPECT_EQ(10.0, bid->bid);
    EXPECT_EQ(base::Milliseconds(500), bid->bid_duration);
  }

  {
    // Different ad objects get taken into account.
    ContextRecyclerScope scope(context_recycler);
    mojom::BidderWorkletNonSharedParamsPtr params =
        mojom::BidderWorkletNonSharedParams::New();
    params->ads.emplace();
    params->ads.value().emplace_back(GURL("https://example.com/notad1"),
                                     absl::nullopt);

    context_recycler.set_bid_bindings()->ReInitialize(
        base::TimeTicks::Now(),
        /*has_top_level_seller_origin=*/false, params.get(),
        /*restrict_to_kanon_ads=*/false);

    task_environment_.FastForwardBy(base::Milliseconds(500));

    gin::Dictionary bid_dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    bid_dict.Set("render", std::string("https://example.com/ad1"));
    bid_dict.Set("bid", 10.0);

    std::vector<std::string> error_msgs;
    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), bid_dict));

    EXPECT_THAT(
        error_msgs,
        ElementsAre("https://example.org/script.js:3 Uncaught TypeError: "
                    "bid render URL 'https://example.com/ad1' isn't one of "
                    "the registered creative URLs."));
    EXPECT_FALSE(context_recycler.set_bid_bindings()->has_bid());
  }

  {
    // Some components, and in a nested auction, w/o permission.
    ContextRecyclerScope scope(context_recycler);
    mojom::BidderWorkletNonSharedParamsPtr params =
        mojom::BidderWorkletNonSharedParams::New();
    params->ads.emplace();
    params->ads.value().emplace_back(GURL("https://example.com/ad3"),
                                     absl::nullopt);
    params->ad_components.emplace();
    params->ad_components.value().emplace_back(
        GURL("https://example.com/portion1"), absl::nullopt);
    params->ad_components.value().emplace_back(
        GURL("https://example.com/portion2"), absl::nullopt);

    context_recycler.set_bid_bindings()->ReInitialize(
        base::TimeTicks::Now(),
        /*has_top_level_seller_origin=*/true, params.get(),
        /*restrict_to_kanon_ads=*/false);

    task_environment_.FastForwardBy(base::Milliseconds(100));

    gin::Dictionary bid_dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    bid_dict.Set("render", std::string("https://example.com/ad1"));
    bid_dict.Set("bid", 10.0);
    std::vector<std::string> error_msgs;
    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), bid_dict));

    EXPECT_THAT(
        error_msgs,
        ElementsAre(
            "https://example.org/script.js:3 Uncaught TypeError: bid does not "
            "have allowComponentAuction set to true. Bid dropped from "
            "component auction."));
    EXPECT_FALSE(context_recycler.set_bid_bindings()->has_bid());
  }

  {
    // Some components, and in a nested auction, w/permission.
    ContextRecyclerScope scope(context_recycler);
    mojom::BidderWorkletNonSharedParamsPtr params =
        mojom::BidderWorkletNonSharedParams::New();
    params->ads.emplace();
    params->ads.value().emplace_back(GURL("https://example.com/ad5"),
                                     absl::nullopt);
    params->ad_components.emplace();
    params->ad_components.value().emplace_back(
        GURL("https://example.com/portion3"), absl::nullopt);
    params->ad_components.value().emplace_back(
        GURL("https://example.com/portion4"), absl::nullopt);
    params->ad_components.value().emplace_back(
        GURL("https://example.com/portion5"), absl::nullopt);

    context_recycler.set_bid_bindings()->ReInitialize(
        base::TimeTicks::Now(),
        /*has_top_level_seller_origin=*/true, params.get(),
        /*restrict_to_kanon_ads=*/false);

    task_environment_.FastForwardBy(base::Milliseconds(200));

    gin::Dictionary bid_dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    bid_dict.Set("render", std::string("https://example.com/ad5"));
    bid_dict.Set("bid", 15.0);
    bid_dict.Set("allowComponentAuction", true);
    std::vector<v8::Local<v8::Value>> components;
    components.push_back(gin::ConvertToV8(
        helper_->isolate(), std::string("https://example.com/portion3")));
    components.push_back(gin::ConvertToV8(
        helper_->isolate(), std::string("https://example.com/portion5")));
    bid_dict.Set("adComponents", components);

    std::vector<std::string> error_msgs;
    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), bid_dict));

    EXPECT_THAT(error_msgs, ElementsAre());
    ASSERT_TRUE(context_recycler.set_bid_bindings()->has_bid());
    mojom::BidderWorkletBidPtr bid =
        context_recycler.set_bid_bindings()->TakeBid();
    EXPECT_EQ("https://example.com/ad5", bid->render_url);
    EXPECT_EQ(15.0, bid->bid);
    EXPECT_EQ(base::Milliseconds(200), bid->bid_duration);
    ASSERT_TRUE(bid->ad_components.has_value());
    EXPECT_THAT(bid->ad_components.value(),
                ElementsAre(GURL("https://example.com/portion3"),
                            GURL("https://example.com/portion5")));
  }

  {
    // Wrong components.
    ContextRecyclerScope scope(context_recycler);
    mojom::BidderWorkletNonSharedParamsPtr params =
        mojom::BidderWorkletNonSharedParams::New();
    params->ads.emplace();
    params->ads.value().emplace_back(GURL("https://example.com/ad5"),
                                     absl::nullopt);
    params->ad_components.emplace();
    params->ad_components.value().emplace_back(
        GURL("https://example.com/portion6"), absl::nullopt);
    params->ad_components.value().emplace_back(
        GURL("https://example.com/portion7"), absl::nullopt);
    params->ad_components.value().emplace_back(
        GURL("https://example.com/portion8"), absl::nullopt);

    context_recycler.set_bid_bindings()->ReInitialize(
        base::TimeTicks::Now(),
        /*has_top_level_seller_origin=*/false, params.get(),
        /*restrict_to_kanon_ads=*/false);

    task_environment_.FastForwardBy(base::Milliseconds(200));

    gin::Dictionary bid_dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    bid_dict.Set("render", std::string("https://example.com/ad5"));
    bid_dict.Set("bid", 15.0);
    std::vector<v8::Local<v8::Value>> components;
    components.push_back(gin::ConvertToV8(
        helper_->isolate(), std::string("https://example.com/portion3")));
    components.push_back(gin::ConvertToV8(
        helper_->isolate(), std::string("https://example.com/portion5")));
    bid_dict.Set("adComponents", components);

    std::vector<std::string> error_msgs;
    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), bid_dict));

    EXPECT_THAT(
        error_msgs,
        ElementsAre(
            "https://example.org/script.js:3 Uncaught TypeError: bid "
            "adComponents "
            "URL 'https://example.com/portion3' isn't one of the registered "
            "creative URLs."));
    EXPECT_FALSE(context_recycler.set_bid_bindings()->has_bid());
  }

  {
    // restrict_to_kanon_ads = true affects checking.
    ContextRecyclerScope scope(context_recycler);
    mojom::BidderWorkletNonSharedParamsPtr params =
        mojom::BidderWorkletNonSharedParams::New();
    params->ads.emplace();
    params->ads.value().emplace_back(GURL("https://example.com/ad1"),
                                     absl::nullopt);

    context_recycler.set_bid_bindings()->ReInitialize(
        base::TimeTicks::Now(),
        /*has_top_level_seller_origin=*/false, params.get(),
        /*restrict_to_kanon_ads=*/true);

    task_environment_.FastForwardBy(base::Milliseconds(500));

    gin::Dictionary bid_dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    bid_dict.Set("render", std::string("https://example.com/ad1"));
    bid_dict.Set("bid", 10.0);

    std::vector<std::string> error_msgs;
    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), bid_dict));

    EXPECT_THAT(error_msgs, ElementsAre("https://example.org/script.js:3 "
                                        "Uncaught TypeError: bid render URL "
                                        "'https://example.com/ad1' isn't one "
                                        "of the registered creative URLs."));
    EXPECT_FALSE(context_recycler.set_bid_bindings()->has_bid());
  }

  {
    // restrict_to_kanon_ads = true affects checking, with ad permitted.
    ContextRecyclerScope scope(context_recycler);
    mojom::BidderWorkletNonSharedParamsPtr params =
        mojom::BidderWorkletNonSharedParams::New();
    params->ads.emplace();
    params->ads.value().emplace_back(GURL("https://example.com/ad1"),
                                     absl::nullopt);
    params->ads_kanon.emplace(GURL("https://example.com/ad1"), true);

    context_recycler.set_bid_bindings()->ReInitialize(
        base::TimeTicks::Now(),
        /*has_top_level_seller_origin=*/false, params.get(),
        /*restrict_to_kanon_ads=*/true);

    task_environment_.FastForwardBy(base::Milliseconds(500));

    gin::Dictionary bid_dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    bid_dict.Set("render", std::string("https://example.com/ad1"));
    bid_dict.Set("bid", 10.0);

    std::vector<std::string> error_msgs;
    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), bid_dict));

    EXPECT_THAT(error_msgs, ElementsAre());
    ASSERT_TRUE(context_recycler.set_bid_bindings()->has_bid());
    mojom::BidderWorkletBidPtr bid =
        context_recycler.set_bid_bindings()->TakeBid();
    EXPECT_EQ("https://example.com/ad1", bid->render_url);
    EXPECT_EQ(10.0, bid->bid);
    EXPECT_EQ(base::Milliseconds(500), bid->bid_duration);
  }
}

// Exercise SetPriorityBindings, and make sure they reset properly.
TEST_F(ContextRecyclerTest, SetPriorityBindings) {
  const char kScript[] = R"(
    function test(priority) {
      setPriority(priority);
    }
  )";

  v8::Local<v8::UnboundScript> script = Compile(kScript);
  ASSERT_FALSE(script.IsEmpty());

  ContextRecycler context_recycler(helper_.get());
  context_recycler.AddSetPriorityBindings();

  {
    // Make sure an exception doesn't stick around between executions.
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;
    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), std::string("not-a-priority")));
    EXPECT_THAT(
        error_msgs,
        ElementsAre("https://example.org/script.js:3 Uncaught TypeError: "
                    "setPriority requires 1 double parameter."));
  }

  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;
    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), 5.0));
    EXPECT_THAT(error_msgs, ElementsAre());
    ASSERT_TRUE(
        context_recycler.set_priority_bindings()->set_priority().has_value());
    EXPECT_EQ(5.0,
              context_recycler.set_priority_bindings()->set_priority().value());
  }

  // Should already be cleared between executions.
  EXPECT_FALSE(
      context_recycler.set_priority_bindings()->set_priority().has_value());

  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;
    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), 10.0));
    EXPECT_THAT(error_msgs, ElementsAre());
    ASSERT_TRUE(
        context_recycler.set_priority_bindings()->set_priority().has_value());
    EXPECT_EQ(10.0,
              context_recycler.set_priority_bindings()->set_priority().value());
  }
}

TEST_F(ContextRecyclerTest, BidderLazyFiller) {
  // Test to make sure lifetime managing/avoiding UaF is done right.
  // Actual argument passing is covered thoroughly in bidder worklet unit tests.
  const char kScript[] = R"(
    function test(obj) {
      if (!globalThis.stash) {
        // On first run
        globalThis.stash = obj;
      } else {
        return JSON.stringify(globalThis.stash);
      }
    }
  )";

  v8::Local<v8::UnboundScript> script = Compile(kScript);
  ASSERT_FALSE(script.IsEmpty());

  ContextRecycler context_recycler(helper_.get());
  context_recycler.AddInterestGroupLazyFiller();
  context_recycler.AddBiddingBrowserSignalsLazyFiller();

  {
    base::Time now = base::Time::Now();
    mojom::BidderWorkletNonSharedParamsPtr ig_params =
        mojom::BidderWorkletNonSharedParams::New();
    ig_params->user_bidding_signals.emplace("{\"j\": 1}");
    ig_params->trusted_bidding_signals_keys.emplace();
    ig_params->trusted_bidding_signals_keys->push_back("a");
    ig_params->trusted_bidding_signals_keys->push_back("b");
    ig_params->priority_vector.emplace();
    ig_params->priority_vector->insert(
        std::pair<std::string, double>("a", 42.0));

    mojom::BiddingBrowserSignalsPtr bs_params =
        mojom::BiddingBrowserSignals::New();
    bs_params->prev_wins.push_back(
        mojom::PreviousWin::New(now - base::Minutes(1), "[\"a\"]"));
    bs_params->prev_wins.push_back(
        mojom::PreviousWin::New(now - base::Minutes(2), "[\"b\"]"));

    ContextRecyclerScope scope(context_recycler);
    context_recycler.interest_group_lazy_filler()->ReInitialize(
        ig_params.get());
    context_recycler.bidding_browser_signals_lazy_filler()->ReInitialize(
        bs_params.get(), now);

    v8::Local<v8::Object> arg(v8::Object::New(helper_->isolate()));
    context_recycler.interest_group_lazy_filler()->FillInObject(arg);
    context_recycler.bidding_browser_signals_lazy_filler()->FillInObject(arg);

    std::vector<std::string> error_msgs;
    Run(scope, script, "test", error_msgs, arg);
    EXPECT_THAT(error_msgs, ElementsAre());
  }

  {
    base::Time now = base::Time::Now();
    mojom::BidderWorkletNonSharedParamsPtr ig_params =
        mojom::BidderWorkletNonSharedParams::New();
    ig_params->user_bidding_signals.emplace("{\"k\": 2}");
    ig_params->trusted_bidding_signals_keys.emplace();
    ig_params->trusted_bidding_signals_keys->push_back("c");
    ig_params->trusted_bidding_signals_keys->push_back("d");
    ig_params->priority_vector.emplace();
    ig_params->priority_vector->insert(
        std::pair<std::string, double>("e", 12.0));

    mojom::BiddingBrowserSignalsPtr bs_params =
        mojom::BiddingBrowserSignals::New();
    bs_params->prev_wins.push_back(
        mojom::PreviousWin::New(now - base::Minutes(3), "[\"c\"]"));
    bs_params->prev_wins.push_back(
        mojom::PreviousWin::New(now - base::Minutes(4), "[\"d\"]"));

    ContextRecyclerScope scope(context_recycler);
    context_recycler.interest_group_lazy_filler()->ReInitialize(
        ig_params.get());
    context_recycler.bidding_browser_signals_lazy_filler()->ReInitialize(
        bs_params.get(), now);

    v8::Local<v8::Object> arg(v8::Object::New(helper_->isolate()));
    context_recycler.interest_group_lazy_filler()->FillInObject(arg);
    context_recycler.bidding_browser_signals_lazy_filler()->FillInObject(arg);

    std::vector<std::string> error_msgs;
    v8::MaybeLocal<v8::Value> maybe_result =
        Run(scope, script, "test", error_msgs, arg);
    EXPECT_THAT(error_msgs, ElementsAre());
    v8::Local<v8::Value> result;
    ASSERT_TRUE(maybe_result.ToLocal(&result));
    std::string str_result;
    ASSERT_TRUE(gin::ConvertFromV8(helper_->isolate(), result, &str_result));
    EXPECT_EQ(
        "{\"userBiddingSignals\":{\"k\":2},"
        "\"trustedBiddingSignalsKeys\":[\"c\",\"d\"],"
        "\"priorityVector\":{\"e\":12},"
        "\"prevWins\":[[240,[\"d\"]],[180,[\"c\"]]]}",
        str_result);
  }
}

TEST_F(ContextRecyclerTest, BidderLazyFiller2) {
  // Test to make sure that stale objects with fields added that are no longer
  // there handle it gracefully.
  const char kScript[] = R"(
    function test(obj) {
      if (!globalThis.stash) {
        // On first run
        globalThis.stash = obj;
      } else {
        return JSON.stringify(globalThis.stash);
      }
    }
  )";

  v8::Local<v8::UnboundScript> script = Compile(kScript);
  ASSERT_FALSE(script.IsEmpty());

  ContextRecycler context_recycler(helper_.get());
  context_recycler.AddInterestGroupLazyFiller();
  context_recycler.AddBiddingBrowserSignalsLazyFiller();

  {
    base::Time now = base::Time::Now();
    mojom::BidderWorkletNonSharedParamsPtr ig_params =
        mojom::BidderWorkletNonSharedParams::New();
    ig_params->user_bidding_signals.emplace("{\"j\": 1}");
    ig_params->trusted_bidding_signals_keys.emplace();
    ig_params->trusted_bidding_signals_keys->push_back("a");
    ig_params->trusted_bidding_signals_keys->push_back("b");
    ig_params->priority_vector.emplace();
    ig_params->priority_vector->insert(
        std::pair<std::string, double>("a", 42.0));

    mojom::BiddingBrowserSignalsPtr bs_params =
        mojom::BiddingBrowserSignals::New();
    bs_params->prev_wins.push_back(
        mojom::PreviousWin::New(now - base::Minutes(1), "[\"a\"]"));
    bs_params->prev_wins.push_back(
        mojom::PreviousWin::New(now - base::Minutes(2), "[\"b\"]"));

    ContextRecyclerScope scope(context_recycler);
    context_recycler.interest_group_lazy_filler()->ReInitialize(
        ig_params.get());
    context_recycler.bidding_browser_signals_lazy_filler()->ReInitialize(
        bs_params.get(), now);

    v8::Local<v8::Object> arg(v8::Object::New(helper_->isolate()));
    context_recycler.interest_group_lazy_filler()->FillInObject(arg);
    context_recycler.bidding_browser_signals_lazy_filler()->FillInObject(arg);

    std::vector<std::string> error_msgs;
    Run(scope, script, "test", error_msgs, arg);
    EXPECT_THAT(error_msgs, ElementsAre());
  }

  {
    // Now cover the data for the fields not actually being there.
    base::Time now = base::Time::Now();
    mojom::BidderWorkletNonSharedParamsPtr ig_params =
        mojom::BidderWorkletNonSharedParams::New();
    mojom::BiddingBrowserSignalsPtr bs_params =
        mojom::BiddingBrowserSignals::New();

    ContextRecyclerScope scope(context_recycler);
    context_recycler.interest_group_lazy_filler()->ReInitialize(
        ig_params.get());
    context_recycler.bidding_browser_signals_lazy_filler()->ReInitialize(
        bs_params.get(), now);

    v8::Local<v8::Object> arg(v8::Object::New(helper_->isolate()));
    context_recycler.interest_group_lazy_filler()->FillInObject(arg);
    context_recycler.bidding_browser_signals_lazy_filler()->FillInObject(arg);

    std::vector<std::string> error_msgs;
    v8::MaybeLocal<v8::Value> maybe_result =
        Run(scope, script, "test", error_msgs, arg);
    EXPECT_THAT(error_msgs, ElementsAre());
    v8::Local<v8::Value> result;
    ASSERT_TRUE(maybe_result.ToLocal(&result));
    std::string str_result;
    ASSERT_TRUE(gin::ConvertFromV8(helper_->isolate(), result, &str_result));
    EXPECT_EQ(
        "{\"userBiddingSignals\":null,"
        "\"trustedBiddingSignalsKeys\":null,"
        "\"priorityVector\":null,"
        "\"prevWins\":[]}",
        str_result);
  }
}

class ContextRecyclerPrivateAggregationEnabledTest
    : public ContextRecyclerTest {
 public:
  ContextRecyclerPrivateAggregationEnabledTest() {
    scoped_feature_list_.InitAndEnableFeature(content::kPrivateAggregationApi);
  }

  // Wraps a debug_key into the appropriate dictionary. Templated to allow both
  // integers and strings.
  template <typename T>
  v8::Local<v8::Value> WrapDebugKey(T debug_key) {
    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("debug_key", debug_key);
    return gin::ConvertToV8(helper_->isolate(), dict);
  }

  // Expects that pa_requests has one request, and the request has the given
  // bucket, value and debug_key (or none, if absl::nullopt). Also expects that
  // debug mode is enabled if debug_key is not absl::nullopt.
  void ExpectOneHistogramRequestEqualTo(
      std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>
          pa_requests,
      absl::uint128 bucket,
      int value,
      absl::optional<content::mojom::DebugKeyPtr> debug_key = absl::nullopt) {
    content::mojom::AggregatableReportHistogramContribution
        expected_contribution(bucket, value);

    content::mojom::DebugModeDetailsPtr debug_mode_details;
    if (debug_key.has_value()) {
      debug_mode_details = content::mojom::DebugModeDetails::New(
          /*is_enabled=*/true,
          /*debug_key=*/std::move(debug_key.value()));
    } else {
      debug_mode_details = content::mojom::DebugModeDetails::New();
    }

    auction_worklet::mojom::PrivateAggregationRequest expected_request(
        auction_worklet::mojom::AggregatableReportContribution::
            NewHistogramContribution(expected_contribution.Clone()),
        content::mojom::AggregationServiceMode::kDefault,
        std::move(debug_mode_details));

    ASSERT_EQ(pa_requests.size(), 1u);
    EXPECT_EQ(pa_requests[0], expected_request.Clone());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Exercise `sendHistogramReport()` of PrivateAggregationBindings, and make sure
// they reset properly.
TEST_F(ContextRecyclerPrivateAggregationEnabledTest,
       PrivateAggregationBindingsSendHistogramReport) {
  using PrivateAggregationRequests =
      std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>;

  const char kScript[] = R"(
    function test(args) {
      // Passing BigInts in directly is complicated so we construct them from
      // strings.
      if (typeof args.bucket === "string") {
        args.bucket = BigInt(args.bucket);
      }
      privateAggregation.sendHistogramReport(args);
    }
    function doNothing() {}
  )";

  v8::Local<v8::UnboundScript> script = Compile(kScript);
  ASSERT_FALSE(script.IsEmpty());

  ContextRecycler context_recycler(helper_.get());
  context_recycler.AddPrivateAggregationBindings(
      /*private_aggregation_permissions_policy_allowed=*/true);

  // Basic test
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", std::string("123"));
    dict.Set("value", 45);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(error_msgs, ElementsAre());

    ExpectOneHistogramRequestEqualTo(
        context_recycler.private_aggregation_bindings()
            ->TakePrivateAggregationRequests(),
        /*bucket=*/123, /*value=*/45);
  }

  // Large bucket
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", std::string("18446744073709551616"));
    dict.Set("value", 45);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(error_msgs, ElementsAre());

    ExpectOneHistogramRequestEqualTo(
        context_recycler.private_aggregation_bindings()
            ->TakePrivateAggregationRequests(),
        /*bucket=*/absl::MakeUint128(/*high=*/1, /*low=*/0), /*value=*/45);
  }

  // Maximum bucket
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", std::string("340282366920938463463374607431768211455"));
    dict.Set("value", 45);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(error_msgs, ElementsAre());

    ExpectOneHistogramRequestEqualTo(
        context_recycler.private_aggregation_bindings()
            ->TakePrivateAggregationRequests(),
        /*bucket=*/absl::Uint128Max(), /*value=*/45);
  }

  // Zero bucket
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", std::string("0"));
    dict.Set("value", 45);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(error_msgs, ElementsAre());

    ExpectOneHistogramRequestEqualTo(
        context_recycler.private_aggregation_bindings()
            ->TakePrivateAggregationRequests(),
        /*bucket=*/0, /*value=*/45);
  }

  // Zero value
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", std::string("123"));
    dict.Set("value", 0);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(error_msgs, ElementsAre());

    ExpectOneHistogramRequestEqualTo(
        context_recycler.private_aggregation_bindings()
            ->TakePrivateAggregationRequests(),
        /*bucket=*/123, /*value=*/0);
  }

  // Multiple requests
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    {
      gin::Dictionary dict_1 = gin::Dictionary::CreateEmpty(helper_->isolate());
      dict_1.Set("bucket", std::string("123"));
      dict_1.Set("value", 45);

      Run(scope, script, "test", error_msgs,
          gin::ConvertToV8(helper_->isolate(), dict_1));
      EXPECT_THAT(error_msgs, ElementsAre());
    }
    {
      gin::Dictionary dict_2 = gin::Dictionary::CreateEmpty(helper_->isolate());
      dict_2.Set("bucket", std::string("678"));
      dict_2.Set("value", 90);

      Run(scope, script, "test", error_msgs,
          gin::ConvertToV8(helper_->isolate(), dict_2));
      EXPECT_THAT(error_msgs, ElementsAre());
    }

    content::mojom::AggregatableReportHistogramContribution
        expected_contribution_1(/*bucket=*/123, /*value=*/45);
    auction_worklet::mojom::PrivateAggregationRequest expected_request_1(
        auction_worklet::mojom::AggregatableReportContribution::
            NewHistogramContribution(expected_contribution_1.Clone()),
        content::mojom::AggregationServiceMode::kDefault,
        content::mojom::DebugModeDetails::New());

    content::mojom::AggregatableReportHistogramContribution
        expected_contribution_2(/*bucket=*/678, /*value=*/90);
    auction_worklet::mojom::PrivateAggregationRequest expected_request_2(
        auction_worklet::mojom::AggregatableReportContribution::
            NewHistogramContribution(expected_contribution_2.Clone()),
        content::mojom::AggregationServiceMode::kDefault,
        content::mojom::DebugModeDetails::New());

    PrivateAggregationRequests pa_requests =
        context_recycler.private_aggregation_bindings()
            ->TakePrivateAggregationRequests();
    ASSERT_EQ(pa_requests.size(), 2u);
    EXPECT_EQ(pa_requests[0], expected_request_1.Clone());
    EXPECT_EQ(pa_requests[1], expected_request_2.Clone());
  }

  // Non-integer value
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", std::string("123"));
    dict.Set("value", 4.5);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(
        error_msgs,
        ElementsAre("https://example.org/script.js:8 Uncaught TypeError: "
                    "Value must be an integer Number."));

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }

  // Too large bucket
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", std::string("340282366920938463463374607431768211456"));
    dict.Set("value", 45);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(
        error_msgs,
        ElementsAre("https://example.org/script.js:8 Uncaught TypeError: "
                    "BigInt is too large."));

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }

  // Non-BigInt bucket
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", 123);
    dict.Set("value", 45);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(
        error_msgs,
        ElementsAre("https://example.org/script.js:8 Uncaught TypeError: "
                    "bucket must be a BigInt."));

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }

  // Negative bucket
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", std::string("-1"));
    dict.Set("value", 45);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(
        error_msgs,
        ElementsAre("https://example.org/script.js:8 Uncaught TypeError: "
                    "BigInt must be non-negative."));

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }

  // Negative value
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", std::string("123"));
    dict.Set("value", -1);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(
        error_msgs,
        ElementsAre("https://example.org/script.js:8 Uncaught TypeError: "
                    "Value must be non-negative."));

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }

  // Missing bucket
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("value", 45);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(
        error_msgs,
        ElementsAre(
            "https://example.org/script.js:8 Uncaught TypeError: "
            "Invalid or missing bucket in sendHistogramReport argument."));

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }

  // Missing value
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", std::string("123"));

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(
        error_msgs,
        ElementsAre(
            "https://example.org/script.js:8 Uncaught TypeError: "
            "Invalid or missing value in sendHistogramReport argument."));

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }

  // API not called
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());

    Run(scope, script, "doNothing", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(error_msgs, ElementsAre());

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }
}

// Exercise `enableDebugMode()` of PrivateAggregationBindings, and make sure
// they reset properly.
TEST_F(ContextRecyclerPrivateAggregationEnabledTest,
       PrivateAggregationBindingsEnableDebugMode) {
  using PrivateAggregationRequests =
      std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>;

  const char kScript[] = R"(
    function test(args) {
      // Passing BigInts in directly is complicated so we construct them from
      // strings.
      if (typeof args.bucket === "string") {
        args.bucket = BigInt(args.bucket);
      }
      privateAggregation.sendHistogramReport(args);
    }
    function enableDebugMode(arg) {
      if (arg === undefined) {
        privateAggregation.enableDebugMode();
        return;
      }

      // Passing BigInts in directly is complicated so we construct them from
      // strings.
      if (typeof arg.debug_key === "string") {
        arg.debug_key = BigInt(arg.debug_key);
      }
      privateAggregation.enableDebugMode(arg);
    }
  )";

  v8::Local<v8::UnboundScript> script = Compile(kScript);
  ASSERT_FALSE(script.IsEmpty());

  ContextRecycler context_recycler(helper_.get());
  context_recycler.AddPrivateAggregationBindings(
      /*private_aggregation_permissions_policy_allowed=*/true);

  // Debug mode enabled with no debug key
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    Run(scope, script, "enableDebugMode", error_msgs);
    EXPECT_THAT(error_msgs, ElementsAre());

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", std::string("123"));
    dict.Set("value", 45);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(error_msgs, ElementsAre());

    ExpectOneHistogramRequestEqualTo(
        context_recycler.private_aggregation_bindings()
            ->TakePrivateAggregationRequests(),
        /*bucket=*/123, /*value=*/45, /*debug_key=*/nullptr);
  }

  // Debug mode enabled with debug key
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    Run(scope, script, "enableDebugMode", error_msgs,
        WrapDebugKey(std::string("1234")));
    EXPECT_THAT(error_msgs, ElementsAre());

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", std::string("123"));
    dict.Set("value", 45);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(error_msgs, ElementsAre());

    ExpectOneHistogramRequestEqualTo(
        context_recycler.private_aggregation_bindings()
            ->TakePrivateAggregationRequests(),
        /*bucket=*/123, /*value=*/45,
        /*debug_key=*/content::mojom::DebugKey::New(1234u));
  }

  // Debug mode enabled with large debug key
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    Run(scope, script, "enableDebugMode", error_msgs,
        WrapDebugKey(std::string("18446744073709551615")));
    EXPECT_THAT(error_msgs, ElementsAre());

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", std::string("123"));
    dict.Set("value", 45);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(error_msgs, ElementsAre());

    ExpectOneHistogramRequestEqualTo(
        context_recycler.private_aggregation_bindings()
            ->TakePrivateAggregationRequests(),
        /*bucket=*/123, /*value=*/45, /*debug_key=*/
        content::mojom::DebugKey::New(std::numeric_limits<uint64_t>::max()));
  }

  // Negative debug key
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    Run(scope, script, "enableDebugMode", error_msgs,
        WrapDebugKey(std::string("-1")));
    EXPECT_THAT(
        error_msgs,
        ElementsAre("https://example.org/script.js:21 Uncaught TypeError: "
                    "BigInt must be non-negative."));

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }

  // Too large debug key
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    Run(scope, script, "enableDebugMode", error_msgs,
        WrapDebugKey(std::string("18446744073709551616")));
    EXPECT_THAT(error_msgs,
                ElementsAre("https://example.org/script.js:21 Uncaught "
                            "TypeError: BigInt is too large."));

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }

  // Non-BigInt debug key
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    Run(scope, script, "enableDebugMode", error_msgs, WrapDebugKey(1234));
    EXPECT_THAT(
        error_msgs,
        ElementsAre("https://example.org/script.js:21 Uncaught TypeError: "
                    "debug_key must be a BigInt."));

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }

  // Invalid enableDebugMode argument
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    // The debug key is not wrapped in a dictionary.
    Run(scope, script, "enableDebugMode", error_msgs,
        gin::ConvertToV8(helper_->isolate(), 1234));
    EXPECT_THAT(
        error_msgs,
        ElementsAre("https://example.org/script.js:21 Uncaught TypeError: "
                    "Invalid argument in enableDebugMode."));

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }

  // enableDebugMode called twice: second call fails, first continues to apply
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    Run(scope, script, "enableDebugMode", error_msgs,
        WrapDebugKey(std::string("1234")));
    EXPECT_THAT(error_msgs, ElementsAre());

    Run(scope, script, "enableDebugMode", error_msgs);
    EXPECT_THAT(
        error_msgs,
        ElementsAre("https://example.org/script.js:12 Uncaught TypeError: "
                    "enableDebugMode may be called at most once."));
    error_msgs.clear();

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", std::string("123"));
    dict.Set("value", 45);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(error_msgs, ElementsAre());

    ExpectOneHistogramRequestEqualTo(
        context_recycler.private_aggregation_bindings()
            ->TakePrivateAggregationRequests(),
        /*bucket=*/123, /*value=*/45,
        /*debug_key=*/content::mojom::DebugKey::New(1234u));
  }

  // enableDebugMode called after report requested: debug details still applied
  // Note that Shared Storage worklets have different behavior in this case.
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", std::string("123"));
    dict.Set("value", 45);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(error_msgs, ElementsAre());

    Run(scope, script, "enableDebugMode", error_msgs,
        WrapDebugKey(std::string("1234")));
    EXPECT_THAT(error_msgs, ElementsAre());

    ExpectOneHistogramRequestEqualTo(
        context_recycler.private_aggregation_bindings()
            ->TakePrivateAggregationRequests(),
        /*bucket=*/123, /*value=*/45,
        /*debug_key=*/content::mojom::DebugKey::New(1234u));
  }

  // Multiple debug mode reports
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    Run(scope, script, "enableDebugMode", error_msgs,
        WrapDebugKey(std::string("1234")));
    EXPECT_THAT(error_msgs, ElementsAre());

    {
      gin::Dictionary dict_1 = gin::Dictionary::CreateEmpty(helper_->isolate());
      dict_1.Set("bucket", std::string("123"));
      dict_1.Set("value", 45);

      Run(scope, script, "test", error_msgs,
          gin::ConvertToV8(helper_->isolate(), dict_1));
      EXPECT_THAT(error_msgs, ElementsAre());
    }
    {
      gin::Dictionary dict_2 = gin::Dictionary::CreateEmpty(helper_->isolate());
      dict_2.Set("bucket", std::string("678"));
      dict_2.Set("value", 90);

      Run(scope, script, "test", error_msgs,
          gin::ConvertToV8(helper_->isolate(), dict_2));
      EXPECT_THAT(error_msgs, ElementsAre());
    }

    content::mojom::AggregatableReportHistogramContribution
        expected_contribution_1(/*bucket=*/123, /*value=*/45);
    auction_worklet::mojom::PrivateAggregationRequest expected_request_1(
        auction_worklet::mojom::AggregatableReportContribution::
            NewHistogramContribution(expected_contribution_1.Clone()),
        content::mojom::AggregationServiceMode::kDefault,
        content::mojom::DebugModeDetails::New(
            /*is_enabled=*/true,
            /*debug_key=*/content::mojom::DebugKey::New(1234u)));

    content::mojom::AggregatableReportHistogramContribution
        expected_contribution_2(/*bucket=*/678, /*value=*/90);
    auction_worklet::mojom::PrivateAggregationRequest expected_request_2(
        auction_worklet::mojom::AggregatableReportContribution::
            NewHistogramContribution(expected_contribution_2.Clone()),
        content::mojom::AggregationServiceMode::kDefault,
        content::mojom::DebugModeDetails::New(
            /*is_enabled=*/true,
            /*debug_key=*/content::mojom::DebugKey::New(1234u)));

    PrivateAggregationRequests pa_requests =
        context_recycler.private_aggregation_bindings()
            ->TakePrivateAggregationRequests();
    ASSERT_EQ(pa_requests.size(), 2u);
    EXPECT_EQ(pa_requests[0], expected_request_1.Clone());
    EXPECT_EQ(pa_requests[1], expected_request_2.Clone());
  }
}

class ContextRecyclerPrivateAggregationExtensionsEnabledTest
    : public ContextRecyclerTest {
 public:
  ContextRecyclerPrivateAggregationExtensionsEnabledTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        content::kPrivateAggregationApi,
        {{"fledge_extensions_enabled", "true"}});
  }

  // Creates a PrivateAggregationRequest with ForEvent contribution.
  auction_worklet::mojom::PrivateAggregationRequestPtr CreateForEventRequest(
      absl::uint128 bucket,
      int value,
      const std::string& event_type) {
    auction_worklet::mojom::AggregatableReportForEventContribution contribution(
        auction_worklet::mojom::ForEventSignalBucket::NewIdBucket(bucket),
        auction_worklet::mojom::ForEventSignalValue::NewIntValue(value),
        std::move(event_type));

    return auction_worklet::mojom::PrivateAggregationRequest::New(
        auction_worklet::mojom::AggregatableReportContribution::
            NewForEventContribution(contribution.Clone()),
        content::mojom::AggregationServiceMode::kDefault,
        content::mojom::DebugModeDetails::New());
  }

  // Expects given `pa_requests` to have one item, which equals to the
  // PrivateAggregationRequests created from given ForEvent contribution
  // `expected_contribution`.
  void ExpectOneForEventRequestEqualTo(
      std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>
          pa_requests,
      auction_worklet::mojom::AggregatableReportForEventContributionPtr
          expected_contribution) {
    auction_worklet::mojom::PrivateAggregationRequest expected_request(
        auction_worklet::mojom::AggregatableReportContribution::
            NewForEventContribution(expected_contribution.Clone()),
        content::mojom::AggregationServiceMode::kDefault,
        content::mojom::DebugModeDetails::New());

    ASSERT_EQ(pa_requests.size(), 1u);
    EXPECT_EQ(pa_requests[0], expected_request.Clone());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Exercise `reportContributionsForEvent()` of PrivateAggregationBindings, and
// make sure they reset properly.
TEST_F(ContextRecyclerPrivateAggregationExtensionsEnabledTest,
       PrivateAggregationForEventBindings) {
  using PrivateAggregationRequests =
      std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>;

  const char kScript[] = R"(
    function test(args) {
      // Passing BigInts in directly is complicated so we construct them from
      // strings.
      if (typeof args.bucket === 'string') {
        args.bucket = BigInt(args.bucket);
      } else if (
          typeof args.bucket === 'object' &&
          typeof args.bucket.offset === 'string') {
        args.bucket.offset = BigInt(args.bucket.offset);
      }
      privateAggregation.reportContributionForEvent('reserved.win', args);
    }

    function testDifferentEventTypes(args) {
      // Passing BigInts in directly is complicated so we construct them from
      // strings.
      if (typeof args.bucket === "string") {
        args.bucket = BigInt(args.bucket);
      }
      privateAggregation.reportContributionForEvent('reserved.win', args);
      // Add 1 to value, to let reserved.loss request gets different
      // contribution from reserved.win request.
      args.value += 1;
      privateAggregation.reportContributionForEvent('reserved.loss', args);
      args.value += 1;
      privateAggregation.reportContributionForEvent('reserved.always', args);
      args.value += 1;
      // Arbitrary unreserved event type.
      privateAggregation.reportContributionForEvent('click', args);
    }

    function testMissingEventType(args) {
      if (typeof args.bucket === "string") {
        args.bucket = BigInt(args.bucket);
      }
      privateAggregation.reportContributionForEvent(args);
    }

    function testMissingContribution() {
      privateAggregation.reportContributionForEvent('reserved.win');
    }

    function testWrongArgumentsOrder(args) {
      if (typeof args.bucket === "string") {
        args.bucket = BigInt(args.bucket);
      }
      privateAggregation.reportContributionForEvent(args, 'reserved.win');
    }

    function testInvalidReservedEventType(args) {
      if (typeof args.bucket === "string") {
        args.bucket = BigInt(args.bucket);
      }
      privateAggregation.reportContributionForEvent("reserved.something", args);
    }

    function doNothing() {}
  )";

  v8::Local<v8::UnboundScript> script = Compile(kScript);
  ASSERT_FALSE(script.IsEmpty());

  ContextRecycler context_recycler(helper_.get());
  context_recycler.AddPrivateAggregationBindings(
      /*private_aggregation_permissions_policy_allowed=*/true);

  // Basic test
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", std::string("123"));
    dict.Set("value", 45);

    Run(scope, script, "testDifferentEventTypes", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(error_msgs, ElementsAre());

    auto pa_requests = context_recycler.private_aggregation_bindings()
                           ->TakePrivateAggregationRequests();

    ASSERT_EQ(pa_requests.size(), 4u);
    EXPECT_EQ(pa_requests[0],
              CreateForEventRequest(/*bucket=*/123, /*value=*/45,
                                    /*event_type=*/kReservedWin));
    EXPECT_EQ(pa_requests[1],
              CreateForEventRequest(/*bucket=*/123, /*value=*/46,
                                    /*event_type=*/kReservedLoss));
    EXPECT_EQ(pa_requests[2],
              CreateForEventRequest(/*bucket=*/123, /*value=*/47,
                                    /*event_type=*/kReservedAlways));
    EXPECT_EQ(pa_requests[3],
              CreateForEventRequest(/*bucket=*/123, /*value=*/48,
                                    /*event_type=*/"click"));

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }

  // Missing event_type (the first argument) to reportContributionForEvent()
  // API.
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", std::string("123"));
    dict.Set("value", 45);

    Run(scope, script, "testMissingEventType", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(
        error_msgs,
        ElementsAre(
            "https://example.org/script.js:37 Uncaught TypeError: "
            "reportContributionForEvent requires 2 parameters, with first "
            "parameter being a string and second parameter being an object."));

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }

  // Missing contribution (the second argument) to reportContributionForEvent()
  // API.
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());

    Run(scope, script, "testMissingContribution", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(
        error_msgs,
        ElementsAre(
            "https://example.org/script.js:41 Uncaught TypeError: "
            "reportContributionForEvent requires 2 parameters, with first "
            "parameter being a string and second parameter being an object."));

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }

  // The two arguments to reportContributionForEvent() API are in wrong order.
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", std::string("123"));
    dict.Set("value", 45);

    Run(scope, script, "testWrongArgumentsOrder", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(
        error_msgs,
        ElementsAre(
            "https://example.org/script.js:48 Uncaught TypeError: "
            "reportContributionForEvent requires 2 parameters, with first "
            "parameter being a string and second parameter being an object."));

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }

  // Invalid reserved event type.
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", std::string("123"));
    dict.Set("value", 45);

    Run(scope, script, "testInvalidReservedEventType", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    // Don't throw an error if an invalid reserved event type is provided, to
    // provide forward compatibility with new reserved event types added later.
    EXPECT_THAT(error_msgs, ElementsAre());

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }

  // Large bucket
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", std::string("18446744073709551616"));
    dict.Set("value", 45);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(error_msgs, ElementsAre());

    auction_worklet::mojom::AggregatableReportForEventContribution
        expected_contribution(
            /*bucket=*/auction_worklet::mojom::ForEventSignalBucket::
                NewIdBucket(absl::MakeUint128(/*high=*/1, /*low=*/0)),
            /*value=*/
            auction_worklet::mojom::ForEventSignalValue::NewIntValue(45),
            /*event_type=*/kReservedWin);

    ExpectOneForEventRequestEqualTo(
        context_recycler.private_aggregation_bindings()
            ->TakePrivateAggregationRequests(),
        expected_contribution.Clone());
  }

  // Maximum bucket
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", std::string("340282366920938463463374607431768211455"));
    dict.Set("value", 45);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(error_msgs, ElementsAre());

    auction_worklet::mojom::AggregatableReportForEventContribution
        expected_contribution(
            /*bucket=*/auction_worklet::mojom::ForEventSignalBucket::
                NewIdBucket(absl::Uint128Max()),
            /*value=*/
            auction_worklet::mojom::ForEventSignalValue::NewIntValue(45),
            /*event_type=*/kReservedWin);

    ExpectOneForEventRequestEqualTo(
        context_recycler.private_aggregation_bindings()
            ->TakePrivateAggregationRequests(),
        expected_contribution.Clone());
  }

  // Zero bucket
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", std::string("0"));
    dict.Set("value", 45);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(error_msgs, ElementsAre());

    auction_worklet::mojom::AggregatableReportForEventContribution
        expected_contribution(
            /*bucket=*/auction_worklet::mojom::ForEventSignalBucket::
                NewIdBucket(0),
            /*value=*/
            auction_worklet::mojom::ForEventSignalValue::NewIntValue(45),
            /*event_type=*/kReservedWin);

    ExpectOneForEventRequestEqualTo(
        context_recycler.private_aggregation_bindings()
            ->TakePrivateAggregationRequests(),
        expected_contribution.Clone());
  }

  // Zero value
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", std::string("123"));
    dict.Set("value", 0);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(error_msgs, ElementsAre());

    auction_worklet::mojom::AggregatableReportForEventContribution
        expected_contribution(
            /*bucket=*/auction_worklet::mojom::ForEventSignalBucket::
                NewIdBucket(123),
            /*value=*/
            auction_worklet::mojom::ForEventSignalValue::NewIntValue(0),
            /*event_type=*/kReservedWin);

    ExpectOneForEventRequestEqualTo(
        context_recycler.private_aggregation_bindings()
            ->TakePrivateAggregationRequests(),
        expected_contribution.Clone());
  }

  // Multiple requests
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    {
      gin::Dictionary dict_1 = gin::Dictionary::CreateEmpty(helper_->isolate());
      dict_1.Set("bucket", std::string("123"));
      dict_1.Set("value", 45);

      Run(scope, script, "test", error_msgs,
          gin::ConvertToV8(helper_->isolate(), dict_1));
      EXPECT_THAT(error_msgs, ElementsAre());
    }
    {
      gin::Dictionary dict_2 = gin::Dictionary::CreateEmpty(helper_->isolate());
      dict_2.Set("bucket", std::string("678"));
      dict_2.Set("value", 90);

      Run(scope, script, "test", error_msgs,
          gin::ConvertToV8(helper_->isolate(), dict_2));
      EXPECT_THAT(error_msgs, ElementsAre());
    }

    PrivateAggregationRequests pa_requests =
        context_recycler.private_aggregation_bindings()
            ->TakePrivateAggregationRequests();
    ASSERT_EQ(pa_requests.size(), 2u);
    EXPECT_EQ(pa_requests[0],
              CreateForEventRequest(/*bucket=*/123, /*value=*/45,
                                    /*event_type=*/kReservedWin));
    EXPECT_EQ(pa_requests[1],
              CreateForEventRequest(/*bucket=*/678, /*value=*/90,
                                    /*event_type=*/kReservedWin));
  }

  // Too large bucket
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", std::string("340282366920938463463374607431768211456"));
    dict.Set("value", 45);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(
        error_msgs,
        ElementsAre("https://example.org/script.js:12 Uncaught TypeError: "
                    "BigInt is too large."));

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }

  // Dictionary bucket
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary bucket_dict =
        gin::Dictionary::CreateEmpty(helper_->isolate());
    bucket_dict.Set("baseValue", std::string("bidRejectReason"));
    bucket_dict.Set("scale", 2);
    bucket_dict.Set("offset", std::string("-255"));

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", bucket_dict);
    dict.Set("value", 1);

    auto signal_bucket = auction_worklet::mojom::SignalBucket::New(
        /*base_value=*/auction_worklet::mojom::BaseValue::kBidRejectReason,
        /*scale=*/2,
        /*offset=*/
        auction_worklet::mojom::BucketOffset::New(/*value=*/255,
                                                  /*is_negative=*/true));

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(error_msgs, ElementsAre());

    auction_worklet::mojom::AggregatableReportForEventContribution
        expected_contribution(
            /*bucket=*/auction_worklet::mojom::ForEventSignalBucket::
                NewSignalBucket(std::move(signal_bucket)),
            /*value=*/
            auction_worklet::mojom::ForEventSignalValue::NewIntValue(1),
            /*event_type=*/kReservedWin);

    ExpectOneForEventRequestEqualTo(
        context_recycler.private_aggregation_bindings()
            ->TakePrivateAggregationRequests(),
        expected_contribution.Clone());
  }

  // Dictionary bucket. Scale and offset are optional.
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary bucket_dict =
        gin::Dictionary::CreateEmpty(helper_->isolate());
    bucket_dict.Set("baseValue", std::string("bidRejectReason"));

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", bucket_dict);
    dict.Set("value", 1);

    auction_worklet::mojom::SignalBucket signal_bucket;
    signal_bucket.base_value =
        auction_worklet::mojom::BaseValue::kBidRejectReason;
    signal_bucket.offset = nullptr;

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(error_msgs, ElementsAre());

    auction_worklet::mojom::AggregatableReportForEventContribution
        expected_contribution(
            /*bucket=*/auction_worklet::mojom::ForEventSignalBucket::
                NewSignalBucket(signal_bucket.Clone()),
            /*value=*/
            auction_worklet::mojom::ForEventSignalValue::NewIntValue(1),
            /*event_type=*/kReservedWin);

    ExpectOneForEventRequestEqualTo(
        context_recycler.private_aggregation_bindings()
            ->TakePrivateAggregationRequests(),
        expected_contribution.Clone());
  }

  // Invalid bucket dictionary, which has no "baseValue" key.
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary bucket_dict =
        gin::Dictionary::CreateEmpty(helper_->isolate());
    bucket_dict.Set("scale", 1);

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", bucket_dict);
    dict.Set("value", 1);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(error_msgs,
                ElementsAre("https://example.org/script.js:12 Uncaught "
                            "TypeError: Invalid bucket dictionary."));

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }

  // Invalid bucket dictionary, whose baseValue is invalid.
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary bucket_dict =
        gin::Dictionary::CreateEmpty(helper_->isolate());
    bucket_dict.Set("baseValue", std::string("notValidBaseValue"));

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", bucket_dict);
    dict.Set("value", 1);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(error_msgs,
                ElementsAre("https://example.org/script.js:12 Uncaught "
                            "TypeError: Invalid bucket dictionary."));

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }

  // Invalid bucket dictionary, whose scale is not a Number.
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary bucket_dict =
        gin::Dictionary::CreateEmpty(helper_->isolate());
    bucket_dict.Set("baseValue", std::string("winningBid"));
    bucket_dict.Set("scale", std::string("255"));

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", bucket_dict);
    dict.Set("value", 1);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(error_msgs,
                ElementsAre("https://example.org/script.js:12 Uncaught "
                            "TypeError: Invalid bucket dictionary."));

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }

  // Invalid bucket dictionary, whose offset is not a BigInt.
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary bucket_dict =
        gin::Dictionary::CreateEmpty(helper_->isolate());
    bucket_dict.Set("baseValue", std::string("winningBid"));
    bucket_dict.Set("offset", 255);

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", bucket_dict);
    dict.Set("value", 1);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(error_msgs,
                ElementsAre("https://example.org/script.js:12 Uncaught "
                            "TypeError: Invalid bucket dictionary."));

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }

  // Dictionary value
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary value_dict =
        gin::Dictionary::CreateEmpty(helper_->isolate());
    value_dict.Set("baseValue", std::string("winningBid"));
    value_dict.Set("scale", 2);
    value_dict.Set("offset", -5);

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", std::string("1596"));
    dict.Set("value", value_dict);

    auto signal_value = auction_worklet::mojom::SignalValue::New(
        /*base_value=*/auction_worklet::mojom::BaseValue::kWinningBid,
        /*scale=*/2,
        /*offset=*/-5);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(error_msgs, ElementsAre());

    auction_worklet::mojom::AggregatableReportForEventContribution
        expected_contribution(
            /*bucket=*/auction_worklet::mojom::ForEventSignalBucket::
                NewIdBucket(1596),
            /*value=*/
            auction_worklet::mojom::ForEventSignalValue::NewSignalValue(
                std::move(signal_value)),
            /*event_type=*/kReservedWin);

    ExpectOneForEventRequestEqualTo(
        context_recycler.private_aggregation_bindings()
            ->TakePrivateAggregationRequests(),
        expected_contribution.Clone());
  }

  // Invalid value dictionary, which has no base_value key
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary value_dict =
        gin::Dictionary::CreateEmpty(helper_->isolate());
    value_dict.Set("offset", 255);

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", std::string("1"));
    dict.Set("value", value_dict);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(error_msgs,
                ElementsAre("https://example.org/script.js:12 Uncaught "
                            "TypeError: Invalid value dictionary."));

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }

  // Non BigInt or dictionary bucket
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", 12.3);
    dict.Set("value", 45);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(
        error_msgs,
        ElementsAre("https://example.org/script.js:12 Uncaught TypeError: "
                    "Bucket must be a BigInt or a dictionary."));

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }

  // Non integer or dictionary value
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", std::string("123"));
    dict.Set("value", 4.5);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(
        error_msgs,
        ElementsAre("https://example.org/script.js:12 Uncaught TypeError: "
                    "Value must be an integer or a dictionary."));

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }

  // Negative bucket
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", std::string("-1"));
    dict.Set("value", 45);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(
        error_msgs,
        ElementsAre("https://example.org/script.js:12 Uncaught TypeError: "
                    "BigInt must be non-negative."));

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }

  // Negative value
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", std::string("123"));
    dict.Set("value", -1);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(
        error_msgs,
        ElementsAre("https://example.org/script.js:12 Uncaught TypeError: "
                    "Value must be non-negative."));

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }

  // Missing bucket
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("value", 45);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(
        error_msgs,
        ElementsAre("https://example.org/script.js:12 Uncaught TypeError: "
                    "Invalid or missing bucket in reportContributionForEvent's "
                    "argument."));

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }

  // Missing value
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", 123);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(
        error_msgs,
        ElementsAre("https://example.org/script.js:12 Uncaught TypeError: "
                    "Invalid or missing value in reportContributionForEvent's "
                    "argument."));

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }

  // API not called
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());

    Run(scope, script, "doNothing", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(error_msgs, ElementsAre());

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }
}

class ContextRecyclerPrivateAggregationDisabledTest
    : public ContextRecyclerTest {
 public:
  ContextRecyclerPrivateAggregationDisabledTest() {
    scoped_feature_list_.InitAndDisableFeature(content::kPrivateAggregationApi);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Exercise PrivateAggregationBindings, and make sure they reset properly.
TEST_F(ContextRecyclerPrivateAggregationDisabledTest,
       PrivateAggregationBindings) {
  const char kScript[] = R"(
    function test(args) {
      // Passing BigInts in directly is complicated so we construct them from
      // strings.
      if (typeof args.bucket === "string") {
        args.bucket = BigInt(args.bucket);
      }
      privateAggregation.sendHistogramReport(args);
    }
  )";

  v8::Local<v8::UnboundScript> script = Compile(kScript);
  ASSERT_FALSE(script.IsEmpty());

  ContextRecycler context_recycler(helper_.get());
  context_recycler.AddPrivateAggregationBindings(
      /*private_aggregation_permissions_policy_allowed=*/true);

  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", std::string("123"));
    dict.Set("value", 45);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(
        error_msgs,
        ElementsAre("https://example.org/script.js:8 Uncaught ReferenceError: "
                    "privateAggregation is not defined."));

    ASSERT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }
}

class ContextRecyclerPrivateAggregationDisabledForFledgeOnlyTest
    : public ContextRecyclerTest {
 public:
  ContextRecyclerPrivateAggregationDisabledForFledgeOnlyTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        content::kPrivateAggregationApi, {{"enabled_in_fledge", "false"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Exercise PrivateAggregationBindings, and make sure they reset properly.
TEST_F(ContextRecyclerPrivateAggregationDisabledForFledgeOnlyTest,
       PrivateAggregationBindings) {
  const char kScript[] = R"(
    function test(args) {
      // Passing BigInts in directly is complicated so we construct them from
      // strings.
      if (typeof args.bucket === "string") {
        args.bucket = BigInt(args.bucket);
      }
      privateAggregation.sendHistogramReport(args);
    }
  )";

  v8::Local<v8::UnboundScript> script = Compile(kScript);
  ASSERT_FALSE(script.IsEmpty());

  ContextRecycler context_recycler(helper_.get());
  context_recycler.AddPrivateAggregationBindings(
      /*private_aggregation_permissions_policy_allowed=*/true);

  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", std::string("123"));
    dict.Set("value", 45);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(
        error_msgs,
        ElementsAre("https://example.org/script.js:8 Uncaught ReferenceError: "
                    "privateAggregation is not defined."));

    ASSERT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }
}

class ContextRecyclerPrivateAggregationOnlyFledgeExtensionsDisabledTest
    : public ContextRecyclerTest {
 public:
  ContextRecyclerPrivateAggregationOnlyFledgeExtensionsDisabledTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        content::kPrivateAggregationApi,
        {{"fledge_extensions_enabled", "false"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Make sure that `reportContributionForEvent()` isn't available, but the other
// `privateAggregation` functions are.
TEST_F(ContextRecyclerPrivateAggregationOnlyFledgeExtensionsDisabledTest,
       PrivateAggregationForEventBindings) {
  using PrivateAggregationRequests =
      std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>;

  const char kScript[] = R"(
    function test(args) {
      // Passing BigInts in directly is complicated so we construct them from
      // strings.
      if (typeof args.bucket === "string") {
        args.bucket = BigInt(args.bucket);
      }
      privateAggregation.sendHistogramReport(args);
      privateAggregation.reportContributionForEvent("example", args);
    }
  )";

  v8::Local<v8::UnboundScript> script = Compile(kScript);
  ASSERT_FALSE(script.IsEmpty());

  ContextRecycler context_recycler(helper_.get());
  context_recycler.AddPrivateAggregationBindings(
      /*private_aggregation_permissions_policy_allowed=*/true);

  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", std::string("123"));
    dict.Set("value", 45);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(
        error_msgs,
        ElementsAre("https://example.org/script.js:9 Uncaught TypeError: "
                    "privateAggregation.reportContributionForEvent is not a "
                    "function."));

    PrivateAggregationRequests pa_requests =
        context_recycler.private_aggregation_bindings()
            ->TakePrivateAggregationRequests();
    ASSERT_EQ(pa_requests.size(), 1u);
  }
}

}  // namespace auction_worklet
