// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/trusted_signals.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/worklet_test_util.h"
#include "net/http/http_status_code.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-forward.h"

namespace auction_worklet {
namespace {

// Common JSON used for most bidding signals tests. Key 4 is deliberately
// skipped.
const char kBaseBiddingJson[] = R"(
  {
    "key1": 1,
    "key2": [2],
    "key3": null,
    "key5": "value5",
    "key 6": 6,
    "key=7": 7,
    "key,8": 8
  }
)";

// Common JSON used for most scoring signals tests.
const char kBaseScoringJson[] = R"(
  {
    "renderUrls": {
      "https://foo.test/": 1,
      "https://bar.test/": [2],
      "https://baz.test/": null,
      "https://shared.test/": "render url"
    },
    "adComponentRenderUrls": {
      "https://foosub.test/": 2,
      "https://barsub.test/": [3],
      "https://bazsub.test/": null,
      "https://shared.test/": "ad component url"
    }
  }
)";

const char kHostname[] = "publisher";

class TrustedSignalsTest : public testing::Test {
 public:
  TrustedSignalsTest() {
    v8_helper_ = AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner());
  }

  ~TrustedSignalsTest() override { task_environment_.RunUntilIdle(); }

  // Sets the HTTP response and then fetches bidding signals and waits for
  // completion. Returns nullptr on failure.
  scoped_refptr<TrustedSignals::Result> FetchBiddingSignalsWithResponse(
      const GURL& url,
      const std::string& response,
      std::set<std::string> trusted_bidding_signals_keys,
      const std::string& hostname) {
    AddJsonResponse(&url_loader_factory_, url, response);
    return FetchBiddingSignals(std::move(trusted_bidding_signals_keys),
                               hostname);
  }

  // Fetches bidding signals and waits for completion. Returns nullptr on
  // failure.
  scoped_refptr<TrustedSignals::Result> FetchBiddingSignals(
      std::set<std::string> trusted_bidding_signals_keys,
      const std::string& hostname) {
    CHECK(!load_signals_run_loop_);

    DCHECK(!load_signals_result_);
    auto bidding_signals = TrustedSignals::LoadBiddingSignals(
        &url_loader_factory_, std::move(trusted_bidding_signals_keys), hostname,
        base_url_, v8_helper_,
        base::BindOnce(&TrustedSignalsTest::LoadSignalsCallback,
                       base::Unretained(this)));
    WaitForLoadComplete();
    return std::move(load_signals_result_);
  }

  // Sets the HTTP response and then fetches scoring signals and waits for
  // completion. Returns nullptr on failure.
  scoped_refptr<TrustedSignals::Result> FetchScoringSignalsWithResponse(
      const GURL& url,
      const std::string& response,
      std::set<std::string> render_urls,
      std::set<std::string> ad_component_render_urls,
      const std::string& hostname) {
    AddJsonResponse(&url_loader_factory_, url, response);
    return FetchScoringSignals(std::move(render_urls),
                               std::move(ad_component_render_urls), hostname);
  }

  // Fetches scoring signals and waits for completion. Returns nullptr on
  // failure.
  scoped_refptr<TrustedSignals::Result> FetchScoringSignals(
      std::set<std::string> render_urls,
      std::set<std::string> ad_component_render_urls,
      const std::string& hostname) {
    CHECK(!load_signals_run_loop_);

    DCHECK(!load_signals_result_);
    auto scoring_signals = TrustedSignals::LoadScoringSignals(
        &url_loader_factory_, std::move(render_urls),
        std::move(ad_component_render_urls), hostname, base_url_, v8_helper_,
        base::BindOnce(&TrustedSignalsTest::LoadSignalsCallback,
                       base::Unretained(this)));
    WaitForLoadComplete();
    return std::move(load_signals_result_);
  }

  // Wait for LoadSignalsCallback to be invoked.
  void WaitForLoadComplete() {
    // Since LoadSignalsCallback is always invoked asynchronously, fine to
    // create the RunLoop after creating the TrustedSignals object, which will
    // ultimately trigger the invocation.
    load_signals_run_loop_ = std::make_unique<base::RunLoop>();
    load_signals_run_loop_->Run();
    load_signals_run_loop_.reset();
  }

  // Returns the results of calling TrustedSignals::Result::GetBiddingSignals()
  // with `trusted_bidding_signals_keys`. Returns value as a JSON std::string,
  // for easy testing.
  std::string ExtractBiddingSignals(
      TrustedSignals::Result* signals,
      std::vector<std::string> trusted_bidding_signals_keys) {
    base::RunLoop run_loop;

    std::string result;
    v8_helper_->v8_runner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper_.get());
          v8::Isolate* isolate = v8_helper_->isolate();
          // Could use the scratch context, but using a separate one more
          // closely resembles actual use.
          v8::Local<v8::Context> context = v8::Context::New(isolate);
          v8::Context::Scope context_scope(context);

          v8::Local<v8::Value> value = signals->GetBiddingSignals(
              v8_helper_.get(), context, trusted_bidding_signals_keys);

          if (!v8_helper_->ExtractJson(context, value, &result)) {
            result = "JSON extraction failed.";
          }
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  // Returns the results of calling TrustedSignals::Result::GetScoringSignals()
  // with the provided parameters. Returns value as a JSON std::string, for easy
  // testing.
  std::string ExtractScoringSignals(
      TrustedSignals::Result* signals,
      const GURL& render_url,
      const std::vector<std::string>& ad_component_render_urls) {
    base::RunLoop run_loop;

    std::string result;
    v8_helper_->v8_runner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper_.get());
          v8::Isolate* isolate = v8_helper_->isolate();
          // Could use the scratch context, but using a separate one more
          // closely resembles actual use.
          v8::Local<v8::Context> context = v8::Context::New(isolate);
          v8::Context::Scope context_scope(context);

          v8::Local<v8::Value> value = signals->GetScoringSignals(
              v8_helper_.get(), context, render_url, ad_component_render_urls);

          if (!v8_helper_->ExtractJson(context, value, &result)) {
            result = "JSON extraction failed.";
          }
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

 protected:
  void LoadSignalsCallback(scoped_refptr<TrustedSignals::Result> result,
                           absl::optional<std::string> error_msg) {
    load_signals_result_ = std::move(result);
    error_msg_ = std::move(error_msg);
    EXPECT_EQ(load_signals_result_.get() == nullptr, error_msg_.has_value());
    load_signals_run_loop_->Quit();
  }

  base::test::TaskEnvironment task_environment_;

  // URL without query params attached.
  const GURL base_url_ = GURL("https://url.test/");

  // Reuseable run loop for loading the signals. It's always populated after
  // creating the worklet, to cause a crash if the callback is invoked
  // synchronously.
  std::unique_ptr<base::RunLoop> load_signals_run_loop_;
  scoped_refptr<TrustedSignals::Result> load_signals_result_;
  absl::optional<std::string> error_msg_;

  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<AuctionV8Helper> v8_helper_;
};

