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
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/bidder_lazy_filler.h"
#include "content/services/auction_worklet/for_debugging_only_bindings.h"
#include "content/services/auction_worklet/private_aggregation_bindings.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "content/services/auction_worklet/register_ad_beacon_bindings.h"
#include "content/services/auction_worklet/register_ad_macro_bindings.h"
#include "content/services/auction_worklet/report_bindings.h"
#include "content/services/auction_worklet/set_bid_bindings.h"
#include "content/services/auction_worklet/set_priority_bindings.h"
#include "content/services/auction_worklet/worklet_test_util.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/ad_auction_currencies.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/private_aggregation/aggregatable_report.mojom-shared.h"
#include "third_party/blink/public/mojom/private_aggregation/aggregatable_report.mojom.h"
#include "third_party/blink/public/mojom/private_aggregation/private_aggregation_host.mojom-forward.h"
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
    // Some of the tests fast-forward the mock time by half a second at a time,
    // so give an unreasonably generous time limit.
    time_limit_ = helper_->CreateTimeLimit(base::Seconds(500));
    time_limit_scope_ =
        std::make_unique<AuctionV8Helper::TimeLimitScope>(time_limit_.get());
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
    v8::LocalVector<v8::Value> args(helper_->isolate());
    if (!maybe_arg.IsEmpty())
      args.push_back(maybe_arg);
    if (!helper_->RunScript(scope.GetContext(), script,
                            /*debug_id=*/nullptr, time_limit_.get(),
                            error_msgs)) {
      return {};
    }
    return helper_->CallFunction(
        scope.GetContext(),
        /*debug_id=*/nullptr, helper_->FormatScriptName(script), function_name,
        args, time_limit_.get(), error_msgs);
  }

  // Runs a function with a list of arguments.
  v8::MaybeLocal<v8::Value> Run(ContextRecyclerScope& scope,
                                v8::Local<v8::UnboundScript> script,
                                const std::string& function_name,
                                std::vector<std::string>& error_msgs,
                                v8::LocalVector<v8::Value> args) {
    if (!helper_->RunScript(scope.GetContext(), script,
                            /*debug_id=*/nullptr, time_limit_.get(),
                            error_msgs)) {
      return {};
    }
    return helper_->CallFunction(
        scope.GetContext(),
        /*debug_id=*/nullptr, helper_->FormatScriptName(script), function_name,
        args, time_limit_.get(), error_msgs);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<AuctionV8Helper> helper_;
  std::unique_ptr<AuctionV8Helper::FullIsolateScope> v8_scope_;
  std::unique_ptr<AuctionV8Helper::TimeLimit> time_limit_;
  std::unique_ptr<AuctionV8Helper::TimeLimitScope> time_limit_scope_;
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
  {
    ContextRecyclerScope scope(context_recycler);  // Initialize context
    context_recycler.AddForDebuggingOnlyBindings();
  }

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
  {
    ContextRecyclerScope scope(context_recycler);  // Initialize context
    context_recycler.AddRegisterAdBeaconBindings();
  }

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
  {
    ContextRecyclerScope scope(context_recycler);  // Initialize context
    context_recycler.AddReportBindings();
  }

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
  {
    ContextRecyclerScope scope(context_recycler);  // Initialize context
    context_recycler.AddSetBidBindings();
  }

  base::RepeatingCallback<bool(const GURL&)> matches_ad1 = base::BindRepeating(
      [](const GURL& url) { return url == GURL("https://example.com/ad1"); });
  base::RepeatingCallback<bool(const GURL&)> ignore_arg_return_false =
      base::BindRepeating([](const GURL& ignored) { return false; });

  {
    mojom::BidderWorkletNonSharedParamsPtr params =
        mojom::BidderWorkletNonSharedParams::New();
    ContextRecyclerScope scope(context_recycler);
    params->ads.emplace();
    params->ads.value().emplace_back(GURL("https://example.com/ad1"),
                                     absl::nullopt);
    context_recycler.set_bid_bindings()->ReInitialize(
        base::TimeTicks::Now(),
        /*has_top_level_seller_origin=*/false, params.get(),
        /*per_buyer_currency=*/absl::nullopt,
        /*is_ad_excluded=*/ignore_arg_return_false,
        /*is_component_ad_excluded=*/ignore_arg_return_false);

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
    EXPECT_EQ("https://example.com/ad1", bid->ad_descriptor.url);
    EXPECT_EQ(10.0, bid->bid);
    EXPECT_EQ(base::Milliseconds(500), bid->bid_duration);
    EXPECT_EQ(mojom::RejectReason::kNotAvailable,
              context_recycler.set_bid_bindings()->reject_reason());
  }

  {
    // Different ad objects get taken into account.
    mojom::BidderWorkletNonSharedParamsPtr params =
        mojom::BidderWorkletNonSharedParams::New();
    ContextRecyclerScope scope(context_recycler);
    params->ads.emplace();
    params->ads.value().emplace_back(GURL("https://example.com/notad1"),
                                     absl::nullopt);

    context_recycler.set_bid_bindings()->ReInitialize(
        base::TimeTicks::Now(),
        /*has_top_level_seller_origin=*/false, params.get(),
        /*per_buyer_currency=*/absl::nullopt,
        /*is_ad_excluded=*/ignore_arg_return_false,
        /*is_component_ad_excluded=*/ignore_arg_return_false);

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
    EXPECT_EQ(mojom::RejectReason::kNotAvailable,
              context_recycler.set_bid_bindings()->reject_reason());
  }

  {
    mojom::BidderWorkletNonSharedParamsPtr params =
        mojom::BidderWorkletNonSharedParams::New();
    // Some components, and in a nested auction, w/o permission.
    ContextRecyclerScope scope(context_recycler);
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
        /*per_buyer_currency=*/absl::nullopt,
        /*is_ad_excluded=*/ignore_arg_return_false,
        /*is_component_ad_excluded=*/ignore_arg_return_false);

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
    mojom::BidderWorkletNonSharedParamsPtr params =
        mojom::BidderWorkletNonSharedParams::New();
    ContextRecyclerScope scope(context_recycler);
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
        /*per_buyer_currency=*/absl::nullopt,
        /*is_ad_excluded=*/ignore_arg_return_false,
        /*is_component_ad_excluded=*/ignore_arg_return_false);

    task_environment_.FastForwardBy(base::Milliseconds(200));

    gin::Dictionary bid_dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    bid_dict.Set("render", std::string("https://example.com/ad5"));
    bid_dict.Set("bid", 15.0);
    bid_dict.Set("allowComponentAuction", true);
    v8::LocalVector<v8::Value> components(helper_->isolate());
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
    EXPECT_EQ("https://example.com/ad5", bid->ad_descriptor.url);
    EXPECT_EQ(15.0, bid->bid);
    EXPECT_EQ(base::Milliseconds(200), bid->bid_duration);
    ASSERT_TRUE(bid->ad_component_descriptors.has_value());
    EXPECT_THAT(
        bid->ad_component_descriptors.value(),
        ElementsAre(blink::AdDescriptor(GURL("https://example.com/portion3")),
                    blink::AdDescriptor(GURL("https://example.com/portion5"))));
  }

  {
    // Wrong components.
    mojom::BidderWorkletNonSharedParamsPtr params =
        mojom::BidderWorkletNonSharedParams::New();
    ContextRecyclerScope scope(context_recycler);
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
        /*per_buyer_currency=*/absl::nullopt,
        /*is_ad_excluded=*/ignore_arg_return_false,
        /*is_component_ad_excluded=*/ignore_arg_return_false);

    task_environment_.FastForwardBy(base::Milliseconds(200));

    gin::Dictionary bid_dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    bid_dict.Set("render", std::string("https://example.com/ad5"));
    bid_dict.Set("bid", 15.0);
    v8::LocalVector<v8::Value> components(helper_->isolate());
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
    // use ad filter function - ads excluded.
    mojom::BidderWorkletNonSharedParamsPtr params =
        mojom::BidderWorkletNonSharedParams::New();
    ContextRecyclerScope scope(context_recycler);
    params->ads.emplace();
    params->ads.value().emplace_back(GURL("https://example.com/ad1"),
                                     absl::nullopt);

    context_recycler.set_bid_bindings()->ReInitialize(
        base::TimeTicks::Now(),
        /*has_top_level_seller_origin=*/false, params.get(),
        /*per_buyer_currency=*/absl::nullopt,
        /*is_ad_excluded=*/matches_ad1,
        /*is_component_ad_excluded=*/matches_ad1);

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
    // use ad filter function - ads permitted.
    mojom::BidderWorkletNonSharedParamsPtr params =
        mojom::BidderWorkletNonSharedParams::New();
    ContextRecyclerScope scope(context_recycler);
    params->ads.emplace();
    params->ads.value().emplace_back(GURL("https://example.com/ad2"),
                                     absl::nullopt);

    context_recycler.set_bid_bindings()->ReInitialize(
        base::TimeTicks::Now(),
        /*has_top_level_seller_origin=*/false, params.get(),
        /*per_buyer_currency=*/absl::nullopt,
        /*is_ad_excluded=*/matches_ad1,
        /*is_component_ad_excluded=*/matches_ad1);

    task_environment_.FastForwardBy(base::Milliseconds(500));

    gin::Dictionary bid_dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    bid_dict.Set("render", std::string("https://example.com/ad2"));
    bid_dict.Set("bid", 10.0);

    std::vector<std::string> error_msgs;
    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), bid_dict));

    EXPECT_THAT(error_msgs, ElementsAre());
    ASSERT_TRUE(context_recycler.set_bid_bindings()->has_bid());
    mojom::BidderWorkletBidPtr bid =
        context_recycler.set_bid_bindings()->TakeBid();
    EXPECT_EQ("https://example.com/ad2", bid->ad_descriptor.url);
    EXPECT_EQ(10.0, bid->bid);
    EXPECT_EQ(base::Milliseconds(500), bid->bid_duration);
  }

  {
    // Bid currency --- expect USD.
    mojom::BidderWorkletNonSharedParamsPtr params =
        mojom::BidderWorkletNonSharedParams::New();
    ContextRecyclerScope scope(context_recycler);
    params->ads.emplace();
    params->ads.value().emplace_back(GURL("https://example.com/ad2"),
                                     absl::nullopt);

    context_recycler.set_bid_bindings()->ReInitialize(
        base::TimeTicks::Now(),
        /*has_top_level_seller_origin=*/false, params.get(),
        blink::AdCurrency::From("USD"),
        /*is_ad_excluded=*/matches_ad1,
        /*is_component_ad_excluded=*/matches_ad1);

    gin::Dictionary bid_dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    bid_dict.Set("render", std::string("https://example.com/ad2"));
    bid_dict.Set("bid", 10.0);
    bid_dict.Set("bidCurrency", std::string("USD"));

    std::vector<std::string> error_msgs;
    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), bid_dict));

    EXPECT_THAT(error_msgs, ElementsAre());
    ASSERT_TRUE(context_recycler.set_bid_bindings()->has_bid());
    mojom::BidderWorkletBidPtr bid =
        context_recycler.set_bid_bindings()->TakeBid();
    EXPECT_EQ("https://example.com/ad2", bid->ad_descriptor.url);
    EXPECT_EQ(10.0, bid->bid);
    ASSERT_TRUE(bid->bid_currency.has_value());
    EXPECT_EQ("USD", bid->bid_currency->currency_code());
    EXPECT_EQ(mojom::RejectReason::kNotAvailable,
              context_recycler.set_bid_bindings()->reject_reason());
  }

  {
    // Bid currency --- expect CAD.
    mojom::BidderWorkletNonSharedParamsPtr params =
        mojom::BidderWorkletNonSharedParams::New();
    ContextRecyclerScope scope(context_recycler);
    params->ads.emplace();
    params->ads.value().emplace_back(GURL("https://example.com/ad2"),
                                     absl::nullopt);

    context_recycler.set_bid_bindings()->ReInitialize(
        base::TimeTicks::Now(),
        /*has_top_level_seller_origin=*/false, params.get(),
        blink::AdCurrency::From("CAD"),
        /*is_ad_excluded=*/matches_ad1,
        /*is_component_ad_excluded=*/matches_ad1);

    gin::Dictionary bid_dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    bid_dict.Set("render", std::string("https://example.com/ad2"));
    bid_dict.Set("bid", 10.0);
    bid_dict.Set("bidCurrency", std::string("USD"));

    std::vector<std::string> error_msgs;
    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), bid_dict));

    EXPECT_THAT(
        error_msgs,
        ElementsAre(
            "https://example.org/script.js:3 Uncaught TypeError: bidCurrency "
            "mismatch; returned 'USD', expected 'CAD'."));
    EXPECT_FALSE(context_recycler.set_bid_bindings()->has_bid());
    EXPECT_EQ(mojom::RejectReason::kWrongGenerateBidCurrency,
              context_recycler.set_bid_bindings()->reject_reason());
  }

  {
    // Make sure the reject reason doesn't latch.
    mojom::BidderWorkletNonSharedParamsPtr params =
        mojom::BidderWorkletNonSharedParams::New();
    ContextRecyclerScope scope(context_recycler);
    params->ads.emplace();
    params->ads.value().emplace_back(GURL("https://example.com/ad2"),
                                     absl::nullopt);

    context_recycler.set_bid_bindings()->ReInitialize(
        base::TimeTicks::Now(),
        /*has_top_level_seller_origin=*/false, params.get(),
        blink::AdCurrency::From("CAD"),
        /*is_ad_excluded=*/matches_ad1,
        /*is_component_ad_excluded=*/matches_ad1);

    gin::Dictionary bid_dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    bid_dict.Set("render", std::string("https://example.com/ad2"));
    bid_dict.Set("bid", 10.0);
    bid_dict.Set("bidCurrency", std::string("CAD"));

    std::vector<std::string> error_msgs;
    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), bid_dict));

    EXPECT_THAT(error_msgs, ElementsAre());
    ASSERT_TRUE(context_recycler.set_bid_bindings()->has_bid());
    mojom::BidderWorkletBidPtr bid =
        context_recycler.set_bid_bindings()->TakeBid();
    EXPECT_EQ("https://example.com/ad2", bid->ad_descriptor.url);
    EXPECT_EQ(10.0, bid->bid);
    ASSERT_TRUE(bid->bid_currency.has_value());
    EXPECT_EQ("CAD", bid->bid_currency->currency_code());
    EXPECT_EQ(mojom::RejectReason::kNotAvailable,
              context_recycler.set_bid_bindings()->reject_reason());
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
  {
    ContextRecyclerScope scope(context_recycler);  // Initialize context
    context_recycler.AddSetPriorityBindings();
  }

  {
    // Make sure an exception doesn't stick around between executions.
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;
    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), std::string("not-a-priority")));
    EXPECT_THAT(
        error_msgs,
        ElementsAre("https://example.org/script.js:3 Uncaught TypeError: "
                    "setPriority(): Converting argument 'priority' to a Number "
                    "did not produce a finite double."));
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
  {
    ContextRecyclerScope scope(context_recycler);  // Initialize context
    context_recycler.AddInterestGroupLazyFiller();
    context_recycler.AddBiddingBrowserSignalsLazyFiller();
  }

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
        "\"prevWins\":[[240,[\"d\"]],[180,[\"c\"]]],"
        "\"prevWinsMs\":[[240000,[\"d\"]],[180000,[\"c\"]]]}",
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
  {
    ContextRecyclerScope scope(context_recycler);  // Initialize context
    context_recycler.AddInterestGroupLazyFiller();
    context_recycler.AddBiddingBrowserSignalsLazyFiller();
  }

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
        "\"prevWins\":[],"
        "\"prevWinsMs\":[]}",
        str_result);
  }
}

