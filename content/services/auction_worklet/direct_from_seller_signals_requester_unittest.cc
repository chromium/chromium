// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/direct_from_seller_signals_requester.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/worklet_test_util.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"

namespace auction_worklet {

namespace {

constexpr char kRequiredHeaders[] =
    "X-Allow-FLEDGE: true\nX-FLEDGE-Auction-Only: true";

constexpr char kRequiredHeadersNewName[] =
    "X-Allow-FLEDGE: true\nAd-Auction-Only: true";

constexpr char kRequiredHeadersBothNewAndOldNames[] =
    "X-Allow-FLEDGE: true\nAd-Auction-Only: true\nX-FLEDGE-Auction-Only: true";

constexpr char kRequiredHeadersBothNewAndOldNamesMismatch[] =
    "X-Allow-FLEDGE: true\nAd-Auction-Only: true\nX-FLEDGE-Auction-Only: false";

constexpr char kFalseAuctionOnly[] =
    "X-Allow-FLEDGE: true\nX-FLEDGE-Auction-Only: false";

constexpr char kFalseAuctionOnlyNewName[] =
    "X-Allow-FLEDGE: true\nAd-Auction-Only: false";

// The signals URL and response are arbitrary, from the point of
// DirectFromSellerSignalsRequester.
constexpr char kSignalsUrl[] = "https://seller.com/signals?sellerSignals";
constexpr char kSignalsResponse[] = R"({"seller":"signals"})";

constexpr char kSignalsUrl2[] = "https://seller.com/signals?auctionSignals";
constexpr char kSignalsResponse2[] = R"({"auction":"signals"})";

constexpr char kUtf8Charset[] = "utf-8";

using Request = DirectFromSellerSignalsRequester::Request;

using Result = DirectFromSellerSignalsRequester::Result;

}  // namespace

class DirectFromSellerSignalsRequesterTest : public testing::Test {
 protected:
  std::string ExtractSignals(const Result& result) {
    base::RunLoop run_loop;

    std::string signals;
    v8_helper_->v8_runner()->PostTask(
        FROM_HERE,
        base::BindLambdaForTesting([this, &result, &run_loop, &signals] {
          AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper_.get());
          v8::Isolate* isolate = v8_helper_->isolate();
          // Could use the scratch context, but using a separate one more
          // closely resembles actual use.
          v8::Local<v8::Context> context = v8::Context::New(isolate);
          v8::Context::Scope context_scope(context);

          std::vector<std::string> errors;
          v8::Local<v8::Value> value =
              result.GetSignals(*v8_helper_, context, errors);
          if (!errors.empty()) {
            EXPECT_TRUE(value->IsNull());
            signals = base::JoinString(errors, "\n");
          } else if (v8_helper_->ExtractJson(
                         context, value, /*script_timeout=*/nullptr,
                         &signals) != AuctionV8Helper::Result::kSuccess) {
            signals = "JSON extraction failed.";
          }

          run_loop.Quit();
        }));

    run_loop.Run();
    return signals;
  }

  Result LoadSignalsAndWait(DirectFromSellerSignalsRequester& requester,
                            const GURL& signals_url) {
    base::RunLoop run_loop;
    Result result;
    std::unique_ptr<Request> request =
        requester.LoadSignals(url_loader_factory_, signals_url,
                              base::BindLambdaForTesting(
                                  [&run_loop, &result](Result callback_result) {
                                    result = std::move(callback_result);
                                    run_loop.Quit();
                                  }));

    run_loop.Run();
    return result;
  }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<AuctionV8Helper> v8_helper_ =
      AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner());
  network::TestURLLoaderFactory url_loader_factory_;
};

TEST_F(DirectFromSellerSignalsRequesterTest, LoadNotCached) {
  DirectFromSellerSignalsRequester requester;
  AddResponse(&url_loader_factory_, GURL(kSignalsUrl), kJsonMimeType,
              kUtf8Charset, kSignalsResponse, kRequiredHeaders);
  Result result = LoadSignalsAndWait(requester, GURL(kSignalsUrl));
  EXPECT_EQ(kSignalsResponse, ExtractSignals(result));
}