TEST_F(TrustedSignalsTest, BiddingSignalsNetworkError) {
  url_loader_factory_.AddResponse(
      "https://url.test/?hostname=publisher&keys=key1", kBaseBiddingJson,
      net::HTTP_NOT_FOUND);
  EXPECT_FALSE(FetchBiddingSignals({"key1"}, kHostname));
  ASSERT_TRUE(error_msg_.has_value());
  EXPECT_EQ(
      "Failed to load https://url.test/?hostname=publisher&keys=key1 "
      "HTTP status = 404 Not Found.",
      error_msg_.value());
}

TEST_F(TrustedSignalsTest, ScoringSignalsNetworkError) {
  url_loader_factory_.AddResponse(
      "https://url.test/"
      "?hostname=publisher&renderUrls=https%3A%2F%2Ffoo.test%2F",
      kBaseScoringJson, net::HTTP_NOT_FOUND);
  EXPECT_FALSE(FetchScoringSignals(
      /*render_urls=*/{"https://foo.test/"},
      /*ad_component_render_urls=*/{}, kHostname));
  ASSERT_TRUE(error_msg_.has_value());
  EXPECT_EQ(
      "Failed to load "
      "https://url.test/"
      "?hostname=publisher&renderUrls=https%3A%2F%2Ffoo.test%2F "
      "HTTP status = 404 Not Found.",
      error_msg_.value());
}

