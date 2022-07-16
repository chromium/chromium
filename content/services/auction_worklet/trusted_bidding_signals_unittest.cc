// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/trusted_bidding_signals.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
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

// Common JSON used for most tests. Key 4 is deliberately skipped.
const char kBaseJson[] = R"(
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

const char kHostname[] = "publisher";

class TrustedBiddingSignalsTest : public testing::Test {
 public:
  TrustedBiddingSignalsTest() {
    v8_helper_ = AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner());
  }

  ~TrustedBiddingSignalsTest() override { task_environment_.RunUntilIdle(); }

  // Sets the HTTP response and then fetches bidding signals and waits for
  // completion. Returns nullptr on failure.
  std::unique_ptr<TrustedBiddingSignals::Result>
  FetchBiddingSignalsWithResponse(
      const GURL& url,
      const std::string& response,
      std::vector<std::string> trusted_bidding_signals_keys,
      const std::string& hostname) {
    AddJsonResponse(&url_loader_factory_, url, response);
    return FetchBiddingSignals(trusted_bidding_signals_keys, hostname);
  }

  // Fetches bidding signals and waits for completion. Returns nullptr on
  // failure.
  std::unique_ptr<TrustedBiddingSignals::Result> FetchBiddingSignals(
      std::vector<std::string> trusted_bidding_signals_keys,
      const std::string& hostname) {
    CHECK(!load_signals_run_loop_);

    DCHECK(!load_signals_result_);
    auto bidding_signals = std::make_unique<TrustedBiddingSignals>(
        &url_loader_factory_, std::move(trusted_bidding_signals_keys),
        std::move(hostname), base_url_, v8_helper_,
        base::BindOnce(&TrustedBiddingSignalsTest::LoadSignalsCallback,
                       base::Unretained(this)));
    load_signals_run_loop_ = std::make_unique<base::RunLoop>();
    load_signals_run_loop_->Run();
    load_signals_run_loop_.reset();
    return std::move(load_signals_result_);
  }

  // Returns the results of calling TrustedBiddingSignals::Result::GetSignals()
  // with `trusted_bidding_signals_keys`. Returns value as a JSON std::string,
  // for easy testing.
  std::string ExtractSignals(
      TrustedBiddingSignals::Result* signals,
      std::vector<std::string> trusted_bidding_signals_keys) {
    base::RunLoop run_loop;

    std::string result;
    v8_helper_->v8_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](scoped_refptr<AuctionV8Helper> v8_helper,
               TrustedBiddingSignals::Result* signals,
               std::vector<std::string> trusted_bidding_signals_keys,
               std::string* result_out, base::OnceClosure quit_closure) {
              AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper.get());
              v8::Isolate* isolate = v8_helper->isolate();
              // Could use the scratch context, but using a separate one more
              // closely resembles actual use.
              v8::Local<v8::Context> context = v8::Context::New(isolate);
              v8::Context::Scope context_scope(context);

              v8::Local<v8::Value> value = signals->GetSignals(
                  v8_helper.get(), context, trusted_bidding_signals_keys);

              if (!v8_helper->ExtractJson(context, value, result_out)) {
                *result_out = "JSON extraction failed.";
              }
              std::move(quit_closure).Run();
            },
            v8_helper_, signals, std::move(trusted_bidding_signals_keys),
            &result, run_loop.QuitClosure()));
    run_loop.Run();
    return result;
  }

 protected:
  void LoadSignalsCallback(
      std::unique_ptr<TrustedBiddingSignals::Result> result,
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
  std::unique_ptr<TrustedBiddingSignals::Result> load_signals_result_;
  absl::optional<std::string> error_msg_;

  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<AuctionV8Helper> v8_helper_;
};

TEST_F(TrustedBiddingSignalsTest, NetworkError) {
  url_loader_factory_.AddResponse(
      "https://url.test/?hostname=publisher&keys=key1", kBaseJson,
      net::HTTP_NOT_FOUND);
  EXPECT_FALSE(FetchBiddingSignals({"key1"}, kHostname));
  ASSERT_TRUE(error_msg_.has_value());
  EXPECT_EQ(
      "Failed to load https://url.test/?hostname=publisher&keys=key1 "
      "HTTP status = 404 Not Found.",
      error_msg_.value());
}

TEST_F(TrustedBiddingSignalsTest, ResponseNotJson) {
  EXPECT_FALSE(FetchBiddingSignalsWithResponse(
      GURL("https://url.test/?hostname=publisher&keys=key1"), "Not Json",
      {"key1"}, kHostname));
  ASSERT_TRUE(error_msg_.has_value());
  EXPECT_EQ("https://url.test/ Unable to parse as a JSON object.",
            error_msg_.value());
}

