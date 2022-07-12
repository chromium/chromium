// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/context_recycler.h"

#include <stdint.h>

#include <limits>
#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/for_debugging_only_bindings.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/register_ad_beacon_bindings.h"
#include "content/services/auction_worklet/report_bindings.h"
#include "content/services/auction_worklet/set_bid_bindings.h"
#include "content/services/auction_worklet/set_priority_bindings.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "v8/include/v8-context.h"

using testing::ElementsAre;
using testing::Pair;

namespace auction_worklet {

class ContextRecyclerTest : public testing::Test {
 public:
  ContextRecyclerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        helper_(AuctionV8Helper::Create(base::ThreadTaskRunnerHandle::Get())) {
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
                              /*debug_id=*/nullptr, function_name, args,
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
    absl::optional<std::vector<blink::InterestGroup::Ad>> ads;
    absl::optional<std::vector<blink::InterestGroup::Ad>> ad_components;
    ads.emplace();
    ads.value().emplace_back(GURL("https://example.com/ad1"), absl::nullopt);

    context_recycler.set_bid_bindings()->ReInitialize(
        base::TimeTicks::Now(),
        /*has_top_level_seller_origin=*/false, &ads, &ad_components);

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
    absl::optional<std::vector<blink::InterestGroup::Ad>> ads;
    absl::optional<std::vector<blink::InterestGroup::Ad>> ad_components;
    ads.emplace();
    ads.value().emplace_back(GURL("https://example.com/notad1"), absl::nullopt);

    context_recycler.set_bid_bindings()->ReInitialize(
        base::TimeTicks::Now(),
        /*has_top_level_seller_origin=*/false, &ads, &ad_components);

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
    absl::optional<std::vector<blink::InterestGroup::Ad>> ads;
    absl::optional<std::vector<blink::InterestGroup::Ad>> ad_components;
    ads.emplace();
    ads.value().emplace_back(GURL("https://example.com/ad3"), absl::nullopt);
    ad_components.emplace();
    ad_components.value().emplace_back(GURL("https://example.com/portion1"),
                                       absl::nullopt);
    ad_components.value().emplace_back(GURL("https://example.com/portion2"),
                                       absl::nullopt);

    context_recycler.set_bid_bindings()->ReInitialize(
        base::TimeTicks::Now(),
        /*has_top_level_seller_origin=*/true, &ads, &ad_components);

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
    absl::optional<std::vector<blink::InterestGroup::Ad>> ads;
    absl::optional<std::vector<blink::InterestGroup::Ad>> ad_components;
    ads.emplace();
    ads.value().emplace_back(GURL("https://example.com/ad5"), absl::nullopt);
    ad_components.emplace();
    ad_components.value().emplace_back(GURL("https://example.com/portion3"),
                                       absl::nullopt);
    ad_components.value().emplace_back(GURL("https://example.com/portion4"),
                                       absl::nullopt);
    ad_components.value().emplace_back(GURL("https://example.com/portion5"),
                                       absl::nullopt);

    context_recycler.set_bid_bindings()->ReInitialize(
        base::TimeTicks::Now(),
        /*has_top_level_seller_origin=*/true, &ads, &ad_components);

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
    absl::optional<std::vector<blink::InterestGroup::Ad>> ads;
    absl::optional<std::vector<blink::InterestGroup::Ad>> ad_components;
    ads.emplace();
    ads.value().emplace_back(GURL("https://example.com/ad5"), absl::nullopt);
    ad_components.emplace();
    ad_components.value().emplace_back(GURL("https://example.com/portion6"),
                                       absl::nullopt);
    ad_components.value().emplace_back(GURL("https://example.com/portion7"),
                                       absl::nullopt);
    ad_components.value().emplace_back(GURL("https://example.com/portion8"),
                                       absl::nullopt);

    context_recycler.set_bid_bindings()->ReInitialize(
        base::TimeTicks::Now(),
        /*has_top_level_seller_origin=*/false, &ads, &ad_components);

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

}  // namespace auction_worklet