TEST_F(TrustedSignalsTest, BiddingSignalsResponseNotJson) {
  EXPECT_FALSE(FetchBiddingSignalsWithResponse(
      GURL("https://url.test/?hostname=publisher&keys=key1"), "Not Json",
      {"key1"}, kHostname));
  ASSERT_TRUE(error_msg_.has_value());
  EXPECT_EQ("https://url.test/ Unable to parse as a JSON object.",
            error_msg_.value());
}

TEST_F(TrustedSignalsTest, ScoringSignalsResponseNotJson) {
  EXPECT_FALSE(FetchScoringSignalsWithResponse(
      GURL("https://url.test/"
           "?hostname=publisher&renderUrls=https%3A%2F%2Ffoo.test%2F"),
      "Not Json",
      /*render_urls=*/{"https://foo.test/"},
      /*ad_component_render_urls=*/{}, kHostname));
  ASSERT_TRUE(error_msg_.has_value());
  EXPECT_EQ("https://url.test/ Unable to parse as a JSON object.",
            error_msg_.value());
}

TEST_F(TrustedSignalsTest, BiddingSignalsResponseNotObject) {
  EXPECT_FALSE(FetchBiddingSignalsWithResponse(
      GURL("https://url.test/?hostname=publisher&keys=key1"), "42", {"key1"},
      kHostname));
  ASSERT_TRUE(error_msg_.has_value());
  EXPECT_EQ("https://url.test/ Unable to parse as a JSON object.",
            error_msg_.value());
}

TEST_F(TrustedSignalsTest, ScoringSignalsResponseNotObject) {
  EXPECT_FALSE(FetchScoringSignalsWithResponse(
      GURL("https://url.test/"
           "?hostname=publisher&renderUrls=https%3A%2F%2Ffoo.test%2F"),
      "42", /*render_urls=*/{"https://foo.test/"},
      /*ad_component_render_urls=*/{}, kHostname));
  ASSERT_TRUE(error_msg_.has_value());
  EXPECT_EQ("https://url.test/ Unable to parse as a JSON object.",
            error_msg_.value());
}

TEST_F(TrustedSignalsTest, ScoringSignalsExpectedEntriesNotPresent) {
  scoped_refptr<TrustedSignals::Result> signals =
      FetchScoringSignalsWithResponse(
          GURL("https://url.test/?hostname=publisher"
               "&renderUrls=https%3A%2F%2Ffoo.test%2F"
               "&adComponentRenderUrls=https%3A%2F%2Fbar.test%2F"),
          R"({"foo":4,"bar":5})",
          /*render_urls=*/{"https://foo.test/"},
          /*ad_component_render_urls=*/{"https://bar.test/"}, kHostname);
  ASSERT_TRUE(signals);
  EXPECT_EQ(R"({"renderUrl":{"https://foo.test/":null},)"
            R"("adComponentRenderUrls":{"https://bar.test/":null}})",
            ExtractScoringSignals(
                signals.get(), /*render_url=*/GURL("https://foo.test/"),
                /*ad_component_render_urls=*/{"https://bar.test/"}));
  EXPECT_FALSE(error_msg_.has_value());
}

