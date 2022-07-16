// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/trusted_scoring_signals.h"

#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
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

// Common JSON used by a number of tests.
const char kBaseJson[] = R"(
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

class TrustedScoringSignalsTest : public testing::Test {
 public:
  TrustedScoringSignalsTest() {
    v8_helper_ = AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner());
  }

  ~TrustedScoringSignalsTest() override { task_environment_.RunUntilIdle(); }

  // Sets the HTTP response and then fetches scoring signals and waits for
  // completion. Returns nullptr on failure.
  std::unique_ptr<TrustedScoringSignals::Result>
  FetchScoringSignalsWithResponse(const GURL& url,
                                  const std::string& response,
                                  std::set<GURL> render_urls,
                                  std::set<GURL> ad_component_render_urls,
                                  const std::string& hostname) {
    AddJsonResponse(&url_loader_factory_, url, response);
    return FetchScoringSignals(render_urls, ad_component_render_urls, hostname);
  }

  // Fetches scoring signals and waits for completion. Returns nullptr on
  // failure.
  std::unique_ptr<TrustedScoringSignals::Result> FetchScoringSignals(
      std::set<GURL> render_urls,
      std::set<GURL> ad_component_render_urls,
      const std::string& hostname) {
    CHECK(!load_signals_run_loop_);

    DCHECK(!load_signals_result_);
    auto scoring_signals = std::make_unique<TrustedScoringSignals>(
        &url_loader_factory_, std::move(render_urls),
        std::move(ad_component_render_urls), std::move(hostname), base_url_,
        v8_helper_,
        base::BindOnce(&TrustedScoringSignalsTest::LoadSignalsCallback,
                       base::Unretained(this)));
    load_signals_run_loop_ = std::make_unique<base::RunLoop>();
    load_signals_run_loop_->Run();
    load_signals_run_loop_.reset();
    return std::move(load_signals_result_);
  }

  // Returns the results of calling TrustedScoringSignals::Result::GetSignals()
  // with the provided parameters. Returns value as a JSON std::string, for easy
  // testing.
  std::string ExtractSignals(TrustedScoringSignals::Result* signals,
                             const GURL& render_url,
                             const std::set<GURL>& ad_component_render_urls) {
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

          v8::Local<v8::Value> value = signals->GetSignals(
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
  void LoadSignalsCallback(
      std::unique_ptr<TrustedScoringSignals::Result> result,
      absl::optional<std::string> error_msg) {
    load_signals_result_ = std::move(result);
    error_msg_ = std::move(error_msg);
    EXPECT_EQ(load_signals_result_ == nullptr, error_msg_.has_value());
    load_signals_run_loop_->Quit();
  }

  base::test::TaskEnvironment task_environment_;

  // URL without query params attached.
  const GURL base_url_ = GURL("https://url.test/");

  // Reuseable run loop for loading the signals. It's always populated after
  // creating the worklet, to cause a crash if the callback is invoked
  // synchronously.
  std::unique_ptr<base::RunLoop> load_signals_run_loop_;
  std::unique_ptr<TrustedScoringSignals::Result> load_signals_result_;
  absl::optional<std::string> error_msg_;

  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<AuctionV8Helper> v8_helper_;
};

TEST_F(TrustedScoringSignalsTest, NetworkError) {
  url_loader_factory_.AddResponse(
      "https://url.test/"
      "?hostname=publisher&renderUrls=https%3A%2F%2Ffoo.test%2F",
      kBaseJson, net::HTTP_NOT_FOUND);
  EXPECT_FALSE(FetchScoringSignals(
      /*render_urls=*/{GURL("https://foo.test/")},
      /*ad_component_render_urls=*/{}, kHostname));
  ASSERT_TRUE(error_msg_.has_value());
  EXPECT_EQ(
      "Failed to load "
      "https://url.test/"
      "?hostname=publisher&renderUrls=https%3A%2F%2Ffoo.test%2F "
      "HTTP status = 404 Not Found.",
      error_msg_.value());
}

TEST_F(TrustedScoringSignalsTest, ResponseNotJson) {
  EXPECT_FALSE(FetchScoringSignalsWithResponse(
      GURL("https://url.test/"
           "?hostname=publisher&renderUrls=https%3A%2F%2Ffoo.test%2F"),
      "Not Json",
      /*render_urls=*/{GURL("https://foo.test/")},
      /*ad_component_render_urls=*/{}, kHostname));
  ASSERT_TRUE(error_msg_.has_value());
  EXPECT_EQ("https://url.test/ Unable to parse as a JSON object.",
            error_msg_.value());
}

TEST_F(TrustedScoringSignalsTest, ResponseNotObject) {
  EXPECT_FALSE(FetchScoringSignalsWithResponse(
      GURL("https://url.test/"
           "?hostname=publisher&renderUrls=https%3A%2F%2Ffoo.test%2F"),
      "42", /*render_urls=*/{GURL("https://foo.test/")},
      /*ad_component_render_urls=*/{}, kHostname));
  ASSERT_TRUE(error_msg_.has_value());
  EXPECT_EQ("https://url.test/ Unable to parse as a JSON object.",
            error_msg_.value());
}

TEST_F(TrustedScoringSignalsTest, ExpectedEntriesNotPresent) {
  std::unique_ptr<TrustedScoringSignals::Result> signals =
      FetchScoringSignalsWithResponse(
          GURL("https://url.test/?hostname=publisher"
               "&renderUrls=https%3A%2F%2Ffoo.test%2F"
               "&adComponentRenderUrls=https%3A%2F%2Fbar.test%2F"),
          R"({"foo":4,"bar":5})",
          /*render_urls=*/{GURL("https://foo.test/")},
          /*ad_component_render_urls=*/{GURL("https://bar.test/")}, kHostname);
  ASSERT_TRUE(signals);
  EXPECT_EQ(
      R"({"renderUrl":{"https://foo.test/":null},"adComponentRenderUrls":{"https://bar.test/":null}})",
      ExtractSignals(signals.get(), /*render_url=*/GURL("https://foo.test/"),
                     /*ad_component_render_urls=*/{GURL("https://bar.test/")}));
  EXPECT_FALSE(error_msg_.has_value());
}

TEST_F(TrustedScoringSignalsTest, NestedEntriesNotObjects) {
  std::unique_ptr<TrustedScoringSignals::Result> signals =
      FetchScoringSignalsWithResponse(
          GURL("https://url.test/?hostname=publisher"
               "&renderUrls=https%3A%2F%2Ffoo.test%2F"
               "&adComponentRenderUrls=https%3A%2F%2Fbar.test%2F"),
          R"({"renderUrls":4,"adComponentRenderUrls":5})",
          /*render_urls=*/{GURL("https://foo.test/")},
          /*ad_component_render_urls=*/{GURL("https://bar.test/")}, kHostname);
  ASSERT_TRUE(signals);
  EXPECT_EQ(
      R"({"renderUrl":{"https://foo.test/":null},"adComponentRenderUrls":{"https://bar.test/":null}})",
      ExtractSignals(signals.get(), /*render_url=*/GURL("https://foo.test/"),
                     /*ad_component_render_urls=*/{GURL("https://bar.test/")}));
  EXPECT_FALSE(error_msg_.has_value());
}

TEST_F(TrustedScoringSignalsTest, KeysMissing) {
  std::unique_ptr<TrustedScoringSignals::Result> signals =
      FetchScoringSignalsWithResponse(
          GURL("https://url.test/?hostname=publisher"
               "&renderUrls=https%3A%2F%2Ffoo.test%2F"
               "&adComponentRenderUrls=https%3A%2F%2Fbar.test%2F"),
          R"({"renderUrls":{"these":"are not"},")"
          R"(adComponentRenderUrls":{"the values":"you're looking for"}})",
          /*render_urls=*/{GURL("https://foo.test/")},
          /*ad_component_render_urls=*/{GURL("https://bar.test/")}, kHostname);
  ASSERT_TRUE(signals);
  EXPECT_EQ(
      R"({"renderUrl":{"https://foo.test/":null},"adComponentRenderUrls":{"https://bar.test/":null}})",
      ExtractSignals(signals.get(), /*render_url=*/GURL("https://foo.test/"),
                     /*ad_component_render_urls=*/{GURL("https://bar.test/")}));
  EXPECT_FALSE(error_msg_.has_value());
}