TEST_F(ContextRecyclerTest, SharedStorageMethods) {
  using RequestType =
      auction_worklet::TestAuctionSharedStorageHost::RequestType;
  using Request = auction_worklet::TestAuctionSharedStorageHost::Request;

  const std::string kInvalidValue(
      static_cast<size_t>(
          blink::features::kMaxSharedStorageStringLength.Get()) +
          1,
      '*');

  const char kScript[] = R"(
    function testSet(...args) {
      sharedStorage.set(...args);
    }

    function testAppend(...args) {
      sharedStorage.append(...args);
    }

    function testDelete(...args) {
      sharedStorage.delete(...args);
    }

    function testClear(...args) {
      sharedStorage.clear(...args);
    }
  )";

  v8::Local<v8::UnboundScript> script = Compile(kScript);
  ASSERT_FALSE(script.IsEmpty());

  auction_worklet::TestAuctionSharedStorageHost test_shared_storage_host;

  ContextRecycler context_recycler(helper_.get());
  {
    ContextRecyclerScope scope(context_recycler);  // Initialize context
    context_recycler.AddSharedStorageBindings(
        &test_shared_storage_host,
        /*shared_storage_permissions_policy_allowed=*/true);
  }

  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    Run(scope, script, "testSet", error_msgs,
        /*args=*/
        v8::LocalVector<v8::Value>(
            helper_->isolate(),
            {gin::ConvertToV8(helper_->isolate(), std::string("a")),
             gin::ConvertToV8(helper_->isolate(), std::string("b"))}));
    EXPECT_THAT(error_msgs, ElementsAre());

    EXPECT_THAT(test_shared_storage_host.observed_requests(),
                ElementsAre(Request{.type = RequestType::kSet,
                                    .key = u"a",
                                    .value = u"b",
                                    .ignore_if_present = false}));

    test_shared_storage_host.ClearObservedRequests();
  }

  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary options_dict =
        gin::Dictionary::CreateEmpty(helper_->isolate());
    options_dict.Set("ignoreIfPresent", true);

    Run(scope, script, "testSet", error_msgs,
        /*args=*/
        v8::LocalVector<v8::Value>(
            helper_->isolate(),
            {gin::ConvertToV8(helper_->isolate(), std::string("a")),
             gin::ConvertToV8(helper_->isolate(), std::string("b")),
             gin::ConvertToV8(helper_->isolate(), options_dict)}));
    EXPECT_THAT(error_msgs, ElementsAre());

    EXPECT_THAT(test_shared_storage_host.observed_requests(),
                ElementsAre(Request{.type = RequestType::kSet,
                                    .key = u"a",
                                    .value = u"b",
                                    .ignore_if_present = true}));

    test_shared_storage_host.ClearObservedRequests();
  }

  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    Run(scope, script, "testAppend", error_msgs,
        /*args=*/
        v8::LocalVector<v8::Value>(
            helper_->isolate(),
            {gin::ConvertToV8(helper_->isolate(), std::string("a")),
             gin::ConvertToV8(helper_->isolate(), std::string("b"))}));
    EXPECT_THAT(error_msgs, ElementsAre());

    EXPECT_THAT(test_shared_storage_host.observed_requests(),
                ElementsAre(Request{.type = RequestType::kAppend,
                                    .key = u"a",
                                    .value = u"b",
                                    .ignore_if_present = false}));

    test_shared_storage_host.ClearObservedRequests();
  }

  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    Run(scope, script, "testDelete", error_msgs,
        /*args=*/
        v8::LocalVector<v8::Value>(
            helper_->isolate(),
            {gin::ConvertToV8(helper_->isolate(), std::string("a"))}));
    EXPECT_THAT(error_msgs, ElementsAre());

    EXPECT_THAT(test_shared_storage_host.observed_requests(),
                ElementsAre(Request{.type = RequestType::kDelete,
                                    .key = u"a",
                                    .value = std::u16string(),
                                    .ignore_if_present = false}));

    test_shared_storage_host.ClearObservedRequests();
  }

  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    Run(scope, script, "testClear", error_msgs,
        /*args=*/
        v8::LocalVector<v8::Value>(
            helper_->isolate(),
            {gin::ConvertToV8(helper_->isolate(), std::string("a"))}));
    EXPECT_THAT(error_msgs, ElementsAre());

    EXPECT_THAT(test_shared_storage_host.observed_requests(),
                ElementsAre(Request{.type = RequestType::kClear,
                                    .key = std::u16string(),
                                    .value = std::u16string(),
                                    .ignore_if_present = false}));

    test_shared_storage_host.ClearObservedRequests();
  }

  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    Run(scope, script, "testSet", error_msgs,
        /*args=*/v8::LocalVector<v8::Value>(helper_->isolate()));
    EXPECT_THAT(
        error_msgs,
        ElementsAre(
            "https://example.org/script.js:3 Uncaught TypeError: "
            "sharedStorage.set(): at least 2 argument(s) are required."));
  }

  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    Run(scope, script, "testSet", error_msgs, /*args=*/
        v8::LocalVector<v8::Value>(
            helper_->isolate(),
            {gin::ConvertToV8(helper_->isolate(), std::string("a"))}));
    EXPECT_THAT(
        error_msgs,
        ElementsAre(
            "https://example.org/script.js:3 Uncaught TypeError: "
            "sharedStorage.set(): at least 2 argument(s) are required."));
  }

  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    Run(scope, script, "testSet", error_msgs, /*args=*/
        v8::LocalVector<v8::Value>(
            helper_->isolate(),
            {gin::ConvertToV8(helper_->isolate(), std::string("a")),
             gin::ConvertToV8(helper_->isolate(), std::string("b")),
             gin::ConvertToV8(helper_->isolate(), true)}));
    EXPECT_THAT(
        error_msgs,
        ElementsAre("https://example.org/script.js:3 Uncaught TypeError: "
                    "sharedStorage.set 'options' argument "
                    "Value passed as dictionary is neither object, null, nor "
                    "undefined."));
  }

  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    Run(scope, script, "testSet", error_msgs, /*args=*/
        v8::LocalVector<v8::Value>(
            helper_->isolate(),
            {gin::ConvertToV8(helper_->isolate(), std::string("")),
             gin::ConvertToV8(helper_->isolate(), std::string("b"))}));
    EXPECT_THAT(
        error_msgs,
        ElementsAre("https://example.org/script.js:3 Uncaught TypeError: "
                    "Invalid 'key' argument in sharedStorage.set()."));
  }

  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    Run(scope, script, "testSet", error_msgs, /*args=*/
        v8::LocalVector<v8::Value>(
            helper_->isolate(),
            {gin::ConvertToV8(helper_->isolate(), std::string("a")),
             gin::ConvertToV8(helper_->isolate(), kInvalidValue)}));
    EXPECT_THAT(
        error_msgs,
        ElementsAre("https://example.org/script.js:3 Uncaught TypeError: "
                    "Invalid 'value' argument in sharedStorage.set()."));
  }

  // This shows that if there is a semantic error in argument 0 and a type error
  // in argument 2 the type error is what's reported.
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    Run(scope, script, "testSet", error_msgs, /*args=*/
        v8::LocalVector<v8::Value>(
            helper_->isolate(),
            {gin::ConvertToV8(helper_->isolate(), std::string("")),
             gin::ConvertToV8(helper_->isolate(), std::string("b")),
             gin::ConvertToV8(helper_->isolate(), true)}));
    EXPECT_THAT(
        error_msgs,
        ElementsAre("https://example.org/script.js:3 Uncaught TypeError: "
                    "sharedStorage.set 'options' argument "
                    "Value passed as dictionary is neither object, null, nor "
                    "undefined."));
  }

  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    Run(scope, script, "testAppend", error_msgs,
        /*args=*/v8::LocalVector<v8::Value>(helper_->isolate()));
    EXPECT_THAT(
        error_msgs,
        ElementsAre(
            "https://example.org/script.js:7 Uncaught TypeError: "
            "sharedStorage.append(): at least 2 argument(s) are required."));
  }

  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    Run(scope, script, "testAppend", error_msgs, /*args=*/
        v8::LocalVector<v8::Value>(
            helper_->isolate(),
            {gin::ConvertToV8(helper_->isolate(), std::string("a"))}));
    EXPECT_THAT(
        error_msgs,
        ElementsAre(
            "https://example.org/script.js:7 Uncaught TypeError: "
            "sharedStorage.append(): at least 2 argument(s) are required."));
  }

  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    Run(scope, script, "testAppend", error_msgs, /*args=*/
        v8::LocalVector<v8::Value>(
            helper_->isolate(),
            {gin::ConvertToV8(helper_->isolate(), std::string("")),
             gin::ConvertToV8(helper_->isolate(), std::string("b"))}));
    EXPECT_THAT(
        error_msgs,
        ElementsAre("https://example.org/script.js:7 Uncaught TypeError: "
                    "Invalid 'key' argument in sharedStorage.append()."));
  }

  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    Run(scope, script, "testAppend", error_msgs, /*args=*/
        v8::LocalVector<v8::Value>(
            helper_->isolate(),
            {gin::ConvertToV8(helper_->isolate(), std::string("a")),
             gin::ConvertToV8(helper_->isolate(), kInvalidValue)}));
    EXPECT_THAT(
        error_msgs,
        ElementsAre("https://example.org/script.js:7 Uncaught TypeError: "
                    "Invalid 'value' argument in sharedStorage.append()."));
  }

  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    Run(scope, script, "testDelete", error_msgs,
        /*args=*/v8::LocalVector<v8::Value>(helper_->isolate()));
    EXPECT_THAT(
        error_msgs,
        ElementsAre(
            "https://example.org/script.js:11 Uncaught TypeError: "
            "sharedStorage.delete(): at least 1 argument(s) are required."));
  }

  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    Run(scope, script, "testDelete", error_msgs, /*args=*/
        v8::LocalVector<v8::Value>(
            helper_->isolate(),
            {gin::ConvertToV8(helper_->isolate(), std::string(""))}));
    EXPECT_THAT(
        error_msgs,
        ElementsAre("https://example.org/script.js:11 Uncaught TypeError: "
                    "Invalid 'key' argument in sharedStorage.delete()."));
  }
}