TEST_F(TrustedSignalsTest, ScoringSignalsNestedEntriesNotObjects) {
  scoped_refptr<TrustedSignals::Result> signals =
      FetchScoringSignalsWithResponse(
          GURL("https://url.test/?hostname=publisher"
               "&renderUrls=https%3A%2F%2Ffoo.test%2F"
               "&adComponentRenderUrls=https%3A%2F%2Fbar.test%2F"),
          R"({"renderUrls":4,"adComponentRenderUrls":5})",
          /*render_urls=*/{"https://foo.test/"},
          /*ad_component_render_urls=*/{"https://bar.test/"}, kHostname);
  ASSERT_TRUE(signals);
  EXPECT_EQ(R"({"renderUrl":{"https://foo.test/":null},)"
            R"("adComponentRenderUrls":{"https://bar.test/":null}})",
            ExtractScoringSignals(
                signals.get(), /*render_url=*/GURL("https://foo.test/"),
                /*ad_component_render_urls=*/{"https://bar.test/"}));
  EXPECT_FALSE(error_msg_.has_value());
}

TEST_F(TrustedSignalsTest, BiddingSignalsKeyMissing) {
  scoped_refptr<TrustedSignals::Result> signals =
      FetchBiddingSignalsWithResponse(
          GURL("https://url.test/?hostname=publisher&keys=key4"),
          kBaseBiddingJson, {"key4"}, kHostname);
  ASSERT_TRUE(signals);
  EXPECT_EQ(R"({"key4":null})", ExtractBiddingSignals(signals.get(), {"key4"}));
}

TEST_F(TrustedSignalsTest, ScoringSignalsKeysMissing) {
  scoped_refptr<TrustedSignals::Result> signals =
      FetchScoringSignalsWithResponse(
          GURL("https://url.test/?hostname=publisher"
               "&renderUrls=https%3A%2F%2Ffoo.test%2F"
               "&adComponentRenderUrls=https%3A%2F%2Fbar.test%2F"),
          R"({"renderUrls":{"these":"are not"},")"
          R"(adComponentRenderUrls":{"the values":"you're looking for"}})",
          /*render_urls=*/{"https://foo.test/"},
          /*ad_component_render_urls=*/{"https://bar.test/"}, kHostname);
  ASSERT_TRUE(signals);
  EXPECT_EQ(R"({"renderUrl":{"https://foo.test/":null},)"
            R"("adComponentRenderUrls":{"https://bar.test/":null}})",
            ExtractScoringSignals(
                signals.get(), /*render_url=*/GURL("https://foo.test/"),
                /*ad_component_render_urls=*/{"https://bar.test/"}));
  EXPECT_FALSE(error_msg_.has_value());
}

TEST_F(TrustedSignalsTest, BiddingSignalsOneKey) {
  scoped_refptr<TrustedSignals::Result> signals =
      FetchBiddingSignalsWithResponse(
          GURL("https://url.test/?hostname=publisher&keys=key1"),
          kBaseBiddingJson, {"key1"}, kHostname);
  ASSERT_TRUE(signals);
  EXPECT_EQ(R"({"key1":1})", ExtractBiddingSignals(signals.get(), {"key1"}));
}

TEST_F(TrustedSignalsTest, ScoringSignalsForOneRenderUrl) {
  scoped_refptr<TrustedSignals::Result> signals =
      FetchScoringSignalsWithResponse(
          GURL("https://url.test/"
               "?hostname=publisher&renderUrls=https%3A%2F%2Ffoo.test%2F"),
          kBaseScoringJson,
          /*render_urls=*/{"https://foo.test/"},
          /*ad_component_render_urls=*/{}, kHostname);
  ASSERT_TRUE(signals);
  EXPECT_EQ(R"({"renderUrl":{"https://foo.test/":1}})",
            ExtractScoringSignals(signals.get(),
                                  /*render_url=*/GURL("https://foo.test/"),
                                  /*ad_component_render_urls=*/{}));
  EXPECT_FALSE(error_msg_.has_value());
}