TEST_F(DirectFromSellerSignalsRequesterTest, LoadCached) {
  DirectFromSellerSignalsRequester requester;
  {
    AddResponse(&url_loader_factory_, GURL(kSignalsUrl), kJsonMimeType,
                kUtf8Charset, kSignalsResponse, kRequiredHeaders);
    Result result = LoadSignalsAndWait(requester, GURL(kSignalsUrl));
    EXPECT_EQ(kSignalsResponse, ExtractSignals(result));
  }

  // Fetch again, without responses. The request should succeed from cache.
  url_loader_factory_.ClearResponses();

  {
    base::RunLoop run_loop;
    Result result;
    std::unique_ptr<Request> request =
        requester.LoadSignals(url_loader_factory_, GURL(kSignalsUrl),
                              base::BindLambdaForTesting(
                                  [&run_loop, &result](Result callback_result) {
                                    result = std::move(callback_result);
                                    run_loop.Quit();
                                  }));

    run_loop.Run();
    EXPECT_EQ(kSignalsResponse, ExtractSignals(result));
  }
  EXPECT_EQ(1u, url_loader_factory_.total_requests());
}

TEST_F(DirectFromSellerSignalsRequesterTest, LoadCoalesced) {
  DirectFromSellerSignalsRequester requester;
  base::RunLoop run_loop1;
  base::RunLoop run_loop2;
  Result result1;
  Result result2;
  std::unique_ptr<Request> request1 =
      requester.LoadSignals(url_loader_factory_, GURL(kSignalsUrl),
                            base::BindLambdaForTesting(
                                [&run_loop1, &result1](Result callback_result) {
                                  result1 = std::move(callback_result);
                                  run_loop1.Quit();
                                }));
  // Fetch the same URL again. Only 1 request should be made.
  std::unique_ptr<Request> request2 =
      requester.LoadSignals(url_loader_factory_, GURL(kSignalsUrl),
                            base::BindLambdaForTesting(
                                [&run_loop2, &result2](Result callback_result) {
                                  result2 = std::move(callback_result);
                                  run_loop2.Quit();
                                }));
  AddResponse(&url_loader_factory_, GURL(kSignalsUrl), kJsonMimeType,
              kUtf8Charset, kSignalsResponse, kRequiredHeaders);

  run_loop1.Run();
  run_loop2.Run();
  EXPECT_EQ(kSignalsResponse, ExtractSignals(result1));
  EXPECT_EQ(kSignalsResponse, ExtractSignals(result2));
  EXPECT_EQ(1u, url_loader_factory_.total_requests());
}

TEST_F(DirectFromSellerSignalsRequesterTest, CacheOverwrite) {
  DirectFromSellerSignalsRequester requester;
  {
    AddResponse(&url_loader_factory_, GURL(kSignalsUrl), kJsonMimeType,
                kUtf8Charset, kSignalsResponse, kRequiredHeaders);
    Result result = LoadSignalsAndWait(requester, GURL(kSignalsUrl));
    EXPECT_EQ(kSignalsResponse, ExtractSignals(result));
  }

  // Fetch a new response. It should overwrite the old.
  url_loader_factory_.ClearResponses();
  {
    AddResponse(&url_loader_factory_, GURL(kSignalsUrl2), kJsonMimeType,
                kUtf8Charset, kSignalsResponse2, kRequiredHeaders);
    Result result = LoadSignalsAndWait(requester, GURL(kSignalsUrl2));
    EXPECT_EQ(kSignalsResponse2, ExtractSignals(result));
  }

  // Now, fetch the new response again. The request should succeed from cache.
  url_loader_factory_.ClearResponses();

  {
    base::RunLoop run_loop;
    Result result;
    std::unique_ptr<Request> request =
        requester.LoadSignals(url_loader_factory_, GURL(kSignalsUrl2),
                              base::BindLambdaForTesting(
                                  [&run_loop, &result](Result callback_result) {
                                    result = std::move(callback_result);
                                    run_loop.Quit();
                                  }));
    EXPECT_EQ(0, url_loader_factory_.NumPending());

    run_loop.Run();
    EXPECT_EQ(kSignalsResponse2, ExtractSignals(result));
  }
  EXPECT_EQ(2u, url_loader_factory_.total_requests());
}