TEST_F(ContextRecyclerTest, SharedStorageMethodsPermissionsPolicyDisabled) {
  const char kScript[] = R"(
    function testSet(...args) {
      sharedStorage.set(...args);
    }

    function testAppend(...args) {
      sharedStorage.append(...args);
    }

    function testDelete(...args) {
      sharedStorage.delete(...args);
    }

    function testClear(...args) {
      sharedStorage.clear(...args);
    }
  )";

  v8::Local<v8::UnboundScript> script = Compile(kScript);
  ASSERT_FALSE(script.IsEmpty());

  ContextRecycler context_recycler(helper_.get());
  {
    ContextRecyclerScope scope(context_recycler);  // Initialize context
    context_recycler.AddSharedStorageBindings(
        nullptr,
        /*shared_storage_permissions_policy_allowed=*/false);
  }

  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    Run(scope, script, "testSet", error_msgs,
        /*args=*/v8::LocalVector<v8::Value>(helper_->isolate()));
    EXPECT_THAT(error_msgs,
                ElementsAre("https://example.org/script.js:3 Uncaught "
                            "TypeError: The \"shared-storage\" Permissions "
                            "Policy denied the method on sharedStorage."));
  }

  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    Run(scope, script, "testAppend", error_msgs,
        /*args=*/v8::LocalVector<v8::Value>(helper_->isolate()));
    EXPECT_THAT(error_msgs,
                ElementsAre("https://example.org/script.js:7 Uncaught "
                            "TypeError: The \"shared-storage\" Permissions "
                            "Policy denied the method on sharedStorage."));
  }

  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    Run(scope, script, "testDelete", error_msgs,
        /*args=*/v8::LocalVector<v8::Value>(helper_->isolate()));
    EXPECT_THAT(error_msgs,
                ElementsAre("https://example.org/script.js:11 Uncaught "
                            "TypeError: The \"shared-storage\" Permissions "
                            "Policy denied the method on sharedStorage."));
  }

  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    Run(scope, script, "testClear", error_msgs,
        /*args=*/v8::LocalVector<v8::Value>(helper_->isolate()));
    EXPECT_THAT(error_msgs,
                ElementsAre("https://example.org/script.js:15 Uncaught "
                            "TypeError: The \"shared-storage\" Permissions "
                            "Policy denied the method on sharedStorage."));
  }
}