TEST_F(TrustedSignalsTest, BiddingSignalsMultipleKeys) {
  scoped_refptr<TrustedSignals::Result> signals =
      FetchBiddingSignalsWithResponse(
          GURL("https://url.test/?hostname=publisher&keys=key1,key2,key3,key5"),
          kBaseBiddingJson, {"key3", "key1", "key5", "key2"}, kHostname);
  ASSERT_TRUE(signals);
  EXPECT_EQ(R"({"key1":1})", ExtractBiddingSignals(signals.get(), {"key1"}));
  EXPECT_EQ(R"({"key2":[2]})", ExtractBiddingSignals(signals.get(), {"key2"}));
  EXPECT_EQ(R"({"key3":null})", ExtractBiddingSignals(signals.get(), {"key3"}));
  EXPECT_EQ(R"({"key5":"value5"})",
            ExtractBiddingSignals(signals.get(), {"key5"}));
  EXPECT_EQ(
      R"({"key1":1,"key2":[2],"key3":null,"key5":"value5"})",
      ExtractBiddingSignals(signals.get(), {"key1", "key2", "key3", "key5"}));
}

TEST_F(TrustedSignalsTest, ScoringSignalsMultipleUrls) {
  // URLs are currently added in lexical order.
  scoped_refptr<TrustedSignals::Result> signals =
      FetchScoringSignalsWithResponse(
          GURL("https://url.test/?hostname=publisher"
               "&renderUrls=https%3A%2F%2Fbar.test%2F,"
               "https%3A%2F%2Fbaz.test%2F,https%3A%2F%2Ffoo.test%2F"
               "&adComponentRenderUrls=https%3A%2F%2Fbarsub.test%2F,"
               "https%3A%2F%2Fbazsub.test%2F,https%3A%2F%2Ffoosub.test%2F"),
          kBaseScoringJson,
          /*render_urls=*/
          {"https://foo.test/", "https://bar.test/", "https://baz.test/"},
          /*ad_component_render_urls=*/
          {"https://foosub.test/", "https://barsub.test/",
           "https://bazsub.test/"},
          kHostname);
  ASSERT_TRUE(signals);
  EXPECT_FALSE(error_msg_.has_value());
  EXPECT_EQ(R"({"renderUrl":{"https://bar.test/":[2]},")"
            R"(adComponentRenderUrls":{"https://foosub.test/":2,)"
            R"("https://barsub.test/":[3],"https://bazsub.test/":null}})",
            ExtractScoringSignals(
                signals.get(), /*render_url=*/GURL("https://bar.test/"),
                /*ad_component_render_urls=*/
                {"https://foosub.test/", "https://barsub.test/",
                 "https://bazsub.test/"}));
}

TEST_F(TrustedSignalsTest, BiddingSignalsDuplicateKeys) {
  std::vector<std::string> bidder_signals_vector{"key1", "key2", "key2", "key1",
                                                 "key2"};
  scoped_refptr<TrustedSignals::Result> signals =
      FetchBiddingSignalsWithResponse(
          GURL("https://url.test/?hostname=publisher&keys=key1,key2"),
          kBaseBiddingJson,
          std::set<std::string>{bidder_signals_vector.begin(),
                                bidder_signals_vector.end()},
          kHostname);
  ASSERT_TRUE(signals);
  EXPECT_EQ(R"({"key1":1,"key2":[2]})",
            ExtractBiddingSignals(signals.get(), bidder_signals_vector));
}

TEST_F(TrustedSignalsTest, ScoringSignalsDuplicateKeys) {
  std::vector<std::string> ad_component_render_urls_vector{
      "https://barsub.test/", "https://foosub.test/", "https://foosub.test/",
      "https://barsub.test/"};
  scoped_refptr<TrustedSignals::Result> signals =
      FetchScoringSignalsWithResponse(
          GURL("https://url.test/?hostname=publisher"
               "&renderUrls=https%3A%2F%2Fbar.test%2F,https%3A%2F%2Ffoo.test%2F"
               "&adComponentRenderUrls=https%3A%2F%2Fbarsub.test%2F,"
               "https%3A%2F%2Ffoosub.test%2F"),
          kBaseScoringJson,
          /*render_urls=*/
          {"https://foo.test/", "https://foo.test/", "https://bar.test/",
           "https://bar.test/", "https://foo.test/"},
          std::set<std::string>{ad_component_render_urls_vector.begin(),
                                ad_component_render_urls_vector.end()},
          kHostname);
  ASSERT_TRUE(signals);
  EXPECT_FALSE(error_msg_.has_value());
  EXPECT_EQ(R"({"renderUrl":{"https://bar.test/":[2]},")"
            R"(adComponentRenderUrls":{)"
            R"("https://barsub.test/":[3],"https://foosub.test/":2}})",
            ExtractScoringSignals(signals.get(),
                                  /*render_url=*/GURL("https://bar.test/"),
                                  ad_component_render_urls_vector));
}