TEST_F(TrustedBiddingSignalsTest, ResponseNotObject) {
  EXPECT_FALSE(FetchBiddingSignalsWithResponse(
      GURL("https://url.test/?hostname=publisher&keys=key1"), "42", {"key1"},
      kHostname));
  ASSERT_TRUE(error_msg_.has_value());
  EXPECT_EQ("https://url.test/ Unable to parse as a JSON object.",
            error_msg_.value());
}

TEST_F(TrustedBiddingSignalsTest, KeyMissing) {
  std::unique_ptr<TrustedBiddingSignals::Result> signals =
      FetchBiddingSignalsWithResponse(
          GURL("https://url.test/?hostname=publisher&keys=key4"), kBaseJson,
          {"key4"}, kHostname);
  ASSERT_TRUE(signals);
  EXPECT_EQ(R"({"key4":null})", ExtractSignals(signals.get(), {"key4"}));
}

TEST_F(TrustedBiddingSignalsTest, FetchOneKey) {
  std::unique_ptr<TrustedBiddingSignals::Result> signals =
      FetchBiddingSignalsWithResponse(
          GURL("https://url.test/?hostname=publisher&keys=key1"), kBaseJson,
          {"key1"}, kHostname);
  ASSERT_TRUE(signals);
  EXPECT_EQ(R"({"key1":1})", ExtractSignals(signals.get(), {"key1"}));
}

TEST_F(TrustedBiddingSignalsTest, FetchMultipleKeys) {
  std::unique_ptr<TrustedBiddingSignals::Result> signals =
      FetchBiddingSignalsWithResponse(
          GURL("https://url.test/?hostname=publisher&keys=key3,key1,key5,key2"),
          kBaseJson, {"key3", "key1", "key5", "key2"}, kHostname);
  ASSERT_TRUE(signals);
  EXPECT_EQ(R"({"key1":1})", ExtractSignals(signals.get(), {"key1"}));
  EXPECT_EQ(R"({"key2":[2]})", ExtractSignals(signals.get(), {"key2"}));
  EXPECT_EQ(R"({"key3":null})", ExtractSignals(signals.get(), {"key3"}));
  EXPECT_EQ(R"({"key5":"value5"})", ExtractSignals(signals.get(), {"key5"}));
  EXPECT_EQ(R"({"key1":1,"key2":[2],"key3":null,"key5":"value5"})",
            ExtractSignals(signals.get(), {"key1", "key2", "key3", "key5"}));
}

TEST_F(TrustedBiddingSignalsTest, EscapeQueryParams) {
  std::unique_ptr<TrustedBiddingSignals::Result> signals =
      FetchBiddingSignalsWithResponse(
          GURL("https://url.test/"
               "?hostname=pub+li%26sher&keys=key+6,key%3D7,key%2C8"),
          kBaseJson, {"key 6", "key=7", "key,8"}, "pub li&sher");
  ASSERT_TRUE(signals);
  EXPECT_EQ(R"({"key 6":6})", ExtractSignals(signals.get(), {"key 6"}));
  EXPECT_EQ(R"({"key=7":7})", ExtractSignals(signals.get(), {"key=7"}));
  EXPECT_EQ(R"({"key,8":8})", ExtractSignals(signals.get(), {"key,8"}));
}

// Testcase where the loader is deleted after it queued the parsing of
// the script on V8 thread, but before it gets to finish.
TEST_F(TrustedBiddingSignalsTest, DeleteBeforeCallback) {
  GURL url("https://url.test/?hostname=publisher&keys=key1");

  AddJsonResponse(&url_loader_factory_, url, kBaseJson);

  // Wedge the V8 thread to control when the JSON parsing takes place.
  base::WaitableEvent* event_handle = WedgeV8Thread(v8_helper_.get());

  auto bidding_signals = std::make_unique<TrustedBiddingSignals>(
      &url_loader_factory_, std::vector<std::string>{"key1"}, "publisher",
      base_url_, v8_helper_,
      base::BindOnce([](std::unique_ptr<TrustedBiddingSignals::Result> result,
                        absl::optional<std::string> error_msg) {
        ADD_FAILURE() << "Callback should not be invoked since loader deleted";
      }));
  base::RunLoop().RunUntilIdle();
  bidding_signals.reset();
  event_handle->Signal();
}

}  // namespace
}  // namespace auction_worklet