class ContextRecyclerPrivateAggregationEnabledTest
    : public ContextRecyclerTest {
 public:
  ContextRecyclerPrivateAggregationEnabledTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kPrivateAggregationApi);
  }

  // Wraps a debug_key into the appropriate dictionary. Templated to allow both
  // integers and strings.
  template <typename T>
  v8::Local<v8::Value> WrapDebugKey(T debug_key) {
    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("debugKey", debug_key);
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
      absl::optional<blink::mojom::DebugKeyPtr> debug_key = absl::nullopt) {
    blink::mojom::AggregatableReportHistogramContribution expected_contribution(
        bucket, value);

    blink::mojom::DebugModeDetailsPtr debug_mode_details;
    if (debug_key.has_value()) {
      debug_mode_details = blink::mojom::DebugModeDetails::New(
          /*is_enabled=*/true,
          /*debug_key=*/std::move(debug_key.value()));
    } else {
      debug_mode_details = blink::mojom::DebugModeDetails::New();
    }

    auction_worklet::mojom::PrivateAggregationRequest expected_request(
        auction_worklet::mojom::AggregatableReportContribution::
            NewHistogramContribution(expected_contribution.Clone()),
        blink::mojom::AggregationServiceMode::kDefault,
        std::move(debug_mode_details));

    ASSERT_EQ(pa_requests.size(), 1u);
    EXPECT_EQ(pa_requests[0], expected_request.Clone());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Exercise `contributeToHistogram()` of PrivateAggregationBindings, and make
// sure they reset properly.
TEST_F(ContextRecyclerPrivateAggregationEnabledTest,
       PrivateAggregationBindingsContributeToHistogram) {
  using PrivateAggregationRequests =
      std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>;

  const char kScript[] = R"(
    function test(args) {
      // Passing BigInts in directly is complicated so we construct them from
      // strings.
      if (typeof args.bucket === "string") {
        args.bucket = BigInt(args.bucket);
      }
      privateAggregation.contributeToHistogram(args);
    }
    function doNothing() {}
  )";

  v8::Local<v8::UnboundScript> script = Compile(kScript);
  ASSERT_FALSE(script.IsEmpty());

  ContextRecycler context_recycler(helper_.get());
  {
    ContextRecyclerScope scope(context_recycler);  // Initialize context
    context_recycler.AddPrivateAggregationBindings(
        /*private_aggregation_permissions_policy_allowed=*/true);
  }

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

  // Non-integer Number value (is converted to integer)
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", std::string("123"));
    dict.Set("value", 4.5);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(error_msgs, ElementsAre());

    ExpectOneHistogramRequestEqualTo(
        context_recycler.private_aggregation_bindings()
            ->TakePrivateAggregationRequests(),
        /*bucket=*/123, /*value=*/4);
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

    blink::mojom::AggregatableReportHistogramContribution
        expected_contribution_1(/*bucket=*/123, /*value=*/45);
    auction_worklet::mojom::PrivateAggregationRequest expected_request_1(
        auction_worklet::mojom::AggregatableReportContribution::
            NewHistogramContribution(expected_contribution_1.Clone()),
        blink::mojom::AggregationServiceMode::kDefault,
        blink::mojom::DebugModeDetails::New());

    blink::mojom::AggregatableReportHistogramContribution
        expected_contribution_2(/*bucket=*/678, /*value=*/90);
    auction_worklet::mojom::PrivateAggregationRequest expected_request_2(
        auction_worklet::mojom::AggregatableReportContribution::
            NewHistogramContribution(expected_contribution_2.Clone()),
        blink::mojom::AggregationServiceMode::kDefault,
        blink::mojom::DebugModeDetails::New());

    PrivateAggregationRequests pa_requests =
        context_recycler.private_aggregation_bindings()
            ->TakePrivateAggregationRequests();
    ASSERT_EQ(pa_requests.size(), 2u);
    EXPECT_EQ(pa_requests[0], expected_request_1.Clone());
    EXPECT_EQ(pa_requests[1], expected_request_2.Clone());
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
    EXPECT_THAT(error_msgs,
                ElementsAre("https://example.org/script.js:8 Uncaught "
                            "TypeError: Cannot convert 123 to a BigInt."));

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
        ElementsAre("https://example.org/script.js:8 Uncaught TypeError: "
                    "privateAggregation.contributeToHistogram() 'contribution' "
                    "argument: Required field 'bucket' is undefined."));

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
        ElementsAre("https://example.org/script.js:8 Uncaught TypeError: "
                    "privateAggregation.contributeToHistogram() 'contribution' "
                    "argument: Required field 'value' is undefined."));

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
      privateAggregation.contributeToHistogram(args);
    }
    function enableDebugMode(arg) {
      if (arg === undefined) {
        privateAggregation.enableDebugMode();
        return;
      }

      // Passing BigInts in directly is complicated so we construct them from
      // strings.
      if (typeof arg.debugKey === "string") {
        arg.debugKey = BigInt(arg.debugKey);
      }
      privateAggregation.enableDebugMode(arg);
    }
  )";

  v8::Local<v8::UnboundScript> script = Compile(kScript);
  ASSERT_FALSE(script.IsEmpty());

  ContextRecycler context_recycler(helper_.get());
  {
    ContextRecyclerScope scope(context_recycler);  // Initialize context
    context_recycler.AddPrivateAggregationBindings(
        /*private_aggregation_permissions_policy_allowed=*/true);
  }

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
        /*debug_key=*/blink::mojom::DebugKey::New(1234u));
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
        blink::mojom::DebugKey::New(std::numeric_limits<uint64_t>::max()));
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
    EXPECT_THAT(error_msgs,
                ElementsAre("https://example.org/script.js:21 Uncaught "
                            "TypeError: Cannot convert 1234 to a BigInt."));

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
        ElementsAre(
            "https://example.org/script.js:21 Uncaught TypeError: "
            "privateAggregation.enableDebugMode() 'options' argument: Value "
            "passed as dictionary is neither object, null, nor undefined."));

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
        /*debug_key=*/blink::mojom::DebugKey::New(1234u));
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
        /*debug_key=*/blink::mojom::DebugKey::New(1234u));
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

    blink::mojom::AggregatableReportHistogramContribution
        expected_contribution_1(/*bucket=*/123, /*value=*/45);
    auction_worklet::mojom::PrivateAggregationRequest expected_request_1(
        auction_worklet::mojom::AggregatableReportContribution::
            NewHistogramContribution(expected_contribution_1.Clone()),
        blink::mojom::AggregationServiceMode::kDefault,
        blink::mojom::DebugModeDetails::New(
            /*is_enabled=*/true,
            /*debug_key=*/blink::mojom::DebugKey::New(1234u)));

    blink::mojom::AggregatableReportHistogramContribution
        expected_contribution_2(/*bucket=*/678, /*value=*/90);
    auction_worklet::mojom::PrivateAggregationRequest expected_request_2(
        auction_worklet::mojom::AggregatableReportContribution::
            NewHistogramContribution(expected_contribution_2.Clone()),
        blink::mojom::AggregationServiceMode::kDefault,
        blink::mojom::DebugModeDetails::New(
            /*is_enabled=*/true,
            /*debug_key=*/blink::mojom::DebugKey::New(1234u)));

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
        blink::features::kPrivateAggregationApi,
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
        blink::mojom::AggregationServiceMode::kDefault,
        blink::mojom::DebugModeDetails::New());
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
        blink::mojom::AggregationServiceMode::kDefault,
        blink::mojom::DebugModeDetails::New());

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
      privateAggregation.contributeToHistogramOnEvent('reserved.win', args);
    }

    function testDifferentEventTypes(args) {
      // Passing BigInts in directly is complicated so we construct them from
      // strings.
      if (typeof args.bucket === "string") {
        args.bucket = BigInt(args.bucket);
      }
      privateAggregation.contributeToHistogramOnEvent('reserved.win', args);
      // Add 1 to value, to let reserved.loss request gets different
      // contribution from reserved.win request.
      args.value += 1;
      privateAggregation.contributeToHistogramOnEvent('reserved.loss', args);
      args.value += 1;
      privateAggregation.contributeToHistogramOnEvent('reserved.always', args);
      args.value += 1;
      // Arbitrary unreserved event type.
      privateAggregation.contributeToHistogramOnEvent('click', args);
    }

    function testMissingEventType(args) {
      if (typeof args.bucket === "string") {
        args.bucket = BigInt(args.bucket);
      }
      privateAggregation.contributeToHistogramOnEvent(args);
    }

    function testMissingContribution() {
      privateAggregation.contributeToHistogramOnEvent('reserved.win');
    }

    function testWrongArgumentsOrder(args) {
      if (typeof args.bucket === "string") {
        args.bucket = BigInt(args.bucket);
      }
      privateAggregation.contributeToHistogramOnEvent(args, 'reserved.win');
    }

    function testInvalidReservedEventType(args) {
      if (typeof args.bucket === "string") {
        args.bucket = BigInt(args.bucket);
      }
      privateAggregation.contributeToHistogramOnEvent(
          "reserved.something", args);
    }

    function doNothing() {}
  )";

  v8::Local<v8::UnboundScript> script = Compile(kScript);
  ASSERT_FALSE(script.IsEmpty());

  ContextRecycler context_recycler(helper_.get());
  {
    ContextRecyclerScope scope(context_recycler);  // Initialize context
    context_recycler.AddPrivateAggregationBindings(
        /*private_aggregation_permissions_policy_allowed=*/true);
  }

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

  // Missing event_type (the first argument) to contributeToHistogramOnEvent()
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
        ElementsAre("https://example.org/script.js:37 Uncaught TypeError: "
                    "privateAggregation.contributeToHistogramOnEvent(): at "
                    "least 2 argument(s) are required."));

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }

  // Missing contribution (the second argument) to
  // contributeToHistogramOnEvent() API.
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());

    Run(scope, script, "testMissingContribution", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(
        error_msgs,
        ElementsAre("https://example.org/script.js:41 Uncaught TypeError: "
                    "privateAggregation.contributeToHistogramOnEvent(): at "
                    "least 2 argument(s) are required."));

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }

  // The two arguments to contributeToHistogramOnEvent() API are in wrong order.
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
        ElementsAre("https://example.org/script.js:48 Uncaught TypeError: "
                    "privateAggregation.contributeToHistogramOnEvent() "
                    "'contribution' argument: Value passed as dictionary is "
                    "neither object, null, nor undefined."));

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
    bucket_dict.Set("baseValue", std::string("bid-reject-reason"));
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
    bucket_dict.Set("baseValue", std::string("bid-reject-reason"));

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

  // Non-integer Number value (is converted to integer)
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", std::string("123"));
    dict.Set("value", 4.5);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(error_msgs, ElementsAre());

    auction_worklet::mojom::AggregatableReportForEventContribution
        expected_contribution(
            /*bucket=*/auction_worklet::mojom::ForEventSignalBucket::
                NewIdBucket(123),
            /*value=*/
            auction_worklet::mojom::ForEventSignalValue::NewIntValue(4),
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
    EXPECT_THAT(
        error_msgs,
        ElementsAre(
            "https://example.org/script.js:12 Uncaught TypeError: "
            "privateAggregation.contributeToHistogramOnEvent() 'contribution' "
            "argument: Required field 'baseValue' is undefined."));

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
                            "TypeError: Bucket's 'baseValue' is invalid."));

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }

  // It's fine that a bucket's scale is a string, since a string can get turned
  // into a Number.
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary bucket_dict =
        gin::Dictionary::CreateEmpty(helper_->isolate());
    bucket_dict.Set("baseValue", std::string("winning-bid"));
    bucket_dict.Set("scale", std::string("255"));

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", bucket_dict);
    dict.Set("value", 1);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(error_msgs, ElementsAre());

    EXPECT_FALSE(context_recycler.private_aggregation_bindings()
                     ->TakePrivateAggregationRequests()
                     .empty());
  }

  // Invalid bucket dictionary, whose scale is a BigInt. That fails since
  // A BigInt isn't going to turn into a Number.
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary bucket_dict =
        gin::Dictionary::CreateEmpty(helper_->isolate());
    bucket_dict.Set("baseValue", std::string("winning-bid"));
    v8::Local<v8::Value> big_int_val = v8::BigInt::New(helper_->isolate(), 255);
    bucket_dict.Set("scale", big_int_val);

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", bucket_dict);
    dict.Set("value", 1);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(
        error_msgs,
        ElementsAre("https://example.org/script.js:12 Uncaught TypeError: "
                    "Cannot convert a BigInt value to a number."));

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }

  // Invalid bucket dictionary, whose scale is NaN.
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary bucket_dict =
        gin::Dictionary::CreateEmpty(helper_->isolate());
    bucket_dict.Set("baseValue", std::string("winningBid"));
    bucket_dict.Set("scale", std::numeric_limits<double>::quiet_NaN());

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", bucket_dict);
    dict.Set("value", 1);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(
        error_msgs,
        ElementsAre("https://example.org/script.js:12 Uncaught TypeError: "
                    "privateAggregation.contributeToHistogramOnEvent() "
                    "'contribution' argument: Converting field 'scale' to a "
                    "Number did not produce a finite double."));

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }

  // Invalid bucket dictionary, whose scale is infinity.
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary bucket_dict =
        gin::Dictionary::CreateEmpty(helper_->isolate());
    bucket_dict.Set("baseValue", std::string("winningBid"));
    bucket_dict.Set("scale", std::numeric_limits<double>::infinity());

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", bucket_dict);
    dict.Set("value", 1);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(
        error_msgs,
        ElementsAre("https://example.org/script.js:12 Uncaught TypeError: "
                    "privateAggregation.contributeToHistogramOnEvent() "
                    "'contribution' argument: Converting field 'scale' to a "
                    "Number did not produce a finite double."));

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
    bucket_dict.Set("baseValue", std::string("winning-bid"));
    bucket_dict.Set("offset", 255);

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", bucket_dict);
    dict.Set("value", 1);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(error_msgs,
                ElementsAre("https://example.org/script.js:12 Uncaught "
                            "TypeError: Bucket's 'offset' must be BigInt."));

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
    value_dict.Set("baseValue", std::string("winning-bid"));
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

  // Invalid value dictionary, which has no baseValue key
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
    EXPECT_THAT(
        error_msgs,
        ElementsAre(
            "https://example.org/script.js:12 Uncaught TypeError: "
            "privateAggregation.contributeToHistogramOnEvent() 'contribution' "
            "argument: Required field 'baseValue' is undefined."));

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }

  // Invalid value dictionary, whose offset is a BigInt, not a 32-bit signed
  // integer.
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary value_dict =
        gin::Dictionary::CreateEmpty(helper_->isolate());
    value_dict.Set("baseValue", std::string("winning-bid"));
    v8::Local<v8::Value> bigint_offset = v8::BigInt::New(helper_->isolate(), 1);
    value_dict.Set("offset", bigint_offset);

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", std::string("1"));
    dict.Set("value", value_dict);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(
        error_msgs,
        ElementsAre("https://example.org/script.js:12 Uncaught TypeError: "
                    "Value's 'offset' must be a 32-bit signed integer."));

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }

  // Invalid value dictionary, whose baseValue is invalid.
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary value_dict =
        gin::Dictionary::CreateEmpty(helper_->isolate());
    value_dict.Set("baseValue", std::string("notValidBaseValue"));

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", std::string("1"));
    dict.Set("value", value_dict);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(error_msgs,
                ElementsAre("https://example.org/script.js:12 Uncaught "
                            "TypeError: Value's 'baseValue' is invalid."));

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
                    "Cannot convert 12.3 to a BigInt."));

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }

  // Non Number or dictionary value. It's fine as long as it can get turned into
  // a Number.
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", std::string("123"));
    dict.Set("value", std::string("4.5"));

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(error_msgs, ElementsAre());

    EXPECT_FALSE(context_recycler.private_aggregation_bindings()
                     ->TakePrivateAggregationRequests()
                     .empty());
  }

  // A BigInt value however will not get turned into a number.
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", std::string("123"));
    v8::Local<v8::Value> big_int_val = v8::BigInt::New(helper_->isolate(), 1);
    dict.Set("value", big_int_val);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(
        error_msgs,
        ElementsAre("https://example.org/script.js:12 Uncaught TypeError: "
                    "Cannot convert a BigInt value to a number."));

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
        ElementsAre(
            "https://example.org/script.js:12 Uncaught TypeError: "
            "privateAggregation.contributeToHistogramOnEvent() 'contribution' "
            "argument: Required field 'bucket' is undefined."));

    EXPECT_TRUE(context_recycler.private_aggregation_bindings()
                    ->TakePrivateAggregationRequests()
                    .empty());
  }

  // Missing value, but bucket being wrong type is noticed first.
  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;

    gin::Dictionary dict = gin::Dictionary::CreateEmpty(helper_->isolate());
    dict.Set("bucket", 123);

    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), dict));
    EXPECT_THAT(error_msgs,
                ElementsAre("https://example.org/script.js:12 Uncaught "
                            "TypeError: Cannot convert 123 to a BigInt."));

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
            "https://example.org/script.js:12 Uncaught TypeError: "
            "privateAggregation.contributeToHistogramOnEvent() 'contribution' "
            "argument: Required field 'value' is undefined."));

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
    scoped_feature_list_.InitAndDisableFeature(
        blink::features::kPrivateAggregationApi);
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
      privateAggregation.contributeToHistogram(args);
    }
  )";

  v8::Local<v8::UnboundScript> script = Compile(kScript);
  ASSERT_FALSE(script.IsEmpty());

  ContextRecycler context_recycler(helper_.get());
  {
    ContextRecyclerScope scope(context_recycler);  // Initialize context
    context_recycler.AddPrivateAggregationBindings(
        /*private_aggregation_permissions_policy_allowed=*/true);
  }

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
        blink::features::kPrivateAggregationApi,
        {{"enabled_in_fledge", "false"}});
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
      privateAggregation.contributeToHistogram(args);
    }
  )";

  v8::Local<v8::UnboundScript> script = Compile(kScript);
  ASSERT_FALSE(script.IsEmpty());

  ContextRecycler context_recycler(helper_.get());
  {
    ContextRecyclerScope scope(context_recycler);  // Initialize context
    context_recycler.AddPrivateAggregationBindings(
        /*private_aggregation_permissions_policy_allowed=*/true);
  }

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
        blink::features::kPrivateAggregationApi,
        {{"fledge_extensions_enabled", "false"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Make sure that `contributeToHistogramOnEvent()` isn't available, but the
// other `privateAggregation` functions are.
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
      privateAggregation.contributeToHistogram(args);
      privateAggregation.contributeToHistogramOnEvent("example", args);
    }
  )";

  v8::Local<v8::UnboundScript> script = Compile(kScript);
  ASSERT_FALSE(script.IsEmpty());

  ContextRecycler context_recycler(helper_.get());
  {
    ContextRecyclerScope scope(context_recycler);  // Initialize context
    context_recycler.AddPrivateAggregationBindings(
        /*private_aggregation_permissions_policy_allowed=*/true);
  }

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
                    "privateAggregation.contributeToHistogramOnEvent is not a "
                    "function."));

    PrivateAggregationRequests pa_requests =
        context_recycler.private_aggregation_bindings()
            ->TakePrivateAggregationRequests();
    ASSERT_EQ(pa_requests.size(), 1u);
  }
}