// Test when a single URL is used as both a `renderUrl` and
// `adComponentRenderUrl`.
TEST_F(TrustedSignalsTest, ScoringSignalsSharedUrl) {
  // URLs are currently added in lexical order.
  scoped_refptr<TrustedSignals::Result> signals =
      FetchScoringSignalsWithResponse(
          GURL("https://url.test/?hostname=publisher"
               "&renderUrls=https%3A%2F%2Fshared.test%2F"
               "&adComponentRenderUrls=https%3A%2F%2Fshared.test%2F"),
          kBaseScoringJson,
          /*render_urls=*/
          {"https://shared.test/"},
          /*ad_component_render_urls=*/
          {"https://shared.test/"}, kHostname);
  ASSERT_TRUE(signals);
  EXPECT_FALSE(error_msg_.has_value());
  EXPECT_EQ(
      R"({"renderUrl":{"https://shared.test/":"render url"},")"
      R"(adComponentRenderUrls":{"https://shared.test/":"ad component url"}})",
      ExtractScoringSignals(signals.get(),
                            /*render_url=*/GURL("https://shared.test/"),
                            /*ad_component_render_urls=*/
                            {"https://shared.test/"}));
}

TEST_F(TrustedSignalsTest, BiddingSignalsEscapeQueryParams) {
  scoped_refptr<TrustedSignals::Result> signals =
      FetchBiddingSignalsWithResponse(
          GURL("https://url.test/"
               "?hostname=pub+li%26sher&keys=key+6,key%2C8,key%3D7"),
          kBaseBiddingJson, {"key 6", "key=7", "key,8"}, "pub li&sher");
  ASSERT_TRUE(signals);
  EXPECT_EQ(R"({"key 6":6})", ExtractBiddingSignals(signals.get(), {"key 6"}));
  EXPECT_EQ(R"({"key=7":7})", ExtractBiddingSignals(signals.get(), {"key=7"}));
  EXPECT_EQ(R"({"key,8":8})", ExtractBiddingSignals(signals.get(), {"key,8"}));
}

TEST_F(TrustedSignalsTest, ScoringSignalsEscapeQueryParams) {
  scoped_refptr<TrustedSignals::Result> signals =
      FetchScoringSignalsWithResponse(
          GURL("https://url.test/?hostname=pub+li%26sher"
               "&renderUrls=https%3A%2F%2Ffoo.test%2F%3F%26%3D"
               "&adComponentRenderUrls=https%3A%2F%2Fbar.test%2F%3F%26%3D"),
          R"(
  {
    "renderUrls": {
      "https://foo.test/?&=": 4
    },
    "adComponentRenderUrls": {
      "https://bar.test/?&=": 5
    }
  }
)",
          /*render_urls=*/
          {"https://foo.test/?&="}, /*ad_component_render_urls=*/
          {"https://bar.test/?&="}, "pub li&sher");
  ASSERT_TRUE(signals);
  EXPECT_EQ(R"({"renderUrl":{"https://foo.test/?&=":4},)"
            R"("adComponentRenderUrls":{"https://bar.test/?&=":5}})",
            ExtractScoringSignals(
                signals.get(),                /*render_url=*/
                GURL("https://foo.test/?&="), /*ad_component_render_urls=*/
                {"https://bar.test/?&="}));
  EXPECT_FALSE(error_msg_.has_value());
}