TEST_F(TrustedScoringSignalsTest, FetchForOneRenderUrl) {
  std::unique_ptr<TrustedScoringSignals::Result> signals =
      FetchScoringSignalsWithResponse(
          GURL("https://url.test/"
               "?hostname=publisher&renderUrls=https%3A%2F%2Ffoo.test%2F"),
          kBaseJson,
          /*render_urls=*/{GURL("https://foo.test/")},
          /*ad_component_render_urls=*/{}, kHostname);
  ASSERT_TRUE(signals);
  EXPECT_EQ(
      R"({"renderUrl":{"https://foo.test/":1}})",
      ExtractSignals(signals.get(), /*render_url=*/GURL("https://foo.test/"),
                     /*ad_component_render_urls=*/{}));
  EXPECT_FALSE(error_msg_.has_value());
}

// Currently, there's no case where a fetch will only be for ad components and
// not render URLs, but once requests are batched, it may be useful. That will
// require other API changes and a caching layer, of course.
TEST_F(TrustedScoringSignalsTest, FetchForOneAdComponentUrl) {
  std::unique_ptr<TrustedScoringSignals::Result> signals =
      FetchScoringSignalsWithResponse(
          GURL("https://url.test/"
               "?hostname=publisher&adComponentRenderUrls=https%3A%2F%2Ffoosub."
               "test%2F"),
          kBaseJson,
          /*render_urls=*/{},
          /*ad_component_render_urls=*/{GURL("https://foosub.test/")},
          kHostname);
  ASSERT_TRUE(signals);
  // Currently there's no way to extract only an ad component value. This test
  // is really just about the fetching and parsing logic.
  EXPECT_EQ(
      R"({"renderUrl":{"https://foo.test/":null},"adComponentRenderUrls":{"https://foosub.test/":2}})",
      ExtractSignals(
          signals.get(), /*render_url=*/GURL("https://foo.test/"),
          /*ad_component_render_urls=*/{GURL("https://foosub.test/")}));
  EXPECT_FALSE(error_msg_.has_value());
}