TEST_F(DirectFromSellerSignalsRequesterTest, CantCoalesce) {
  DirectFromSellerSignalsRequester requester;
  base::RunLoop run_loop1;
  base::RunLoop run_loop2;
  Result result1;
  Result result2;
  std::unique_ptr<Request> request1 =
      requester.LoadSignals(url_loader_factory_, GURL(kSignalsUrl),
                            base::BindLambdaForTesting(
                                [&run_loop1, &result1](Result callback_result) {
                                  result1 = std::move(callback_result);
                                  run_loop1.Quit();
                                }));
  // Fetch a different URL at the same time. Two requests should be made.
  std::unique_ptr<Request> request2 =
      requester.LoadSignals(url_loader_factory_, GURL(kSignalsUrl2),
                            base::BindLambdaForTesting(
                                [&run_loop2, &result2](Result callback_result) {
                                  result2 = std::move(callback_result);
                                  run_loop2.Quit();
                                }));
  EXPECT_EQ(2, url_loader_factory_.NumPending());

  // The response to `request1` should end up in the cache, after evicting the
  // response to `request2`.
  AddResponse(&url_loader_factory_, GURL(kSignalsUrl2), kJsonMimeType,
              kUtf8Charset, kSignalsResponse2, kRequiredHeaders);
  run_loop2.Run();

  AddResponse(&url_loader_factory_, GURL(kSignalsUrl), kJsonMimeType,
              kUtf8Charset, kSignalsResponse, kRequiredHeaders);
  run_loop1.Run();

  EXPECT_EQ(kSignalsResponse, ExtractSignals(result1));
  EXPECT_EQ(kSignalsResponse2, ExtractSignals(result2));

  // Now, verify that the first request is still in the cache.
  url_loader_factory_.ClearResponses();
  {
    base::RunLoop run_loop;
    Result result;
    std::unique_ptr<Request> request =
        requester.LoadSignals(url_loader_factory_, GURL(kSignalsUrl),
                              base::BindLambdaForTesting(
                                  [&run_loop, &result](Result callback_result) {
                                    result = std::move(callback_result);
                                    run_loop.Quit();
                                  }));
    EXPECT_EQ(0, url_loader_factory_.NumPending());

    run_loop.Run();
    EXPECT_EQ(kSignalsResponse, ExtractSignals(result));
  }
  EXPECT_EQ(2u, url_loader_factory_.total_requests());
}

TEST_F(DirectFromSellerSignalsRequesterTest, CancelNotCached) {
  DirectFromSellerSignalsRequester requester;
  std::unique_ptr<Request> request = requester.LoadSignals(
      url_loader_factory_, GURL(kSignalsUrl),
      base::BindLambdaForTesting([](Result callback_result) {
        ADD_FAILURE() << "Shouldn't call cancelled callback.";
      }));
  request.reset();
  AddResponse(&url_loader_factory_, GURL(kSignalsUrl), kJsonMimeType,
              kUtf8Charset, kSignalsResponse, kRequiredHeaders);
  base::RunLoop().RunUntilIdle();
}