class ContextRecyclerAdMacroReportingEnabledTest : public ContextRecyclerTest {
 public:
  ContextRecyclerAdMacroReportingEnabledTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kAdAuctionReportingWithMacroApi);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Exercise RegisterAdMacroBindings, and make sure they reset properly.
TEST_F(ContextRecyclerAdMacroReportingEnabledTest, RegisterAdMacroBindings) {
  const char kScript[] = R"(
    function test(prefix) {
      registerAdMacro(prefix + "_name", prefix + "_value");
    }
  )";

  v8::Local<v8::UnboundScript> script = Compile(kScript);
  ASSERT_FALSE(script.IsEmpty());

  ContextRecycler context_recycler(helper_.get());
  {
    ContextRecyclerScope scope(context_recycler);  // Initialize context
    context_recycler.AddRegisterAdMacroBindings();
  }

  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;
    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), std::string("first")));
    EXPECT_THAT(error_msgs, ElementsAre());
    EXPECT_THAT(context_recycler.register_ad_macro_bindings()->TakeAdMacroMap(),
                ElementsAre(Pair("first_name", "first_value")));
  }

  {
    ContextRecyclerScope scope(context_recycler);
    std::vector<std::string> error_msgs;
    Run(scope, script, "test", error_msgs,
        gin::ConvertToV8(helper_->isolate(), std::string("second")));
    EXPECT_THAT(error_msgs, ElementsAre());
    EXPECT_THAT(context_recycler.register_ad_macro_bindings()->TakeAdMacroMap(),
                ElementsAre(Pair("second_name", "second_value")));
  }
}

}  // namespace auction_worklet