// Testcase where the loader is deleted after it queued the parsing of
// the script on V8 thread, but before it gets to finish.
TEST_F(TrustedSignalsTest, BiddingSignalsDeleteBeforeCallback) {
  GURL url("https://url.test/?hostname=publisher&keys=key1");

  AddJsonResponse(&url_loader_factory_, url, kBaseBiddingJson);

  // Wedge the V8 thread to control when the JSON parsing takes place.
  base::WaitableEvent* event_handle = WedgeV8Thread(v8_helper_.get());

  auto bidding_signals = TrustedSignals::LoadBiddingSignals(
      &url_loader_factory_, {"key1"}, "publisher", base_url_, v8_helper_,
      base::BindOnce([](scoped_refptr<TrustedSignals::Result> result,
                        absl::optional<std::string> error_msg) {
        ADD_FAILURE() << "Callback should not be invoked since loader deleted";
      }));
  base::RunLoop().RunUntilIdle();
  bidding_signals.reset();
  event_handle->Signal();
}

// Testcase where the loader is deleted after it queued the parsing of
// the script on V8 thread, but before it gets to finish.
TEST_F(TrustedSignalsTest, ScoringSignalsDeleteBeforeCallback) {
  GURL url(
      "https://url.test/"
      "?hostname=publisher&renderUrls=https%3A%2F%2Ffoo.test%2F");

  AddJsonResponse(&url_loader_factory_, url, kBaseScoringJson);

  // Wedge the V8 thread to control when the JSON parsing takes place.
  base::WaitableEvent* event_handle = WedgeV8Thread(v8_helper_.get());
  auto scoring_signals = TrustedSignals::LoadScoringSignals(
      &url_loader_factory_,
      /*render_urls=*/{"http://foo.test/"},
      /*ad_component_render_urls=*/{}, "publisher", base_url_, v8_helper_,
      base::BindOnce([](scoped_refptr<TrustedSignals::Result> result,
                        absl::optional<std::string> error_msg) {
        ADD_FAILURE() << "Callback should not be invoked since loader deleted";
      }));
  base::RunLoop().RunUntilIdle();
  scoring_signals.reset();
  event_handle->Signal();
}

TEST_F(TrustedSignalsTest, ScoringSignalsWithDataVersion) {
  AddVersionedJsonResponse(
      &url_loader_factory_,
      GURL("https://url.test/"
           "?hostname=publisher&renderUrls=https%3A%2F%2Ffoo.test%2F"),
      kBaseScoringJson, 2u);
  scoped_refptr<TrustedSignals::Result> signals =
      FetchScoringSignals(/*render_urls=*/{"https://foo.test/"},
                          /*ad_component_render_urls=*/{}, kHostname);
  ASSERT_TRUE(signals);
  EXPECT_EQ(R"({"renderUrl":{"https://foo.test/":1}})",
            ExtractScoringSignals(signals.get(),
                                  /*render_url=*/GURL("https://foo.test/"),
                                  /*ad_component_render_urls=*/{}));
  EXPECT_FALSE(error_msg_.has_value());
  EXPECT_EQ(2u, signals->GetDataVersion());
}

TEST_F(TrustedSignalsTest, ScoringSignalsWithInvalidDataVersion) {
  AddResponse(&url_loader_factory_,
              GURL("https://url.test/"
                   "?hostname=publisher&renderUrls=https%3A%2F%2Ffoo.test%2F"),
              kJsonMimeType, absl::nullopt, kBaseScoringJson,
              "X-Allow-FLEDGE: true\nData-Version: 2.0");
  scoped_refptr<TrustedSignals::Result> signals =
      FetchScoringSignals(/*render_urls=*/{"https://foo.test/"},
                          /*ad_component_render_urls=*/{}, kHostname);
  ASSERT_TRUE(error_msg_.has_value());
  EXPECT_EQ(
      "Rejecting load of https://url.test/ due to invalid Data-Version header: "
      "2.0",
      error_msg_.value());
}

}  // namespace
}  // namespace auction_worklet