TEST_F(DirectFromSellerSignalsRequesterTest, CancelCached) {
  DirectFromSellerSignalsRequester requester;
  {
    AddResponse(&url_loader_factory_, GURL(kSignalsUrl), kJsonMimeType,
                kUtf8Charset, kSignalsResponse, kRequiredHeaders);
    Result result = LoadSignalsAndWait(requester, GURL(kSignalsUrl));
    EXPECT_EQ(kSignalsResponse, ExtractSignals(result));
  }

  // Fetch again, without responses. The request should succeed from cache,
  // except that we cancel it.
  url_loader_factory_.ClearResponses();

  {
    base::RunLoop run_loop;
    std::unique_ptr<Request> request = requester.LoadSignals(
        url_loader_factory_, GURL(kSignalsUrl),
        base::BindLambdaForTesting([](Result callback_result) {
          ADD_FAILURE() << "Shouldn't call cancelled callback.";
        }));
    request.reset();
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_EQ(1u, url_loader_factory_.total_requests());
}

TEST_F(DirectFromSellerSignalsRequesterTest, CancelCoalesced) {
  DirectFromSellerSignalsRequester requester;
  base::RunLoop run_loop1;
  Result result1;
  std::unique_ptr<Request> request1 =
      requester.LoadSignals(url_loader_factory_, GURL(kSignalsUrl),
                            base::BindLambdaForTesting(
                                [&run_loop1, &result1](Result callback_result) {
                                  result1 = std::move(callback_result);
                                  run_loop1.Quit();
                                }));
  // Fetch the same URL again. The second request should get coalesced with the
  // first, although the second request gets cancelled.
  std::unique_ptr<Request> request2 = requester.LoadSignals(
      url_loader_factory_, GURL(kSignalsUrl),
      base::BindLambdaForTesting([](Result callback_result) {
        ADD_FAILURE() << "Shouldn't call cancelled callback.";
      }));
  request2.reset();
  AddResponse(&url_loader_factory_, GURL(kSignalsUrl), kJsonMimeType,
              kUtf8Charset, kSignalsResponse, kRequiredHeaders);

  run_loop1.Run();
  EXPECT_EQ(kSignalsResponse, ExtractSignals(result1));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, url_loader_factory_.total_requests());
}

TEST_F(DirectFromSellerSignalsRequesterTest, CancelCoalescedFirstRequest) {
  DirectFromSellerSignalsRequester requester;
  base::RunLoop run_loop2;
  Result result2;
  std::unique_ptr<Request> request1 = requester.LoadSignals(
      url_loader_factory_, GURL(kSignalsUrl),
      base::BindLambdaForTesting([](Result callback_result) {
        ADD_FAILURE() << "Shouldn't call cancelled callback.";
      }));
  // Fetch again, for the same URL. The second request should get coalesced with
  // the first, although the first request gets cancelled.
  std::unique_ptr<Request> request2 =
      requester.LoadSignals(url_loader_factory_, GURL(kSignalsUrl),
                            base::BindLambdaForTesting(
                                [&run_loop2, &result2](Result callback_result) {
                                  result2 = std::move(callback_result);
                                  run_loop2.Quit();
                                }));
  request1.reset();
  AddResponse(&url_loader_factory_, GURL(kSignalsUrl), kJsonMimeType,
              kUtf8Charset, kSignalsResponse, kRequiredHeaders);

  run_loop2.Run();
  EXPECT_EQ(kSignalsResponse, ExtractSignals(result2));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, url_loader_factory_.total_requests());
}

TEST_F(DirectFromSellerSignalsRequesterTest, CancelCoalescedAllRequests) {
  DirectFromSellerSignalsRequester requester;
  std::unique_ptr<Request> request1 = requester.LoadSignals(
      url_loader_factory_, GURL(kSignalsUrl),
      base::BindLambdaForTesting([](Result callback_result) {
        ADD_FAILURE() << "Shouldn't call cancelled callback.";
      }));
  // Fetch again, for the same URL. The second request should get coalesced with
  // the first, although the both requests get cancelled.
  std::unique_ptr<Request> request2 = requester.LoadSignals(
      url_loader_factory_, GURL(kSignalsUrl),
      base::BindLambdaForTesting([](Result callback_result) {
        ADD_FAILURE() << "Shouldn't call cancelled callback.";
      }));
  request1.reset();
  request2.reset();
  AddResponse(&url_loader_factory_, GURL(kSignalsUrl), kJsonMimeType,
              kUtf8Charset, kSignalsResponse, kRequiredHeaders);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, url_loader_factory_.total_requests());
}