TEST_F(TrustedScoringSignalsTest, FetchMultipleUrls) {
  // URLs are currently added in lexical order.
  std::unique_ptr<TrustedScoringSignals::Result> signals =
      FetchScoringSignalsWithResponse(
          GURL("https://url.test/?hostname=publisher"
               "&renderUrls=https%3A%2F%2Fbar.test%2F,"
               "https%3A%2F%2Fbaz.test%2F,https%3A%2F%2Ffoo.test%2F"
               "&adComponentRenderUrls=https%3A%2F%2Fbarsub.test%2F,"
               "https%3A%2F%2Fbazsub.test%2F,https%3A%2F%2Ffoosub.test%2F"),
          kBaseJson,
          /*render_urls=*/
          {GURL("https://foo.test/"), GURL("https://bar.test/"),
           GURL("https://baz.test/")},
          /*ad_component_render_urls=*/
          {GURL("https://foosub.test/"), GURL("https://barsub.test/"),
           GURL("https://bazsub.test/")},
          kHostname);
  ASSERT_TRUE(signals);
  EXPECT_FALSE(error_msg_.has_value());
  EXPECT_EQ(
      R"({"renderUrl":{"https://bar.test/":[2]},")"
      R"(adComponentRenderUrls":{"https://barsub.test/":[3],"https://bazsub.test/":null,"https://foosub.test/":2}})",
      ExtractSignals(
          signals.get(), /*render_url=*/GURL("https://bar.test/"),
          /*ad_component_render_urls=*/
          {GURL("https://foosub.test/"), GURL("https://barsub.test/"),
           GURL("https://bazsub.test/")}));
}

// Test when a single URL is used as both a `renderUrl` and
// `adComponentRenderUrl`.
TEST_F(TrustedScoringSignalsTest, FetchSharedUrl) {
  // URLs are currently added in lexical order.
  std::unique_ptr<TrustedScoringSignals::Result> signals =
      FetchScoringSignalsWithResponse(
          GURL("https://url.test/?hostname=publisher"
               "&renderUrls=https%3A%2F%2Fshared.test%2F"
               "&adComponentRenderUrls=https%3A%2F%2Fshared.test%2F"),
          kBaseJson,
          /*render_urls=*/
          {GURL("https://shared.test/")},
          /*ad_component_render_urls=*/
          {GURL("https://shared.test/")}, kHostname);
  ASSERT_TRUE(signals);
  EXPECT_FALSE(error_msg_.has_value());
  EXPECT_EQ(
      R"({"renderUrl":{"https://shared.test/":"render url"},")"
      R"(adComponentRenderUrls":{"https://shared.test/":"ad component url"}})",
      ExtractSignals(signals.get(), /*render_url=*/GURL("https://shared.test/"),
                     /*ad_component_render_urls=*/
                     {GURL("https://shared.test/")}));
}

TEST_F(TrustedScoringSignalsTest, EscapeQueryParams) {
  std::unique_ptr<TrustedScoringSignals::Result> signals =
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
          {GURL("https://foo.test/?&=")}, /*ad_component_render_urls=*/
          {GURL("https://bar.test/?&=")}, "pub li&sher");
  ASSERT_TRUE(signals);
  EXPECT_EQ(
      R"({"renderUrl":{"https://foo.test/?&=":4},"adComponentRenderUrls":{"https://bar.test/?&=":5}})",
      ExtractSignals(signals.get(),                /*render_url=*/
                     GURL("https://foo.test/?&="), /*ad_component_render_urls=*/
                     {GURL("https://bar.test/?&=")}));
  EXPECT_FALSE(error_msg_.has_value());
}

// Testcase where the loader is deleted after it queued the parsing of
// the script on V8 thread, but before it gets to finish.
TEST_F(TrustedScoringSignalsTest, DeleteBeforeCallback) {
  GURL url(
      "https://url.test/"
      "?hostname=publisher&renderUrls=https%3A%2F%2Ffoo.test%2F");

  AddJsonResponse(&url_loader_factory_, url, kBaseJson);

  // Wedge the V8 thread to control when the JSON parsing takes place.
  base::WaitableEvent* event_handle = WedgeV8Thread(v8_helper_.get());
  auto scoring_signals = std::make_unique<TrustedScoringSignals>(
      &url_loader_factory_,
      /*render_urls=*/std::set<GURL>{GURL("http://foo.test/")},
      /*ad_component_urls=*/std::set<GURL>(), "publisher", base_url_,
      v8_helper_,
      base::BindOnce([](std::unique_ptr<TrustedScoringSignals::Result> result,
                        absl::optional<std::string> error_msg) {
        ADD_FAILURE() << "Callback should not be invoked since loader deleted";
      }));
  base::RunLoop().RunUntilIdle();
  scoring_signals.reset();
  event_handle->Signal();
}

}  // namespace
}  // namespace auction_worklet