TEST_F(DirectFromSellerSignalsRequesterTest, CancelAndRequestAgain) {
  DirectFromSellerSignalsRequester requester;
  std::unique_ptr<Request> request1 = requester.LoadSignals(
      url_loader_factory_, GURL(kSignalsUrl),
      base::BindLambdaForTesting([](Result callback_result) {
        ADD_FAILURE() << "Shouldn't call cancelled callback.";
      }));
  request1.reset();
  AddResponse(&url_loader_factory_, GURL(kSignalsUrl), kJsonMimeType,
              kUtf8Charset, kSignalsResponse, kRequiredHeaders);
  base::RunLoop().RunUntilIdle();

  AddResponse(&url_loader_factory_, GURL(kSignalsUrl), kJsonMimeType,
              kUtf8Charset, kSignalsResponse, kRequiredHeaders);
  Result result = LoadSignalsAndWait(requester, GURL(kSignalsUrl));
  EXPECT_EQ(kSignalsResponse, ExtractSignals(result));
}

TEST_F(DirectFromSellerSignalsRequesterTest, BadJson) {
  DirectFromSellerSignalsRequester requester;
  AddResponse(&url_loader_factory_, GURL(kSignalsUrl), kJsonMimeType,
              kUtf8Charset, "This isn't JSON!", kRequiredHeaders);
  Result result = LoadSignalsAndWait(requester, GURL(kSignalsUrl));
  EXPECT_EQ(
      "DirectFromSellerSignals response for URL "
      "https://seller.com/signals?sellerSignals is not valid JSON.",
      ExtractSignals(result));
}

TEST_F(DirectFromSellerSignalsRequesterTest, MissingAuctionOnly) {
  DirectFromSellerSignalsRequester requester;
  AddResponse(&url_loader_factory_, GURL(kSignalsUrl), kJsonMimeType,
              kUtf8Charset, kSignalsResponse, kAllowFledgeHeader);
  Result result = LoadSignalsAndWait(requester, GURL(kSignalsUrl));
  EXPECT_EQ(
      "Missing Ad-Auction-Only (or deprecated X-FLEDGE-Auction-Only) header.",
      ExtractSignals(result));
}

TEST_F(DirectFromSellerSignalsRequesterTest, NewHeaderName) {
  DirectFromSellerSignalsRequester requester;
  AddResponse(&url_loader_factory_, GURL(kSignalsUrl), kJsonMimeType,
              kUtf8Charset, kSignalsResponse, kRequiredHeadersNewName);
  Result result = LoadSignalsAndWait(requester, GURL(kSignalsUrl));
  EXPECT_EQ(kSignalsResponse, ExtractSignals(result));
}

TEST_F(DirectFromSellerSignalsRequesterTest, BothNewAndOldHeaderNames) {
  DirectFromSellerSignalsRequester requester;
  AddResponse(&url_loader_factory_, GURL(kSignalsUrl), kJsonMimeType,
              kUtf8Charset, kSignalsResponse,
              kRequiredHeadersBothNewAndOldNames);
  Result result = LoadSignalsAndWait(requester, GURL(kSignalsUrl));
  EXPECT_EQ(kSignalsResponse, ExtractSignals(result));
}

TEST_F(DirectFromSellerSignalsRequesterTest, BothNewAndOldHeaderNamesMismatch) {
  DirectFromSellerSignalsRequester requester;
  AddResponse(&url_loader_factory_, GURL(kSignalsUrl), kJsonMimeType,
              kUtf8Charset, kSignalsResponse,
              kRequiredHeadersBothNewAndOldNamesMismatch);
  Result result = LoadSignalsAndWait(requester, GURL(kSignalsUrl));
  EXPECT_EQ(
      "Ad-Auction-Only: true does not match deprecated header "
      "X-FLEDGE-Auction-Only: false.",
      ExtractSignals(result));
}

TEST_F(DirectFromSellerSignalsRequesterTest, BadAuctionOnly) {
  DirectFromSellerSignalsRequester requester;
  AddResponse(&url_loader_factory_, GURL(kSignalsUrl), kJsonMimeType,
              kUtf8Charset, kSignalsResponse, kFalseAuctionOnly);
  Result result = LoadSignalsAndWait(requester, GURL(kSignalsUrl));
  EXPECT_EQ(
      "Wrong Ad-Auction-Only (or deprecated X-FLEDGE-Auction-Only) header "
      "value. Expected \"true\", found "
      "\"false\".",
      ExtractSignals(result));
}

TEST_F(DirectFromSellerSignalsRequesterTest, BadAuctionOnlyNewName) {
  DirectFromSellerSignalsRequester requester;
  AddResponse(&url_loader_factory_, GURL(kSignalsUrl), kJsonMimeType,
              kUtf8Charset, kSignalsResponse, kFalseAuctionOnlyNewName);
  Result result = LoadSignalsAndWait(requester, GURL(kSignalsUrl));
  EXPECT_EQ(
      "Wrong Ad-Auction-Only (or deprecated X-FLEDGE-Auction-Only) header "
      "value. Expected \"true\", found "
      "\"false\".",
      ExtractSignals(result));
}

TEST_F(DirectFromSellerSignalsRequesterTest,
       DeleteRequestWhileRunningCallback) {
  DirectFromSellerSignalsRequester requester;
  base::RunLoop run_loop;
  Result result;
  std::unique_ptr<Request> request;
  request = requester.LoadSignals(
      url_loader_factory_, GURL(kSignalsUrl),
      base::BindLambdaForTesting(
          [&run_loop, &result, &request](Result callback_result) {
            result = std::move(callback_result);
            request.reset();
            run_loop.Quit();
          }));
  AddResponse(&url_loader_factory_, GURL(kSignalsUrl), kJsonMimeType,
              kUtf8Charset, kSignalsResponse, kRequiredHeaders);

  run_loop.Run();
  EXPECT_EQ(kSignalsResponse, ExtractSignals(result));
}

TEST_F(DirectFromSellerSignalsRequesterTest,
       CompletedRequestsCantCancelOtherRequests) {
  DirectFromSellerSignalsRequester requester;
  base::RunLoop run_loop1;
  Result result1;
  std::unique_ptr<Request> request1 =
      requester.LoadSignals(url_loader_factory_, GURL(kSignalsUrl),
                            base::BindLambdaForTesting(
                                [&run_loop1, &result1](Result callback_result) {
                                  result1 = std::move(callback_result);
                                  run_loop1.Quit();
                                }));
  AddResponse(&url_loader_factory_, GURL(kSignalsUrl), kJsonMimeType,
              kUtf8Charset, kSignalsResponse, kRequiredHeaders);

  run_loop1.Run();
  EXPECT_EQ(kSignalsResponse, ExtractSignals(result1));

  // Now, request again for the same URL, but while the new request is ongoing,
  // delete the old request. The new request shouldn't cancel.
  base::RunLoop run_loop2;
  Result result2;
  std::unique_ptr<Request> request2 =
      requester.LoadSignals(url_loader_factory_, GURL(kSignalsUrl),
                            base::BindLambdaForTesting(
                                [&run_loop2, &result2](Result callback_result) {
                                  result2 = std::move(callback_result);
                                  run_loop2.Quit();
                                }));
  AddResponse(&url_loader_factory_, GURL(kSignalsUrl), kJsonMimeType,
              kUtf8Charset, kSignalsResponse, kRequiredHeaders);
  request1.reset();

  run_loop2.Run();
  EXPECT_EQ(kSignalsResponse, ExtractSignals(result2));
}

}  // namespace auction_worklet
