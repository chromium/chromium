// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/trusted_signals_request_manager.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/containers/span_writer.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/cpp/cbor_test_util.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom-forward.h"
#include "content/services/auction_worklet/trusted_signals.h"
#include "content/services/auction_worklet/worklet_test_util.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/third_party/quiche/src/quiche/oblivious_http/oblivious_http_gateway.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-forward.h"

namespace auction_worklet {
namespace {

// Very short time used by some tests that want to wait until just before a
// timer triggers.
constexpr base::TimeDelta kTinyTime = base::Microseconds(1);

// Common JSON used for most bidding signals tests.
const char kBaseBiddingJson[] = R"(
  {
    "keys": {
      "key1": 1,
      "key2": [2],
      "key3": "3"
    },
    "perInterestGroupData": {
      "name1": { "priorityVector": { "foo": 1 } },
      "name2": { "priorityVector": { "foo": 2 } }
    }
  }
)";

// Common JSON used for most scoring signals tests.
const char kBaseScoringJson[] = R"(
  {
    "renderUrls": {
      "https://foo.test/": 1,
      "https://bar.test/": [2]
    },
    "adComponentRenderURLs": {
      "https://foosub.test/": 2,
      "https://barsub.test/": [3],
      "https://bazsub.test/": "4"
    }
  }
)";

// Expected result when fetch trusted scoring signals with `renderUrl` of
// `"https://foo.test/"` and `adComponentRenderURLs` in `kBaseScoringJson[]`.
const char kBaseScoringExpectedResult[] =
    R"({"renderURL":{"https://foo.test/":1},)"
    R"("renderUrl":{"https://foo.test/":1},)"
    R"("adComponentRenderURLs":{"https://foosub.test/":2,)"
    R"("https://barsub.test/":[3],"https://bazsub.test/":"4"},)"
    R"("adComponentRenderUrls":{"https://foosub.test/":2,)"
    R"("https://barsub.test/":[3],"https://bazsub.test/":"4"}})";

const char kKVv2BiddingBase1[] =
    R"(
    [
      {
        "id": 0,
        "keyGroupOutputs": [
          {
            "tags": [
              "interestGroupNames"
            ],
            "keyValues": {
              "name1": {
                "value": "{\"priorityVector\":{\"foo\":1}}"
              }
            }
          },
          {
            "tags": [
              "keys"
            ],
            "keyValues": {
              "key1": {
                "value": "1"
              },
              "key2": {
                "value": "[2]"
              }
            }
          }
        ]
      }
    ])";

const char kKVv2BiddingBase2[] =
    R"(
    [
      {
        "id": 0,
        "keyGroupOutputs": [
          {
            "tags": [
              "interestGroupNames"
            ],
            "keyValues": {
              "name2": {
                "value": "{\"priorityVector\":{\"foo\":2}}"
              }
            }
          },
          {
            "tags": [
              "keys"
            ],
            "keyValues": {
              "key2": {
                "value": "[2]"
              },
              "key3": {
                "value": "\"3\""
              }
            }
          }
        ]
      }
    ])";

const char kKVv2ScoringBase1[] =
    R"([
          {
            "id": 0,
            "keyGroupOutputs": [
              {
                "tags": [
                  "renderUrls"
                ],
                "keyValues": {
                  "https://bar.test/": {
                    "value": "1"
                  }
                }
              },
              {
                "tags": [
                  "adComponentRenderUrls"
                ],
                "keyValues": {
                  "https://barsub.test/": {
                    "value": "2"
                  },
                  "https://foosub.test/": {
                    "value": "[3]"
                  }
                }
              }
            ]
          }
        ])";

const char kKVv2ScoringBase2[] =
    R"([
          {
            "id": 0,
            "keyGroupOutputs": [
              {
                "tags": [
                  "renderUrls"
                ],
                "keyValues": {
                  "https://baz.test/": {
                    "value": "3"
                  }
                }
              },
              {
                "tags": [
                  "adComponentRenderUrls"
                ],
                "keyValues": {
                  "https://bazsub.test/": {
                    "value": "\"4\""
                  },
                  "https://foosub.test/": {
                    "value": "[3]"
                  }
                }
              }
            ]
          }
        ])";

const char kTopLevelOrigin[] = "https://publisher";
const char kJoiningOriginA[] = "https://origin-a.test";
const char kJoiningOriginB[] = "https://origin-b.test";
const char kOwnerOriginA[] = "https://owner-a.test";
const char kOwnerOriginB[] = "https://owner-b.test";
const char kTrustedKVv2SignalsHost[] = "a.test";
const char kTrustedKVv2BiddingSignalsPath[] = "/bidder-signals";
const char kTrustedKVv2ScoringSignalsPath[] = "/seller-signals";
const uint8_t kKeyId = 0xAA;

// These keys were randomly generated as follows:
// EVP_HPKE_KEY keys;
// EVP_HPKE_KEY_generate(&keys, EVP_hpke_x25519_hkdf_sha256());
// and then EVP_HPKE_KEY_public_key and EVP_HPKE_KEY_private_key were used to
// extract the keys.
const uint8_t kTestPrivateKey[] = {
    0xff, 0x1f, 0x47, 0xb1, 0x68, 0xb6, 0xb9, 0xea, 0x65, 0xf7, 0x97,
    0x4f, 0xf2, 0x2e, 0xf2, 0x36, 0x94, 0xe2, 0xf6, 0xb6, 0x8d, 0x66,
    0xf3, 0xa7, 0x64, 0x14, 0x28, 0xd4, 0x45, 0x35, 0x01, 0x8f,
};

const uint8_t kTestPublicKey[] = {
    0xa1, 0x5f, 0x40, 0x65, 0x86, 0xfa, 0xc4, 0x7b, 0x99, 0x59, 0x70,
    0xf1, 0x85, 0xd9, 0xd8, 0x91, 0xc7, 0x4d, 0xcf, 0x1e, 0xb9, 0x1a,
    0x7d, 0x50, 0xa5, 0x8b, 0x01, 0x68, 0x3e, 0x60, 0x05, 0x2d,
};

mojom::TrustedSignalsPublicKeyPtr CreateDefaultPublicKey() {
  return mojom::TrustedSignalsPublicKey::New(
      std::string(reinterpret_cast<const char*>(&kTestPublicKey[0]),
                  sizeof(kTestPublicKey)),
      kKeyId);
}

// Callback for loading signals that stores the result and runs a closure to
// exit a message loop.
void LoadSignalsCallback(scoped_refptr<TrustedSignals::Result>* results_out,
                         std::optional<std::string>* error_msg_out,
                         base::OnceClosure quit_closure,
                         scoped_refptr<TrustedSignals::Result> result,
                         std::optional<std::string> error_msg) {
  *results_out = std::move(result);
  *error_msg_out = std::move(error_msg);
  EXPECT_EQ(results_out->get() == nullptr, error_msg_out->has_value());
  std::move(quit_closure).Run();
}

// Callback that should never be invoked, for cancellation tests.
void NeverInvokedLoadSignalsCallback(
    scoped_refptr<TrustedSignals::Result> result,
    std::optional<std::string> error_msg) {
  ADD_FAILURE() << "This should not be invoked";
}

class TrustedSignalsRequestManagerTest : public testing::Test {
 public:
  explicit TrustedSignalsRequestManagerTest(
      mojom::TrustedSignalsPublicKeyPtr public_key = nullptr)
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        v8_helper_(
            AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner())),
        bidding_request_manager_(
            TrustedSignalsRequestManager::Type::kBiddingSignals,
            &url_loader_factory_,
            /*auction_network_events_handler=*/
            auction_network_events_handler_.CreateRemote(),
            /*automatically_send_requests=*/false,
            url::Origin::Create(GURL(kTopLevelOrigin)),
            trusted_signals_url_,
            /*experiment_group_id=*/std::nullopt,
            "trusted_bidding_signals_slot_size_param=foo",
            std::move(public_key),
            v8_helper_.get()),
        scoring_request_manager_(
            TrustedSignalsRequestManager::Type::kScoringSignals,
            &url_loader_factory_,
            /*auction_network_events_handler=*/
            auction_network_events_handler_.CreateRemote(),
            /*automatically_send_requests=*/false,
            url::Origin::Create(GURL(kTopLevelOrigin)),
            trusted_signals_url_,
            /*experiment_group_id=*/std::nullopt,
            /*trusted_bidding_signals_slot_size_param=*/"",
            /*public_key=*/nullptr,
            v8_helper_.get()) {}

  ~TrustedSignalsRequestManagerTest() override {
    task_environment_.RunUntilIdle();
  }

  // Sets the HTTP response and then fetches bidding signals and waits for
  // completion. Returns nullptr on failure.
  scoped_refptr<TrustedSignals::Result> FetchBiddingSignalsWithResponse(
      const GURL& url,
      const std::string& response,
      const std::string& interest_group_name,
      const std::optional<std::vector<std::string>>&
          trusted_bidding_signals_keys,
      int32_t max_trusted_bidding_signals_url_length = 0) {
    AddBidderJsonResponse(&url_loader_factory_, url, response);
    return FetchBiddingSignals(interest_group_name,
                               trusted_bidding_signals_keys,
                               max_trusted_bidding_signals_url_length);
  }

  // Fetches bidding signals and waits for completion. Returns nullptr on
  // failure.
  scoped_refptr<TrustedSignals::Result> FetchBiddingSignals(
      const std::string& interest_group_name,
      const std::optional<std::vector<std::string>>&
          trusted_bidding_signals_keys,
      int32_t max_trusted_bidding_signals_url_length = 0) {
    scoped_refptr<TrustedSignals::Result> signals;
    base::RunLoop run_loop;
    auto request = bidding_request_manager_.RequestBiddingSignals(
        interest_group_name, trusted_bidding_signals_keys,
        max_trusted_bidding_signals_url_length,
        base::BindOnce(&LoadSignalsCallback, &signals, &error_msg_,
                       run_loop.QuitClosure()));
    bidding_request_manager_.StartBatchedTrustedSignalsRequest();
    run_loop.Run();
    return signals;
  }

  // Sets the HTTP response and then fetches scoring signals and waits for
  // completion. Returns nullptr on failure.
  scoped_refptr<TrustedSignals::Result> FetchScoringSignalsWithResponse(
      const GURL& url,
      const std::string& response,
      const GURL& render_url,
      const std::vector<std::string>& ad_component_render_urls,
      int32_t max_trusted_scoring_signals_url_length = 0) {
    AddJsonResponse(&url_loader_factory_, url, response);
    return FetchScoringSignals(render_url, ad_component_render_urls,
                               max_trusted_scoring_signals_url_length);
  }

  // Fetches scoring signals and waits for completion. Returns nullptr on
  // failure.
  scoped_refptr<TrustedSignals::Result> FetchScoringSignals(
      const GURL& render_url,
      const std::vector<std::string>& ad_component_render_urls,
      int32_t max_trusted_scoring_signals_url_length = 0) {
    scoped_refptr<TrustedSignals::Result> signals;
    base::RunLoop run_loop;
    auto request = scoring_request_manager_.RequestScoringSignals(
        render_url, ad_component_render_urls,
        max_trusted_scoring_signals_url_length,
        base::BindOnce(&LoadSignalsCallback, &signals, &error_msg_,
                       run_loop.QuitClosure()));
    scoring_request_manager_.StartBatchedTrustedSignalsRequest();
    run_loop.Run();
    return signals;
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

          if (v8_helper_->ExtractJson(context, value,
                                      /*script_timeout=*/nullptr, &result) !=
              AuctionV8Helper::Result::kSuccess) {
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

          if (v8_helper_->ExtractJson(context, value,
                                      /*script_timeout=*/nullptr, &result) !=
              AuctionV8Helper::Result::kSuccess) {
            result = "JSON extraction failed.";
          }
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  // URL without query params attached.
  const GURL trusted_signals_url_ = GURL("https://url.test/");

  // The fetch helpers store the most recent error message, if any, here.
  std::optional<std::string> error_msg_;

  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<AuctionV8Helper> v8_helper_;
  TestAuctionNetworkEventsHandler auction_network_events_handler_;
  TrustedSignalsRequestManager bidding_request_manager_;
  TrustedSignalsRequestManager scoring_request_manager_;
};

TEST_F(TrustedSignalsRequestManagerTest, BiddingSignalsError) {
  url_loader_factory_.AddResponse(
      "https://url.test/?hostname=publisher&keys=key1&interestGroupNames=name1"
      "&trusted_bidding_signals_slot_size_param=foo",
      kBaseBiddingJson, net::HTTP_NOT_FOUND);
  EXPECT_FALSE(FetchBiddingSignals({"name1"}, {{"key1"}}));
  EXPECT_EQ(
      "Failed to load "
      "https://url.test/?hostname=publisher&keys=key1&interestGroupNames=name1"
      "&trusted_bidding_signals_slot_size_param=foo "
      "HTTP status = 404 Not Found.",
      error_msg_);

  // Wait until idle to ensure all requests have been observed within the
  // `auction_network_events_handler_`.
  task_environment_.RunUntilIdle();
  EXPECT_THAT(auction_network_events_handler_.GetObservedRequests(),
              testing::ElementsAre(
                  "Sent URL: "
                  "https://url.test/"
                  "?hostname=publisher&keys=key1&interestGroupNames=name1"
                  "&trusted_bidding_signals_slot_size_param=foo",
                  "Received URL: "
                  "https://url.test/"
                  "?hostname=publisher&keys=key1&interestGroupNames=name1"
                  "&trusted_bidding_signals_slot_size_param=foo",
                  "Completion Status: net::ERR_HTTP_RESPONSE_CODE_FAILURE"));
}

TEST_F(TrustedSignalsRequestManagerTest, ScoringSignalsError) {
  url_loader_factory_.AddResponse(
      "https://url.test/"
      "?hostname=publisher&renderUrls=https%3A%2F%2Ffoo.test%2F",
      kBaseScoringJson, net::HTTP_NOT_FOUND);
  EXPECT_FALSE(FetchScoringSignals(GURL("https://foo.test/"),
                                   /*ad_component_render_urls=*/{}));
  EXPECT_EQ(
      "Failed to load "
      "https://url.test/"
      "?hostname=publisher&renderUrls=https%3A%2F%2Ffoo.test%2F "
      "HTTP status = 404 Not Found.",
      error_msg_);
  // Wait until idle to ensure all requests have been observed within the
  // `auction_network_events_handler_`.
  task_environment_.RunUntilIdle();
  EXPECT_THAT(auction_network_events_handler_.GetObservedRequests(),
              testing::ElementsAre(
                  "Sent URL: https://url.test/"
                  "?hostname=publisher&renderUrls=https%3A%2F%2Ffoo.test%2F",
                  "Received URL: https://url.test/"
                  "?hostname=publisher&renderUrls=https%3A%2F%2Ffoo.test%2F",
                  "Completion Status: net::ERR_HTTP_RESPONSE_CODE_FAILURE"));
}

TEST_F(TrustedSignalsRequestManagerTest, BiddingSignalsBatchedRequestError) {
  url_loader_factory_.AddResponse(
      "https://url.test/?hostname=publisher"
      "&keys=key1,key2&interestGroupNames=name1,name2"
      "&trusted_bidding_signals_slot_size_param=foo",
      kBaseBiddingJson, net::HTTP_NOT_FOUND);

  base::RunLoop run_loop1;
  scoped_refptr<TrustedSignals::Result> signals1;
  std::optional<std::string> error_msg1;
  auto request1 = bidding_request_manager_.RequestBiddingSignals(
      {"name1"}, {{"key1"}}, /*max_trusted_bidding_signals_url_length=*/0,
      base::BindOnce(&LoadSignalsCallback, &signals1, &error_msg1,
                     run_loop1.QuitClosure()));

  base::RunLoop run_loop2;
  scoped_refptr<TrustedSignals::Result> signals2;
  std::optional<std::string> error_msg2;
  auto request2 = bidding_request_manager_.RequestBiddingSignals(
      {"name2"}, {{"key2"}}, /*max_trusted_bidding_signals_url_length=*/0,
      base::BindOnce(&LoadSignalsCallback, &signals2, &error_msg2,
                     run_loop2.QuitClosure()));

  bidding_request_manager_.StartBatchedTrustedSignalsRequest();

  const char kExpectedError[] =
      "Failed to load "
      "https://url.test/"
      "?hostname=publisher&keys=key1,key2&interestGroupNames=name1,name2"
      "&trusted_bidding_signals_slot_size_param=foo "
      "HTTP status = 404 Not Found.";

  run_loop1.Run();
  EXPECT_FALSE(signals1);
  EXPECT_EQ(kExpectedError, error_msg1);

  run_loop2.Run();
  EXPECT_FALSE(signals2);
  EXPECT_EQ(kExpectedError, error_msg2);

  // Wait until idle to ensure all requests have been observed within the
  // `auction_network_events_handler_`.
  task_environment_.RunUntilIdle();
  EXPECT_THAT(
      auction_network_events_handler_.GetObservedRequests(),
      testing::ElementsAre(
          "Sent URL: https://url.test/"
          "?hostname=publisher&keys=key1,key2&interestGroupNames=name1,name2"
          "&trusted_bidding_signals_slot_size_param=foo",
          "Received URL: https://url.test/"
          "?hostname=publisher&keys=key1,key2&interestGroupNames=name1,name2"
          "&trusted_bidding_signals_slot_size_param=foo",
          "Completion Status: net::ERR_HTTP_RESPONSE_CODE_FAILURE"));
}

TEST_F(TrustedSignalsRequestManagerTest, ScoringSignalsBatchedRequestError) {
  url_loader_factory_.AddResponse(
      "https://url.test/?hostname=publisher&"
      "renderUrls=https%3A%2F%2Fbar.test%2F,https%3A%2F%2Ffoo.test%2F",
      kBaseScoringJson, net::HTTP_NOT_FOUND);

  base::RunLoop run_loop1;
  scoped_refptr<TrustedSignals::Result> signals1;
  std::optional<std::string> error_msg1;
  auto request1 = scoring_request_manager_.RequestScoringSignals(
      GURL("https://foo.test/"),
      /*ad_component_render_urls=*/{},
      /*max_trusted_scoring_signals_url_length=*/0,
      base::BindOnce(&LoadSignalsCallback, &signals1, &error_msg1,
                     run_loop1.QuitClosure()));

  base::RunLoop run_loop2;
  scoped_refptr<TrustedSignals::Result> signals2;
  std::optional<std::string> error_msg2;
  auto request2 = scoring_request_manager_.RequestScoringSignals(
      GURL("https://bar.test/"),
      /*ad_component_render_urls=*/{},
      /*max_trusted_scoring_signals_url_length=*/0,
      base::BindOnce(&LoadSignalsCallback, &signals2, &error_msg2,
                     run_loop2.QuitClosure()));

  scoring_request_manager_.StartBatchedTrustedSignalsRequest();

  const char kExpectedError[] =
      "Failed to load https://url.test/?hostname=publisher"
      "&renderUrls=https%3A%2F%2Fbar.test%2F,https%3A%2F%2Ffoo.test%2F "
      "HTTP status = 404 Not Found.";

  run_loop1.Run();
  EXPECT_FALSE(signals1);
  EXPECT_EQ(kExpectedError, error_msg1);

  run_loop2.Run();
  EXPECT_FALSE(signals2);
  EXPECT_EQ(kExpectedError, error_msg2);

  // Wait until idle to ensure all requests have been observed within the
  // `auction_network_events_handler_`.
  task_environment_.RunUntilIdle();
  EXPECT_THAT(
      auction_network_events_handler_.GetObservedRequests(),
      testing::ElementsAre(
          "Sent URL: https://url.test/?hostname=publisher"
          "&renderUrls=https%3A%2F%2Fbar.test%2F,https%3A%2F%2Ffoo.test%2F",
          "Received URL: https://url.test/?hostname=publisher"
          "&renderUrls=https%3A%2F%2Fbar.test%2F,https%3A%2F%2Ffoo.test%2F",
          "Completion Status: net::ERR_HTTP_RESPONSE_CODE_FAILURE"));
}

TEST_F(TrustedSignalsRequestManagerTest, BiddingSignalsOneRequestNullKeys) {
  scoped_refptr<TrustedSignals::Result> signals =
      FetchBiddingSignalsWithResponse(
          GURL("https://url.test/?hostname=publisher"
               "&interestGroupNames=name1"
               "&trusted_bidding_signals_slot_size_param=foo"),
          kBaseBiddingJson, {"name1"},
          /*trusted_bidding_signals_keys=*/std::nullopt);
  ASSERT_TRUE(signals);
  EXPECT_FALSE(error_msg_.has_value());
  const auto priority_vector =
      signals->GetPerGroupData("name1")->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"foo", 1}}),
            *priority_vector);
}

TEST_F(TrustedSignalsRequestManagerTest, BiddingSignalsOneRequest) {
  const std::vector<std::string> kKeys{"key2", "key1"};
  scoped_refptr<TrustedSignals::Result> signals =
      FetchBiddingSignalsWithResponse(
          GURL("https://url.test/?hostname=publisher"
               "&keys=key1,key2&interestGroupNames=name1"
               "&trusted_bidding_signals_slot_size_param=foo"),
          kBaseBiddingJson, {"name1"}, kKeys);
  ASSERT_TRUE(signals);
  EXPECT_FALSE(error_msg_.has_value());
  EXPECT_EQ(R"({"key2":[2],"key1":1})",
            ExtractBiddingSignals(signals.get(), kKeys));
  const auto priority_vector =
      signals->GetPerGroupData("name1")->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"foo", 1}}),
            *priority_vector);

  // Wait until idle to ensure all requests have been observed within the
  // `auction_network_events_handler_`.
  task_environment_.RunUntilIdle();
  EXPECT_THAT(
      auction_network_events_handler_.GetObservedRequests(),
      testing::ElementsAre("Sent URL: https://url.test/?hostname=publisher"
                           "&keys=key1,key2&interestGroupNames=name1"
                           "&trusted_bidding_signals_slot_size_param=foo",
                           "Received URL: https://url.test/?hostname=publisher"
                           "&keys=key1,key2&interestGroupNames=name1"
                           "&trusted_bidding_signals_slot_size_param=foo",
                           "Completion Status: net::OK"));
}

TEST_F(TrustedSignalsRequestManagerTest, ScoringSignalsOneRequest) {
  const GURL kRenderUrl = GURL("https://foo.test/");
  const std::vector<std::string> kAdComponentRenderUrls{
      "https://foosub.test/", "https://barsub.test/", "https://bazsub.test/"};
  // URLs are currently added in lexical order.
  scoped_refptr<TrustedSignals::Result> signals =
      FetchScoringSignalsWithResponse(
          GURL("https://url.test/?hostname=publisher"
               "&renderUrls=https%3A%2F%2Ffoo.test%2F"
               "&adComponentRenderUrls=https%3A%2F%2Fbarsub.test%2F,"
               "https%3A%2F%2Fbazsub.test%2F,https%3A%2F%2Ffoosub.test%2F"),
          kBaseScoringJson, kRenderUrl, kAdComponentRenderUrls);
  ASSERT_TRUE(signals);
  EXPECT_FALSE(error_msg_.has_value());
  EXPECT_EQ(
      R"({"renderURL":{"https://foo.test/":1},)"
      R"("renderUrl":{"https://foo.test/":1},)"
      R"("adComponentRenderURLs":{"https://foosub.test/":2,)"
      R"("https://barsub.test/":[3],"https://bazsub.test/":"4"},)"
      R"("adComponentRenderUrls":{"https://foosub.test/":2,)"
      R"("https://barsub.test/":[3],"https://bazsub.test/":"4"}})",
      ExtractScoringSignals(signals.get(), kRenderUrl, kAdComponentRenderUrls));
}

TEST_F(TrustedSignalsRequestManagerTest, BiddingSignalsSequentialRequests) {
  // Use partially overlapping keys, to cover both the shared and distinct key
  // cases.
  const std::vector<std::string> kKeys1{"key1", "key3"};
  const std::vector<std::string> kKeys2{"key2", "key3"};

  // Note that these responses use different values for the shared key.
  scoped_refptr<TrustedSignals::Result> signals1 =
      FetchBiddingSignalsWithResponse(
          GURL("https://url.test/?hostname=publisher&"
               "keys=key1,key3&interestGroupNames=name1"
               "&trusted_bidding_signals_slot_size_param=foo"),
          R"({"keys":{"key1":1,"key3":3},
                      "perInterestGroupData":
                          {"name1": {"priorityVector": {"foo": 1}}}
                      })",
          {"name1"}, kKeys1);
  ASSERT_TRUE(signals1);
  EXPECT_FALSE(error_msg_.has_value());
  EXPECT_EQ(R"({"key1":1,"key3":3})",
            ExtractBiddingSignals(signals1.get(), kKeys1));
  auto priority_vector = signals1->GetPerGroupData("name1")->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"foo", 1}}),
            *priority_vector);

  scoped_refptr<TrustedSignals::Result> signals2 =
      FetchBiddingSignalsWithResponse(
          GURL("https://url.test/?hostname=publisher"
               "&keys=key2,key3&interestGroupNames=name2"
               "&trusted_bidding_signals_slot_size_param=foo"),
          R"({"keys":{"key2":[2],"key3":[3]},
              "perInterestGroupData":
                  {"name2": {"priorityVector": {"foo": 2}}}
              })",
          {"name2"}, kKeys2);
  ASSERT_TRUE(signals1);
  EXPECT_FALSE(error_msg_.has_value());
  EXPECT_EQ(R"({"key2":[2],"key3":[3]})",
            ExtractBiddingSignals(signals2.get(), kKeys2));
  priority_vector = signals2->GetPerGroupData("name2")->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"foo", 2}}),
            *priority_vector);
}

TEST_F(TrustedSignalsRequestManagerTest, ScoringSignalsSequentialRequests) {
  // Use partially overlapping keys, to cover both the shared and distinct key
  // cases.

  const GURL kRenderUrl1 = GURL("https://foo.test/");
  const std::vector<std::string> kAdComponentRenderUrls1{
      "https://foosub.test/", "https://bazsub.test/"};

  const GURL kRenderUrl2 = GURL("https://bar.test/");
  const std::vector<std::string> kAdComponentRenderUrls2{
      "https://barsub.test/", "https://bazsub.test/"};

  // Note that these responses use different values for the shared key.
  scoped_refptr<TrustedSignals::Result> signals1 =
      FetchScoringSignalsWithResponse(
          GURL("https://url.test/?hostname=publisher"
               "&renderUrls=https%3A%2F%2Ffoo.test%2F"
               "&adComponentRenderUrls=https%3A%2F%2Fbazsub.test%2F,"
               "https%3A%2F%2Ffoosub.test%2F"),
          R"(
{
  "renderUrls": {
    "https://foo.test/": 1
  },
  "adComponentRenderUrls": {
    "https://foosub.test/": 2,
    "https://bazsub.test/": 3
  }
}
          )",
          kRenderUrl1, kAdComponentRenderUrls1);
  ASSERT_TRUE(signals1);
  EXPECT_FALSE(error_msg_.has_value());
  EXPECT_EQ(R"({"renderURL":{"https://foo.test/":1},)"
            R"("renderUrl":{"https://foo.test/":1},)"
            R"("adComponentRenderURLs":{"https://foosub.test/":2,)"
            R"("https://bazsub.test/":3},)"
            R"("adComponentRenderUrls":{"https://foosub.test/":2,)"
            R"("https://bazsub.test/":3}})",
            ExtractScoringSignals(signals1.get(), kRenderUrl1,
                                  kAdComponentRenderUrls1));

  scoped_refptr<TrustedSignals::Result> signals2 =
      FetchScoringSignalsWithResponse(
          GURL("https://url.test/?hostname=publisher"
               "&renderUrls=https%3A%2F%2Fbar.test%2F"
               "&adComponentRenderUrls=https%3A%2F%2Fbarsub.test%2F,"
               "https%3A%2F%2Fbazsub.test%2F"),
          R"(
{
  "renderUrls": {
    "https://bar.test/": 4
  },
  "adComponentRenderURLs": {
    "https://barsub.test/": 5,
    "https://bazsub.test/": 6
  }
}
          )",
          kRenderUrl2, kAdComponentRenderUrls2);
  ASSERT_TRUE(signals2);
  EXPECT_FALSE(error_msg_.has_value());
  EXPECT_EQ(R"({"renderURL":{"https://bar.test/":4},)"
            R"("renderUrl":{"https://bar.test/":4},)"
            R"("adComponentRenderURLs":{"https://barsub.test/":5,)"
            R"("https://bazsub.test/":6},)"
            R"("adComponentRenderUrls":{"https://barsub.test/":5,)"
            R"("https://bazsub.test/":6}})",
            ExtractScoringSignals(signals2.get(), kRenderUrl2,
                                  kAdComponentRenderUrls2));
}

// Test the case where there are multiple network requests live at once.
TEST_F(TrustedSignalsRequestManagerTest,
       BiddingSignalsSimultaneousNetworkRequests) {
  // Use partially overlapping keys, to cover both the shared and distinct key
  // cases.
  const std::vector<std::string> kKeys1{"key1", "key3"};
  const GURL kUrl1 = GURL(
      "https://url.test/?hostname=publisher"
      "&keys=key1,key3&interestGroupNames=name1"
      "&trusted_bidding_signals_slot_size_param=foo");

  const std::vector<std::string> kKeys2{"key2", "key3"};
  const GURL kUrl2 = GURL(
      "https://url.test/?hostname=publisher"
      "&keys=key2,key3&interestGroupNames=name2"
      "&trusted_bidding_signals_slot_size_param=foo");

  base::RunLoop run_loop1;
  scoped_refptr<TrustedSignals::Result> signals1;
  std::optional<std::string> error_msg1;
  auto request1 = bidding_request_manager_.RequestBiddingSignals(
      {"name1"}, kKeys1, /*max_trusted_bidding_signals_url_length=*/0,
      base::BindOnce(&LoadSignalsCallback, &signals1, &error_msg1,
                     run_loop1.QuitClosure()));

  bidding_request_manager_.StartBatchedTrustedSignalsRequest();

  base::RunLoop run_loop2;
  scoped_refptr<TrustedSignals::Result> signals2;
  std::optional<std::string> error_msg2;
  auto request2 = bidding_request_manager_.RequestBiddingSignals(
      {"name2"}, kKeys2, /*max_trusted_bidding_signals_url_length=*/0,
      base::BindOnce(&LoadSignalsCallback, &signals2, &error_msg2,
                     run_loop2.QuitClosure()));

  bidding_request_manager_.StartBatchedTrustedSignalsRequest();

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(url_loader_factory_.IsPending(kUrl1.spec()));
  ASSERT_TRUE(url_loader_factory_.IsPending(kUrl2.spec()));

  // Note that these responses use different values for the shared key.
  AddBidderJsonResponse(&url_loader_factory_, kUrl1,
                        R"({"keys":{"key1":1,"key3":3},
                            "perInterestGroupData":
                                {"name1": {"priorityVector": {"foo": 1}}}
                            })");
  AddBidderJsonResponse(&url_loader_factory_, kUrl2,
                        R"({"keys":{"key2":[2],"key3":[3]},
                            "perInterestGroupData":
                                {"name2": {"priorityVector": {"foo": 2}}}
                            })");

  run_loop1.Run();
  EXPECT_FALSE(error_msg1);
  ASSERT_TRUE(signals1);
  EXPECT_EQ(R"({"key1":1,"key3":3})",
            ExtractBiddingSignals(signals1.get(), kKeys1));
  auto priority_vector = signals1->GetPerGroupData("name1")->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"foo", 1}}),
            *priority_vector);

  run_loop2.Run();
  EXPECT_FALSE(error_msg2);
  ASSERT_TRUE(signals2);
  EXPECT_EQ(R"({"key2":[2],"key3":[3]})",
            ExtractBiddingSignals(signals2.get(), kKeys2));
  priority_vector = signals2->GetPerGroupData("name2")->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"foo", 2}}),
            *priority_vector);
}

// Test the case where there are multiple network requests live at once.
TEST_F(TrustedSignalsRequestManagerTest,
       ScoringSignalsSimultaneousNetworkRequests) {
  // Use partially overlapping keys, to cover both the shared and distinct key
  // cases.

  const GURL kRenderUrl1 = GURL("https://foo.test/");
  const std::vector<std::string> kAdComponentRenderUrls1{
      "https://foosub.test/", "https://bazsub.test/"};
  const GURL kSignalsUrl1 = GURL(
      "https://url.test/?hostname=publisher"
      "&renderUrls=https%3A%2F%2Ffoo.test%2F"
      "&adComponentRenderUrls=https%3A%2F%2Fbazsub.test%2F,"
      "https%3A%2F%2Ffoosub.test%2F");
  const GURL kRenderUrl2 = GURL("https://bar.test/");
  const std::vector<std::string> kAdComponentRenderUrls2{
      "https://barsub.test/", "https://bazsub.test/"};
  const GURL kSignalsUrl2 = GURL(
      "https://url.test/?hostname=publisher"
      "&renderUrls=https%3A%2F%2Fbar.test%2F"
      "&adComponentRenderUrls=https%3A%2F%2Fbarsub.test%2F,"
      "https%3A%2F%2Fbazsub.test%2F");

  base::RunLoop run_loop1;
  scoped_refptr<TrustedSignals::Result> signals1;
  std::optional<std::string> error_msg1;
  auto request1 = scoring_request_manager_.RequestScoringSignals(
      kRenderUrl1, kAdComponentRenderUrls1,
      /*max_trusted_scoring_signals_url_length=*/0,
      base::BindOnce(&LoadSignalsCallback, &signals1, &error_msg1,
                     run_loop1.QuitClosure()));

  scoring_request_manager_.StartBatchedTrustedSignalsRequest();

  base::RunLoop run_loop2;
  scoped_refptr<TrustedSignals::Result> signals2;
  std::optional<std::string> error_msg2;
  auto request2 = scoring_request_manager_.RequestScoringSignals(
      kRenderUrl2, kAdComponentRenderUrls2,
      /*max_trusted_scoring_signals_url_length=*/0,
      base::BindOnce(&LoadSignalsCallback, &signals2, &error_msg2,
                     run_loop2.QuitClosure()));

  scoring_request_manager_.StartBatchedTrustedSignalsRequest();

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(url_loader_factory_.IsPending(kSignalsUrl1.spec()));
  ASSERT_TRUE(url_loader_factory_.IsPending(kSignalsUrl2.spec()));

  // Note that these responses use different values for the shared key.

  AddJsonResponse(&url_loader_factory_, kSignalsUrl1, R"(
{
  "renderUrls": {
    "https://foo.test/": 1
  },
  "adComponentRenderURLs": {
    "https://foosub.test/": 2,
    "https://bazsub.test/": 3
  }
}
          )");

  AddJsonResponse(&url_loader_factory_, kSignalsUrl2, R"(
{
  "renderUrls": {
    "https://bar.test/": 4
  },
  "adComponentRenderURLs": {
    "https://barsub.test/": 5,
    "https://bazsub.test/": 6
  }
}
          )");

  run_loop1.Run();
  EXPECT_FALSE(error_msg1);
  ASSERT_TRUE(signals1);
  EXPECT_EQ(R"({"renderURL":{"https://foo.test/":1},)"
            R"("renderUrl":{"https://foo.test/":1},)"
            R"("adComponentRenderURLs":{"https://foosub.test/":2,)"
            R"("https://bazsub.test/":3},)"
            R"("adComponentRenderUrls":{"https://foosub.test/":2,)"
            R"("https://bazsub.test/":3}})",
            ExtractScoringSignals(signals1.get(), kRenderUrl1,
                                  kAdComponentRenderUrls1));

  run_loop2.Run();
  EXPECT_FALSE(error_msg2);
  ASSERT_TRUE(signals2);
  EXPECT_EQ(R"({"renderURL":{"https://bar.test/":4},)"
            R"("renderUrl":{"https://bar.test/":4},)"
            R"("adComponentRenderURLs":{"https://barsub.test/":5,)"
            R"("https://bazsub.test/":6},)"
            R"("adComponentRenderUrls":{"https://barsub.test/":5,)"
            R"("https://bazsub.test/":6}})",
            ExtractScoringSignals(signals2.get(), kRenderUrl2,
                                  kAdComponentRenderUrls2));
}

TEST_F(TrustedSignalsRequestManagerTest, BiddingSignalsBatchedRequests) {
  // Use partially overlapping keys, to cover both the shared and distinct key
  // cases.
  const std::vector<std::string> kKeys1{"key1", "key3"};
  const std::vector<std::string> kKeys2{"key2", "key3"};

  AddBidderJsonResponse(&url_loader_factory_,
                        GURL("https://url.test/?hostname=publisher"
                             "&keys=key1,key2,key3"
                             "&interestGroupNames=name1,name2"
                             "&trusted_bidding_signals_slot_size_param=foo"),
                        kBaseBiddingJson);

  base::RunLoop run_loop1;
  scoped_refptr<TrustedSignals::Result> signals1;
  std::optional<std::string> error_msg1;
  auto request1 = bidding_request_manager_.RequestBiddingSignals(
      {"name1"}, kKeys1, /*max_trusted_bidding_signals_url_length=*/0,
      base::BindOnce(&LoadSignalsCallback, &signals1, &error_msg1,
                     run_loop1.QuitClosure()));

  base::RunLoop run_loop2;
  scoped_refptr<TrustedSignals::Result> signals2;
  std::optional<std::string> error_msg2;
  auto request2 = bidding_request_manager_.RequestBiddingSignals(
      {"name2"}, kKeys2, /*max_trusted_bidding_signals_url_length=*/0,
      base::BindOnce(&LoadSignalsCallback, &signals2, &error_msg2,
                     run_loop2.QuitClosure()));

  bidding_request_manager_.StartBatchedTrustedSignalsRequest();

  run_loop1.Run();
  EXPECT_FALSE(error_msg1);
  ASSERT_TRUE(signals1);
  EXPECT_EQ(R"({"key1":1,"key3":"3"})",
            ExtractBiddingSignals(signals1.get(), kKeys1));
  auto priority_vector = signals1->GetPerGroupData("name1")->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"foo", 1}}),
            *priority_vector);

  run_loop2.Run();
  EXPECT_FALSE(error_msg2);
  ASSERT_TRUE(signals2);
  EXPECT_EQ(R"({"key2":[2],"key3":"3"})",
            ExtractBiddingSignals(signals2.get(), kKeys2));
  priority_vector = signals2->GetPerGroupData("name2")->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"foo", 2}}),
            *priority_vector);
}

TEST_F(TrustedSignalsRequestManagerTest, ScoringSignalsBatchedRequests) {
  // Use partially overlapping keys, to cover both the shared and distinct key
  // cases.
  const GURL kRenderUrl1 = GURL("https://foo.test/");
  const std::vector<std::string> kAdComponentRenderUrls1{
      "https://foosub.test/", "https://bazsub.test/"};

  const GURL kRenderUrl2 = GURL("https://bar.test/");
  const std::vector<std::string> kAdComponentRenderUrls2{
      "https://barsub.test/", "https://bazsub.test/"};

  const GURL kRenderUrl3 = GURL("https://foo.test/");
  const std::vector<std::string> kAdComponentRenderUrls3{};

  AddJsonResponse(
      &url_loader_factory_,
      GURL("https://url.test/?hostname=publisher"
           "&renderUrls=https%3A%2F%2Fbar.test%2F,https%3A%2F%2Ffoo.test%2F"
           "&adComponentRenderUrls=https%3A%2F%2Fbarsub.test%2F,"
           "https%3A%2F%2Fbazsub.test%2F,https%3A%2F%2Ffoosub.test%2F"),
      kBaseScoringJson);

  base::RunLoop run_loop1;
  scoped_refptr<TrustedSignals::Result> signals1;
  std::optional<std::string> error_msg1;
  auto request1 = scoring_request_manager_.RequestScoringSignals(
      kRenderUrl1, kAdComponentRenderUrls1,
      /*max_trusted_scoring_signals_url_length=*/0,
      base::BindOnce(&LoadSignalsCallback, &signals1, &error_msg1,
                     run_loop1.QuitClosure()));

  base::RunLoop run_loop2;
  scoped_refptr<TrustedSignals::Result> signals2;
  std::optional<std::string> error_msg2;
  auto request2 = scoring_request_manager_.RequestScoringSignals(
      kRenderUrl2, kAdComponentRenderUrls2,
      /*max_trusted_scoring_signals_url_length=*/0,
      base::BindOnce(&LoadSignalsCallback, &signals2, &error_msg2,
                     run_loop2.QuitClosure()));

  base::RunLoop run_loop3;
  scoped_refptr<TrustedSignals::Result> signals3;
  std::optional<std::string> error_msg3;
  auto request3 = scoring_request_manager_.RequestScoringSignals(
      kRenderUrl3, kAdComponentRenderUrls3,
      /*max_trusted_scoring_signals_url_length=*/0,
      base::BindOnce(&LoadSignalsCallback, &signals3, &error_msg3,
                     run_loop3.QuitClosure()));

  scoring_request_manager_.StartBatchedTrustedSignalsRequest();

  run_loop1.Run();
  EXPECT_FALSE(error_msg1);
  ASSERT_TRUE(signals1);
  EXPECT_EQ(R"({"renderURL":{"https://foo.test/":1},)"
            R"("renderUrl":{"https://foo.test/":1},)"
            R"("adComponentRenderURLs":{"https://foosub.test/":2,)"
            R"("https://bazsub.test/":"4"},)"
            R"("adComponentRenderUrls":{"https://foosub.test/":2,)"
            R"("https://bazsub.test/":"4"}})",
            ExtractScoringSignals(signals1.get(), kRenderUrl1,
                                  kAdComponentRenderUrls1));

  run_loop2.Run();
  EXPECT_FALSE(error_msg2);
  ASSERT_TRUE(signals2);
  EXPECT_EQ(R"({"renderURL":{"https://bar.test/":[2]},)"
            R"("renderUrl":{"https://bar.test/":[2]},)"
            R"("adComponentRenderURLs":{"https://barsub.test/":[3],)"
            R"("https://bazsub.test/":"4"},)"
            R"("adComponentRenderUrls":{"https://barsub.test/":[3],)"
            R"("https://bazsub.test/":"4"}})",
            ExtractScoringSignals(signals2.get(), kRenderUrl2,
                                  kAdComponentRenderUrls2));

  run_loop3.Run();
  EXPECT_FALSE(error_msg3);
  ASSERT_TRUE(signals3);
  EXPECT_EQ(R"({"renderURL":{"https://foo.test/":1},)"
            R"("renderUrl":{"https://foo.test/":1}})",
            ExtractScoringSignals(signals3.get(), kRenderUrl3,
                                  kAdComponentRenderUrls3));
}

// Make two requests, cancel both, then try to start a network request. No
// requests should be made. Only test bidders, since sellers have no significant
// differences in this path.
TEST_F(TrustedSignalsRequestManagerTest, CancelAllQueuedRequests) {
  const std::vector<std::string> kKeys1{"key1"};
  const std::vector<std::string> kKeys2{"key2"};

  auto request1 = bidding_request_manager_.RequestBiddingSignals(
      {"name1"}, kKeys1, /*max_trusted_bidding_signals_url_length=*/0,
      base::BindOnce(&NeverInvokedLoadSignalsCallback));
  auto request2 = bidding_request_manager_.RequestBiddingSignals(
      {"name1"}, kKeys2, /*max_trusted_bidding_signals_url_length=*/0,
      base::BindOnce(&NeverInvokedLoadSignalsCallback));

  request1.reset();
  request2.reset();

  bidding_request_manager_.StartBatchedTrustedSignalsRequest();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, url_loader_factory_.NumPending());
}

// Make two requests, cancel the first one, then try to start a network request.
// A request should be made, but only for the key in the request that was not
// cancelled. Only test bidders, since sellers have no significant differences
// in this path.
TEST_F(TrustedSignalsRequestManagerTest, CancelOneRequest) {
  const std::vector<std::string> kKeys1{"key1"};
  const std::vector<std::string> kKeys2{"key2"};

  // The request for `key1` will be cancelled before the network request is
  // created.
  AddBidderJsonResponse(&url_loader_factory_,
                        GURL("https://url.test/?hostname=publisher"
                             "&keys=key2&interestGroupNames=name2"
                             "&trusted_bidding_signals_slot_size_param=foo"),
                        kBaseBiddingJson);

  auto request1 = bidding_request_manager_.RequestBiddingSignals(
      {"name1"}, kKeys1, /*max_trusted_bidding_signals_url_length=*/0,
      base::BindOnce(&NeverInvokedLoadSignalsCallback));

  base::RunLoop run_loop2;
  scoped_refptr<TrustedSignals::Result> signals2;
  std::optional<std::string> error_msg2;
  auto request2 = bidding_request_manager_.RequestBiddingSignals(
      {"name2"}, kKeys2, /*max_trusted_bidding_signals_url_length=*/0,
      base::BindOnce(&LoadSignalsCallback, &signals2, &error_msg2,
                     run_loop2.QuitClosure()));

  request1.reset();

  bidding_request_manager_.StartBatchedTrustedSignalsRequest();
  base::RunLoop().RunUntilIdle();

  run_loop2.Run();
  EXPECT_FALSE(error_msg2);
  ASSERT_TRUE(signals2);
  EXPECT_EQ(R"({"key2":[2]})", ExtractBiddingSignals(signals2.get(), kKeys2));
  const auto priority_vector =
      signals2->GetPerGroupData("name2")->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"foo", 2}}),
            *priority_vector);
}

// Make two requests, try to start a network request, then cancel both requests.
// The network request should be cancelled. Only test bidders, since sellers
// have no significant differences in this path.
TEST_F(TrustedSignalsRequestManagerTest, CancelAllLiveRequests) {
  const std::vector<std::string> kKeys1{"key1"};
  const std::vector<std::string> kKeys2{"key2"};
  const GURL kSignalsUrl = GURL(
      "https://url.test/?hostname=publisher"
      "&keys=key1,key2&interestGroupNames=name1"
      "&trusted_bidding_signals_slot_size_param=foo");

  auto request1 = bidding_request_manager_.RequestBiddingSignals(
      {"name1"}, kKeys1, /*max_trusted_bidding_signals_url_length=*/0,
      base::BindOnce(&NeverInvokedLoadSignalsCallback));
  auto request2 = bidding_request_manager_.RequestBiddingSignals(
      {"name1"}, kKeys2, /*max_trusted_bidding_signals_url_length=*/0,
      base::BindOnce(&NeverInvokedLoadSignalsCallback));

  // Wait for network request to be made, which should include both keys in the
  // URLs.
  bidding_request_manager_.StartBatchedTrustedSignalsRequest();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(url_loader_factory_.IsPending(kSignalsUrl.spec()));

  // Cancel both requests, which should cancel the network request.
  request1.reset();
  request2.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(url_loader_factory_.GetPendingRequest(0)->client.is_connected());
}

// Make two requests, try to start a network request, then cancel the first one.
// The request that was not cancelled should complete normally. Only test
// bidders, since sellers have no significant differences in this path.
TEST_F(TrustedSignalsRequestManagerTest, CancelOneLiveRequest) {
  const std::vector<std::string> kKeys1{"key1"};
  const std::vector<std::string> kKeys2{"key2"};
  const GURL kSignalsUrl = GURL(
      "https://url.test/?hostname=publisher"
      "&keys=key1,key2&interestGroupNames=name1,name2"
      "&trusted_bidding_signals_slot_size_param=foo");

  auto request1 = bidding_request_manager_.RequestBiddingSignals(
      {"name1"}, kKeys1, /*max_trusted_bidding_signals_url_length=*/0,
      base::BindOnce(&NeverInvokedLoadSignalsCallback));

  base::RunLoop run_loop2;
  scoped_refptr<TrustedSignals::Result> signals2;
  std::optional<std::string> error_msg2;
  auto request2 = bidding_request_manager_.RequestBiddingSignals(
      {"name2"}, kKeys2, /*max_trusted_bidding_signals_url_length=*/0,
      base::BindOnce(&LoadSignalsCallback, &signals2, &error_msg2,
                     run_loop2.QuitClosure()));

  // Wait for network request to be made, which should include both keys in the
  // URLs.
  bidding_request_manager_.StartBatchedTrustedSignalsRequest();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(url_loader_factory_.IsPending(kSignalsUrl.spec()));

  // Cancel `request1` and then serve the JSON.
  request1.reset();
  AddBidderJsonResponse(&url_loader_factory_, kSignalsUrl, kBaseBiddingJson);

  //  `request2` should still complete.
  run_loop2.Run();
  EXPECT_FALSE(error_msg2);
  ASSERT_TRUE(signals2);
  EXPECT_EQ(R"({"key2":[2]})", ExtractBiddingSignals(signals2.get(), kKeys2));
  const auto priority_vector =
      signals2->GetPerGroupData("name2")->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"foo", 2}}),
            *priority_vector);

  // The callback of `request1` should not be invoked, since it was cancelled.
  base::RunLoop().RunUntilIdle();
}

// Test that when `automatically_send_requests` is false, requests are not
// automatically started.
TEST_F(TrustedSignalsRequestManagerTest, AutomaticallySendRequestsDisabled) {
  const std::vector<std::string> kKeys{"key1"};

  auto request = bidding_request_manager_.RequestBiddingSignals(
      {"name1"}, kKeys, /*max_trusted_bidding_signals_url_length=*/0,
      base::BindOnce(&NeverInvokedLoadSignalsCallback));
  task_environment_.FastForwardBy(base::Hours(1));
  EXPECT_EQ(0, url_loader_factory_.NumPending());
}

TEST_F(TrustedSignalsRequestManagerTest, AutomaticallySendRequestsEnabled) {
  const std::vector<std::string> kKeys1{"key1"};
  const std::vector<std::string> kKeys2{"key2"};
  const std::vector<std::string> kKeys3{"key3"};

  // Create a new bidding request manager with `automatically_send_requests`
  // enabled.
  TrustedSignalsRequestManager bidding_request_manager(
      TrustedSignalsRequestManager::Type::kBiddingSignals, &url_loader_factory_,
      /*auction_network_events_handler=*/
      auction_network_events_handler_.CreateRemote(),
      /*automatically_send_requests=*/true,
      url::Origin::Create(GURL(kTopLevelOrigin)), trusted_signals_url_,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_bidding_signals_slot_size_param=*/"", /*public_key=*/nullptr,
      v8_helper_.get());

  // Create one Request.
  base::RunLoop run_loop1;
  scoped_refptr<TrustedSignals::Result> signals1;
  std::optional<std::string> error_msg1;
  auto request1 = bidding_request_manager.RequestBiddingSignals(
      {"name1"}, kKeys1, /*max_trusted_bidding_signals_url_length=*/0,
      base::BindOnce(&LoadSignalsCallback, &signals1, &error_msg1,
                     run_loop1.QuitClosure()));

  // Wait until just before the timer triggers. No network requests should be
  // made.
  task_environment_.FastForwardBy(TrustedSignalsRequestManager::kAutoSendDelay -
                                  kTinyTime);
  EXPECT_EQ(0, url_loader_factory_.NumPending());

  // Create another Request.
  base::RunLoop run_loop2;
  scoped_refptr<TrustedSignals::Result> signals2;
  std::optional<std::string> error_msg2;
  auto request2 = bidding_request_manager.RequestBiddingSignals(
      {"name1"}, kKeys2, /*max_trusted_bidding_signals_url_length=*/0,
      base::BindOnce(&LoadSignalsCallback, &signals2, &error_msg2,
                     run_loop2.QuitClosure()));

  // Wait until the exact time the timer should trigger. A single network
  // request should be made, covering both Requests.
  task_environment_.FastForwardBy(kTinyTime);
  ASSERT_EQ(1, url_loader_factory_.NumPending());

  AddBidderJsonResponse(&url_loader_factory_,
                        GURL("https://url.test/?hostname=publisher"
                             "&keys=key1,key2&interestGroupNames=name1"),
                        kBaseBiddingJson);

  run_loop1.Run();
  EXPECT_FALSE(error_msg1);
  ASSERT_TRUE(signals1);
  EXPECT_EQ(R"({"key1":1})", ExtractBiddingSignals(signals1.get(), kKeys1));

  run_loop2.Run();
  EXPECT_FALSE(error_msg2);
  ASSERT_TRUE(signals2);
  EXPECT_EQ(R"({"key2":[2]})", ExtractBiddingSignals(signals2.get(), kKeys2));

  // Create one more request, and wait for the timer to trigger again, to make
  // sure it restarts correctly.
  base::RunLoop run_loop3;
  scoped_refptr<TrustedSignals::Result> signals3;
  std::optional<std::string> error_msg3;
  auto request3 = bidding_request_manager.RequestBiddingSignals(
      {"name1"}, kKeys3, /*max_trusted_bidding_signals_url_length=*/0,
      base::BindOnce(&LoadSignalsCallback, &signals3, &error_msg3,
                     run_loop3.QuitClosure()));
  task_environment_.FastForwardBy(TrustedSignalsRequestManager::kAutoSendDelay);
  EXPECT_EQ(1, url_loader_factory_.NumPending());

  // Complete the request.
  AddBidderJsonResponse(&url_loader_factory_,
                        GURL("https://url.test/?hostname=publisher"
                             "&keys=key3&interestGroupNames=name1"),
                        kBaseBiddingJson);
  run_loop3.Run();
  EXPECT_FALSE(error_msg3);
  ASSERT_TRUE(signals3);
  EXPECT_EQ(R"({"key3":"3"})", ExtractBiddingSignals(signals3.get(), kKeys3));
}

TEST_F(TrustedSignalsRequestManagerTest,
       AutomaticallySendRequestsCancelAllRequestsRestartsTimer) {
  const std::vector<std::string> kKeys1{"key1"};
  const std::vector<std::string> kKeys2{"key2"};

  // Create a new bidding request manager with `automatically_send_requests`
  // enabled.
  TrustedSignalsRequestManager bidding_request_manager(
      TrustedSignalsRequestManager::Type::kBiddingSignals, &url_loader_factory_,
      /*auction_network_events_handler=*/
      auction_network_events_handler_.CreateRemote(),
      /*automatically_send_requests=*/true,
      url::Origin::Create(GURL(kTopLevelOrigin)), trusted_signals_url_,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_bidding_signals_slot_size_param=*/"", /*public_key=*/nullptr,
      v8_helper_.get());

  // Create one Request.
  auto request1 = bidding_request_manager.RequestBiddingSignals(
      {"name1"}, kKeys1, /*max_trusted_bidding_signals_url_length=*/0,
      base::BindOnce(&NeverInvokedLoadSignalsCallback));

  // Wait until just before the timer triggers. No network requests should be
  // made.
  task_environment_.FastForwardBy(TrustedSignalsRequestManager::kAutoSendDelay -
                                  kTinyTime);
  EXPECT_EQ(0, url_loader_factory_.NumPending());

  // Cancel the request. The timer should be stopped.
  request1.reset();

  // Create another Request.
  base::RunLoop run_loop2;
  scoped_refptr<TrustedSignals::Result> signals2;
  std::optional<std::string> error_msg2;
  auto request2 = bidding_request_manager.RequestBiddingSignals(
      {"name1"}, kKeys2, /*max_trusted_bidding_signals_url_length=*/0,
      base::BindOnce(&LoadSignalsCallback, &signals2, &error_msg2,
                     run_loop2.QuitClosure()));

  // Wait until just before the timer triggers if it were correctly restarted.
  // No network requests should be made.
  task_environment_.FastForwardBy(TrustedSignalsRequestManager::kAutoSendDelay -
                                  kTinyTime);
  ASSERT_EQ(0, url_loader_factory_.NumPending());

  // Wait until the exact time the timer should trigger. A single network
  // request should be made for just the second Request.
  task_environment_.FastForwardBy(kTinyTime);
  ASSERT_EQ(1, url_loader_factory_.NumPending());

  AddBidderJsonResponse(&url_loader_factory_,
                        GURL("https://url.test/?hostname=publisher"
                             "&keys=key2&interestGroupNames=name1"),
                        kBaseBiddingJson);

  run_loop2.Run();
  EXPECT_FALSE(error_msg2);
  ASSERT_TRUE(signals2);
  EXPECT_EQ(R"({"key2":[2]})", ExtractBiddingSignals(signals2.get(), kKeys2));
}

TEST_F(TrustedSignalsRequestManagerTest,
       AutomaticallySendRequestsCancelSomeRequestsDoesNotRestartTimer) {
  const std::vector<std::string> kKeys1{"key1"};
  const std::vector<std::string> kKeys2{"key2"};

  // Create a new bidding request manager with `automatically_send_requests`
  // enabled.
  TrustedSignalsRequestManager bidding_request_manager(
      TrustedSignalsRequestManager::Type::kBiddingSignals, &url_loader_factory_,
      /*auction_network_events_handler=*/
      auction_network_events_handler_.CreateRemote(),
      /*automatically_send_requests=*/true,
      url::Origin::Create(GURL(kTopLevelOrigin)), trusted_signals_url_,
      /*experiment_group_id=*/std::nullopt,
      /*trusted_bidding_signals_slot_size_param=*/"", /*public_key=*/nullptr,
      v8_helper_.get());

  // Create one Request.
  auto request1 = bidding_request_manager.RequestBiddingSignals(
      {"name1"}, kKeys1, /*max_trusted_bidding_signals_url_length=*/0,
      base::BindOnce(&NeverInvokedLoadSignalsCallback));

  // Wait until just before the timer triggers. No network requests should be
  // made.
  task_environment_.FastForwardBy(TrustedSignalsRequestManager::kAutoSendDelay -
                                  kTinyTime);
  EXPECT_EQ(0, url_loader_factory_.NumPending());

  // Create another Request.
  base::RunLoop run_loop2;
  scoped_refptr<TrustedSignals::Result> signals2;
  std::optional<std::string> error_msg2;
  auto request2 = bidding_request_manager.RequestBiddingSignals(
      {"name1"}, kKeys2, /*max_trusted_bidding_signals_url_length=*/0,
      base::BindOnce(&LoadSignalsCallback, &signals2, &error_msg2,
                     run_loop2.QuitClosure()));

  // Cancel the first request. The timer should not be stopped, since there's
  // still a request pending.
  request1.reset();

  // Wait until the exact time the timer should trigger. A single network
  // request should be made for just the second Request.
  task_environment_.FastForwardBy(kTinyTime);
  ASSERT_EQ(1, url_loader_factory_.NumPending());

  AddBidderJsonResponse(&url_loader_factory_,
                        GURL("https://url.test/?hostname=publisher"
                             "&keys=key2&interestGroupNames=name1"),
                        kBaseBiddingJson);

  run_loop2.Run();
  EXPECT_FALSE(error_msg2);
  ASSERT_TRUE(signals2);
  EXPECT_EQ(R"({"key2":[2]})", ExtractBiddingSignals(signals2.get(), kKeys2));
}

// Test bidding signals request carries experiment ID.
TEST_F(TrustedSignalsRequestManagerTest, BiddingExperimentGroupIds) {
  const std::vector<std::string> kKeys = {"key1"};

  // Create a new bidding request manager with `experiment_group_id` set.
  TrustedSignalsRequestManager bidding_request_manager(
      TrustedSignalsRequestManager::Type::kBiddingSignals, &url_loader_factory_,
      /*auction_network_events_handler=*/
      auction_network_events_handler_.CreateRemote(),
      /*automatically_send_requests=*/false,
      url::Origin::Create(GURL(kTopLevelOrigin)), trusted_signals_url_,
      /*experiment_group_id=*/934u,
      /*trusted_bidding_signals_slot_size_param=*/"", /*public_key=*/nullptr,
      v8_helper_.get());
  AddBidderJsonResponse(
      &url_loader_factory_,
      GURL("https://url.test/"
           "?hostname=publisher"
           "&keys=key1&interestGroupNames=name1&experimentGroupId=934"),
      kBaseBiddingJson);

  base::RunLoop run_loop;
  scoped_refptr<TrustedSignals::Result> signals;
  std::optional<std::string> error_msg;
  auto request = bidding_request_manager.RequestBiddingSignals(
      {"name1"}, kKeys, /*max_trusted_bidding_signals_url_length=*/0,
      base::BindOnce(&LoadSignalsCallback, &signals, &error_msg,
                     run_loop.QuitClosure()));
  bidding_request_manager.StartBatchedTrustedSignalsRequest();

  run_loop.Run();
  EXPECT_FALSE(error_msg);
  ASSERT_TRUE(signals);
  EXPECT_EQ(R"({"key1":1})", ExtractBiddingSignals(signals.get(), kKeys));
  const auto priority_vector =
      signals->GetPerGroupData("name1")->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"foo", 1}}),
            *priority_vector);
}

// Test scoring signals request carries experiment ID.
TEST_F(TrustedSignalsRequestManagerTest, ScoringExperimentGroupIds) {
  const GURL kRenderUrl1 = GURL("https://foo.test/");
  const std::vector<std::string> kAdComponentRenderUrls1 = {
      "https://foosub.test/", "https://bazsub.test/"};

  // Create a new bidding request manager with `experiment_group_id` set.
  TrustedSignalsRequestManager scoring_request_manager(
      TrustedSignalsRequestManager::Type::kScoringSignals, &url_loader_factory_,
      /*auction_network_events_handler=*/
      auction_network_events_handler_.CreateRemote(),
      /*automatically_send_requests=*/false,
      url::Origin::Create(GURL(kTopLevelOrigin)), trusted_signals_url_,
      /*experiment_group_id=*/344u,
      /*trusted_bidding_signals_slot_size_param=*/"", /*public_key=*/nullptr,
      v8_helper_.get());

  AddJsonResponse(&url_loader_factory_,
                  GURL("https://url.test/?hostname=publisher"
                       "&renderUrls=https%3A%2F%2Ffoo.test%2F"
                       "&adComponentRenderUrls=https%3A%2F%2Fbazsub.test%2F,"
                       "https%3A%2F%2Ffoosub.test%2F&experimentGroupId=344"),
                  kBaseScoringJson);

  base::RunLoop run_loop;
  scoped_refptr<TrustedSignals::Result> signals;
  std::optional<std::string> error_msg;
  auto request1 = scoring_request_manager.RequestScoringSignals(
      kRenderUrl1, kAdComponentRenderUrls1,
      /*max_trusted_scoring_signals_url_length=*/0,
      base::BindOnce(&LoadSignalsCallback, &signals, &error_msg,
                     run_loop.QuitClosure()));

  scoring_request_manager.StartBatchedTrustedSignalsRequest();

  run_loop.Run();
  EXPECT_FALSE(error_msg) << *error_msg;
  ASSERT_TRUE(signals);
  EXPECT_EQ(R"({"renderURL":{"https://foo.test/":1},)"
            R"("renderUrl":{"https://foo.test/":1},)"
            R"("adComponentRenderURLs":{"https://foosub.test/":2,)"
            R"("https://bazsub.test/":"4"},)"
            R"("adComponentRenderUrls":{"https://foosub.test/":2,)"
            R"("https://bazsub.test/":"4"}})",
            ExtractScoringSignals(signals.get(), kRenderUrl1,
                                  kAdComponentRenderUrls1));
}

// Test a single bidding request with 0 (unlimited) length limit.
// TODO(crbug.com/326082728): Remove this test because it will be duplicated
// with `BiddingSignalsOneRequest` after the split feature is enabled by
// default.
TEST_F(TrustedSignalsRequestManagerTest,
       BiddingSignalsOneRequestWithZeroLimit) {
  const std::vector<std::string> kKeys{"key2", "key1"};
  const std::string kUrl =
      "https://url.test/?hostname=publisher"
      "&keys=key1,key2&interestGroupNames=name1"
      "&trusted_bidding_signals_slot_size_param=foo";
  scoped_refptr<TrustedSignals::Result> signals =
      FetchBiddingSignalsWithResponse(
          GURL(kUrl), kBaseBiddingJson, {"name1"}, kKeys,
          /*max_trusted_bidding_signals_url_length=*/0);
  ASSERT_TRUE(signals);
  EXPECT_FALSE(error_msg_.has_value());
  EXPECT_EQ(R"({"key2":[2],"key1":1})",
            ExtractBiddingSignals(signals.get(), kKeys));
  const auto priority_vector =
      signals->GetPerGroupData("name1")->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"foo", 1}}),
            *priority_vector);

  // Wait until idle to ensure all requests have been observed within the
  // `auction_network_events_handler_`.
  task_environment_.RunUntilIdle();
  EXPECT_THAT(auction_network_events_handler_.GetObservedRequests(),
              testing::UnorderedElementsAre("Sent URL: " + kUrl,
                                            "Received URL: " + kUrl,
                                            "Completion Status: net::OK"));
}

// Test a single scoring request with 0 (unlimited) length limit.
// TODO(xtlsheep): Remove this test because it will be duplicated with
// `ScoringSignalsOneRequest` after the split feature is enabled by default.
TEST_F(TrustedSignalsRequestManagerTest,
       ScoringSignalsOneRequestWithZeroLimit) {
  const GURL kRenderUrl = GURL("https://foo.test/");
  const std::vector<std::string> kAdComponentRenderUrls{
      "https://foosub.test/", "https://barsub.test/", "https://bazsub.test/"};
  const std::string kUrl =
      "https://url.test/?hostname=publisher"
      "&renderUrls=https%3A%2F%2Ffoo.test%2F"
      "&adComponentRenderUrls=https%3A%2F%2Fbarsub.test%2F,"
      "https%3A%2F%2Fbazsub.test%2F,https%3A%2F%2Ffoosub.test%2F";
  // URLs are currently added in lexical order.
  scoped_refptr<TrustedSignals::Result> signals =
      FetchScoringSignalsWithResponse(
          GURL(kUrl), kBaseScoringJson, kRenderUrl, kAdComponentRenderUrls,
          /*max_trusted_scoring_signals_url_length=*/0);
  ASSERT_TRUE(signals);
  EXPECT_FALSE(error_msg_.has_value());
  EXPECT_EQ(
      kBaseScoringExpectedResult,
      ExtractScoringSignals(signals.get(), kRenderUrl, kAdComponentRenderUrls));

  // Wait until idle to ensure all requests have been observed within the
  // `auction_network_events_handler_`.
  task_environment_.RunUntilIdle();
  EXPECT_THAT(auction_network_events_handler_.GetObservedRequests(),
              testing::UnorderedElementsAre("Sent URL: " + kUrl,
                                            "Received URL: " + kUrl,
                                            "Completion Status: net::OK"));
}

// Test a single bidding request with a tiny length limit that is smaller than
// the URL generated by itself.
TEST_F(TrustedSignalsRequestManagerTest,
       BiddingSignalsOneRequestWithTinyLimit) {
  const std::vector<std::string> kKeys{"key2", "key1"};
  const std::string kUrl =
      "https://url.test/?hostname=publisher"
      "&keys=key1,key2&interestGroupNames=name1"
      "&trusted_bidding_signals_slot_size_param=foo";
  scoped_refptr<TrustedSignals::Result> signals =
      FetchBiddingSignalsWithResponse(
          GURL(kUrl), kBaseBiddingJson, {"name1"}, kKeys,
          /*max_trusted_bidding_signals_url_length=*/1);
  ASSERT_TRUE(signals);
  EXPECT_FALSE(error_msg_.has_value());
  EXPECT_EQ(R"({"key2":[2],"key1":1})",
            ExtractBiddingSignals(signals.get(), kKeys));
  const auto priority_vector =
      signals->GetPerGroupData("name1")->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"foo", 1}}),
            *priority_vector);

  // Wait until idle to ensure all requests have been observed within the
  // `auction_network_events_handler_`.
  task_environment_.RunUntilIdle();
  EXPECT_THAT(auction_network_events_handler_.GetObservedRequests(),
              testing::UnorderedElementsAre("Sent URL: " + kUrl,
                                            "Received URL: " + kUrl,
                                            "Completion Status: net::OK"));
}

// Test a single scoring request with a tiny length limit that is smaller than
// the URL generated by itself.
TEST_F(TrustedSignalsRequestManagerTest,
       ScoringSignalsOneRequestWithTinyLimit) {
  const GURL kRenderUrl = GURL("https://foo.test/");
  const std::vector<std::string> kAdComponentRenderUrls{
      "https://foosub.test/", "https://barsub.test/", "https://bazsub.test/"};
  const std::string kUrl =
      "https://url.test/?hostname=publisher"
      "&renderUrls=https%3A%2F%2Ffoo.test%2F"
      "&adComponentRenderUrls=https%3A%2F%2Fbarsub.test%2F,"
      "https%3A%2F%2Fbazsub.test%2F,https%3A%2F%2Ffoosub.test%2F";
  // URLs are currently added in lexical order.
  scoped_refptr<TrustedSignals::Result> signals =
      FetchScoringSignalsWithResponse(
          GURL(kUrl), kBaseScoringJson, kRenderUrl, kAdComponentRenderUrls,
          /*max_trusted_scoring_signals_url_length=*/1);
  ASSERT_TRUE(signals);
  EXPECT_FALSE(error_msg_.has_value());
  EXPECT_EQ(
      kBaseScoringExpectedResult,
      ExtractScoringSignals(signals.get(), kRenderUrl, kAdComponentRenderUrls));

  // Wait until idle to ensure all requests have been observed within the
  // `auction_network_events_handler_`.
  task_environment_.RunUntilIdle();
  EXPECT_THAT(auction_network_events_handler_.GetObservedRequests(),
              testing::UnorderedElementsAre("Sent URL: " + kUrl,
                                            "Received URL: " + kUrl,
                                            "Completion Status: net::OK"));
}

// Test a single bidding request with normal length limit that is larger than
// the URL generated by itself.
TEST_F(TrustedSignalsRequestManagerTest,
       BiddingSignalsOneRequestWithNormalLimit) {
  const std::vector<std::string> kKeys{"key2", "key1"};
  const std::string kUrl =
      "https://url.test/?hostname=publisher"
      "&keys=key1,key2&interestGroupNames=name1"
      "&trusted_bidding_signals_slot_size_param=foo";

  scoped_refptr<TrustedSignals::Result> signals =
      FetchBiddingSignalsWithResponse(
          GURL(kUrl), kBaseBiddingJson, {"name1"}, kKeys,
          /*max_trusted_bidding_signals_url_length=*/1000);
  ASSERT_TRUE(signals);
  EXPECT_FALSE(error_msg_.has_value());
  EXPECT_EQ(R"({"key2":[2],"key1":1})",
            ExtractBiddingSignals(signals.get(), kKeys));
  const auto priority_vector =
      signals->GetPerGroupData("name1")->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"foo", 1}}),
            *priority_vector);

  // Wait until idle to ensure all requests have been observed within the
  // `auction_network_events_handler_`.
  task_environment_.RunUntilIdle();
  EXPECT_THAT(auction_network_events_handler_.GetObservedRequests(),
              testing::UnorderedElementsAre("Sent URL: " + kUrl,
                                            "Received URL: " + kUrl,
                                            "Completion Status: net::OK"));
}

// Test a single scoring request with normal length limit that is larger than
// the URL generated by itself.
TEST_F(TrustedSignalsRequestManagerTest,
       ScoringSignalsOneRequestWithNormalLimit) {
  const GURL kRenderUrl = GURL("https://foo.test/");
  const std::vector<std::string> kAdComponentRenderUrls{
      "https://foosub.test/", "https://barsub.test/", "https://bazsub.test/"};
  const std::string kUrl =
      "https://url.test/?hostname=publisher"
      "&renderUrls=https%3A%2F%2Ffoo.test%2F"
      "&adComponentRenderUrls=https%3A%2F%2Fbarsub.test%2F,"
      "https%3A%2F%2Fbazsub.test%2F,https%3A%2F%2Ffoosub.test%2F";
  // URLs are currently added in lexical order.
  scoped_refptr<TrustedSignals::Result> signals =
      FetchScoringSignalsWithResponse(
          GURL(kUrl), kBaseScoringJson, kRenderUrl, kAdComponentRenderUrls,
          /*max_trusted_scoring_signals_url_length=*/1000);
  ASSERT_TRUE(signals);
  EXPECT_FALSE(error_msg_.has_value());
  EXPECT_EQ(
      kBaseScoringExpectedResult,
      ExtractScoringSignals(signals.get(), kRenderUrl, kAdComponentRenderUrls));

  // Wait until idle to ensure all requests have been observed within the
  // `auction_network_events_handler_`.
  task_environment_.RunUntilIdle();
  EXPECT_THAT(auction_network_events_handler_.GetObservedRequests(),
              testing::UnorderedElementsAre("Sent URL: " + kUrl,
                                            "Received URL: " + kUrl,
                                            "Completion Status: net::OK"));
}

// Test two requests are issued in one request when their joint URL length is
// smaller than either of their limits:
// Request A has a limit of 0.
// Request B has a limit of 1000.
// The combined URL length of requests A and B is 131.
TEST_F(TrustedSignalsRequestManagerTest, BiddingSignalsJointBatchedRequests) {
  // Use partially overlapping keys, to cover both the shared and distinct key
  // cases.
  const std::vector<std::string> kKeys1{"key1", "key3"};
  const std::vector<std::string> kKeys2{"key2", "key3"};

  const std::string kSignalsUrl =
      "https://url.test/?hostname=publisher"
      "&keys=key1,key2,key3"
      "&interestGroupNames=name1,name2"
      "&trusted_bidding_signals_slot_size_param=foo";

  base::RunLoop run_loop1;
  scoped_refptr<TrustedSignals::Result> signals1;
  std::optional<std::string> error_msg1;

  auto request1 = bidding_request_manager_.RequestBiddingSignals(
      {"name1"}, kKeys1, /*max_trusted_bidding_signals_url_length=*/0,
      base::BindOnce(&LoadSignalsCallback, &signals1, &error_msg1,
                     run_loop1.QuitClosure()));

  base::RunLoop run_loop2;
  scoped_refptr<TrustedSignals::Result> signals2;
  std::optional<std::string> error_msg2;
  auto request2 = bidding_request_manager_.RequestBiddingSignals(
      {"name2"}, kKeys2, /*max_trusted_bidding_signals_url_length=*/1000,
      base::BindOnce(&LoadSignalsCallback, &signals2, &error_msg2,
                     run_loop2.QuitClosure()));

  AddBidderJsonResponse(&url_loader_factory_, GURL(kSignalsUrl),
                        kBaseBiddingJson);
  bidding_request_manager_.StartBatchedTrustedSignalsRequest();

  run_loop1.Run();
  EXPECT_FALSE(error_msg1);
  ASSERT_TRUE(signals1);
  EXPECT_EQ(R"({"key1":1,"key3":"3"})",
            ExtractBiddingSignals(signals1.get(), kKeys1));
  auto priority_vector = signals1->GetPerGroupData("name1")->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"foo", 1}}),
            *priority_vector);

  run_loop2.Run();
  EXPECT_FALSE(error_msg2);
  ASSERT_TRUE(signals2);
  EXPECT_EQ(R"({"key2":[2],"key3":"3"})",
            ExtractBiddingSignals(signals2.get(), kKeys2));
  priority_vector = signals2->GetPerGroupData("name2")->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"foo", 2}}),
            *priority_vector);

  ASSERT_EQ(signals1, signals2);
  task_environment_.RunUntilIdle();
  EXPECT_THAT(auction_network_events_handler_.GetObservedRequests(),
              testing::UnorderedElementsAre("Sent URL: " + kSignalsUrl,
                                            "Received URL: " + kSignalsUrl,
                                            "Completion Status: net::OK"));
}

// Test two requests are issued in one request when their joint URL length is
// smaller than either of their limits:
// Request A has a limit of 0.
// Request B has a limit of 1000.
// The combined URL length of requests A and B is 208.
TEST_F(TrustedSignalsRequestManagerTest, ScoringSignalsJointBatchedRequests) {
  // Use partially overlapping keys, to cover both the shared and distinct
  // cases.
  const GURL kRenderUrl1 = GURL("https://foo.test/");
  const std::vector<std::string> kAdComponentRenderUrls1{
      "https://foosub.test/", "https://bazsub.test/"};

  const GURL kRenderUrl2 = GURL("https://bar.test/");
  const std::vector<std::string> kAdComponentRenderUrls2{
      "https://barsub.test/", "https://bazsub.test/"};

  const std::string kUrl =
      "https://url.test/?hostname=publisher"
      "&renderUrls=https%3A%2F%2Fbar.test%2F,https%3A%2F%2Ffoo.test%2F"
      "&adComponentRenderUrls=https%3A%2F%2Fbarsub.test%2F,"
      "https%3A%2F%2Fbazsub.test%2F,https%3A%2F%2Ffoosub.test%2F";

  base::RunLoop run_loop1;
  scoped_refptr<TrustedSignals::Result> signals1;
  std::optional<std::string> error_msg1;

  auto request1 = scoring_request_manager_.RequestScoringSignals(
      kRenderUrl1, kAdComponentRenderUrls1,
      /*max_trusted_scoring_signals_url_length=*/0,
      base::BindOnce(&LoadSignalsCallback, &signals1, &error_msg1,
                     run_loop1.QuitClosure()));

  base::RunLoop run_loop2;
  scoped_refptr<TrustedSignals::Result> signals2;
  std::optional<std::string> error_msg2;
  auto request2 = scoring_request_manager_.RequestScoringSignals(
      kRenderUrl2, kAdComponentRenderUrls2,
      /*max_trusted_scoring_signals_url_length=*/1000,
      base::BindOnce(&LoadSignalsCallback, &signals2, &error_msg2,
                     run_loop2.QuitClosure()));

  AddJsonResponse(&url_loader_factory_, GURL(kUrl), kBaseScoringJson);
  scoring_request_manager_.StartBatchedTrustedSignalsRequest();

  run_loop1.Run();
  EXPECT_FALSE(error_msg1);
  ASSERT_TRUE(signals1);
  EXPECT_EQ(R"({"renderURL":{"https://foo.test/":1},)"
            R"("renderUrl":{"https://foo.test/":1},)"
            R"("adComponentRenderURLs":{"https://foosub.test/":2,)"
            R"("https://bazsub.test/":"4"},)"
            R"("adComponentRenderUrls":{"https://foosub.test/":2,)"
            R"("https://bazsub.test/":"4"}})",
            ExtractScoringSignals(signals1.get(), kRenderUrl1,
                                  kAdComponentRenderUrls1));

  run_loop2.Run();
  EXPECT_FALSE(error_msg2);
  ASSERT_TRUE(signals2);
  EXPECT_EQ(R"({"renderURL":{"https://bar.test/":[2]},)"
            R"("renderUrl":{"https://bar.test/":[2]},)"
            R"("adComponentRenderURLs":{"https://barsub.test/":[3],)"
            R"("https://bazsub.test/":"4"},)"
            R"("adComponentRenderUrls":{"https://barsub.test/":[3],)"
            R"("https://bazsub.test/":"4"}})",
            ExtractScoringSignals(signals2.get(), kRenderUrl2,
                                  kAdComponentRenderUrls2));

  ASSERT_EQ(signals1, signals2);
  task_environment_.RunUntilIdle();
  EXPECT_THAT(auction_network_events_handler_.GetObservedRequests(),
              testing::UnorderedElementsAre("Sent URL: " + kUrl,
                                            "Received URL: " + kUrl,
                                            "Completion Status: net::OK"));
}

// Test two requests are issued separately in two requests when their joint URL
// length is bigger than either of their limits:
// Request A has a limit of 130.
// Request B has a limit of 130.
// The combined URL length of requests A and B is 131.
TEST_F(TrustedSignalsRequestManagerTest, BiddingSignalsSplitBatchedRequests) {
  const std::vector<std::string> kKeys1{"key1", "key3"};
  const std::string kUrl1 =
      "https://url.test/?hostname=publisher"
      "&keys=key1,key3&interestGroupNames=name1"
      "&trusted_bidding_signals_slot_size_param=foo";

  const std::vector<std::string> kKeys2{"key2", "key3"};
  const std::string kUrl2 =
      "https://url.test/?hostname=publisher"
      "&keys=key2,key3&interestGroupNames=name2"
      "&trusted_bidding_signals_slot_size_param=foo";

  base::RunLoop run_loop1;
  scoped_refptr<TrustedSignals::Result> signals1;
  std::optional<std::string> error_msg1;

  auto request1 = bidding_request_manager_.RequestBiddingSignals(
      {"name1"}, kKeys1, /*max_trusted_bidding_signals_url_length=*/130,
      base::BindOnce(&LoadSignalsCallback, &signals1, &error_msg1,
                     run_loop1.QuitClosure()));

  base::RunLoop run_loop2;
  scoped_refptr<TrustedSignals::Result> signals2;
  std::optional<std::string> error_msg2;
  auto request2 = bidding_request_manager_.RequestBiddingSignals(
      {"name2"}, kKeys2, /*max_trusted_bidding_signals_url_length=*/130,
      base::BindOnce(&LoadSignalsCallback, &signals2, &error_msg2,
                     run_loop2.QuitClosure()));

  AddBidderJsonResponse(&url_loader_factory_, GURL(kUrl1),
                        R"({"keys":{"key1":1,"key3":3},
                            "perInterestGroupData":
                                {"name1": {"priorityVector": {"foo": 1}}}
                            })");
  AddBidderJsonResponse(&url_loader_factory_, GURL(kUrl2),
                        R"({"keys":{"key2":[2],"key3":[3]},
                            "perInterestGroupData":
                                {"name2": {"priorityVector": {"foo": 2}}}
                            })");
  bidding_request_manager_.StartBatchedTrustedSignalsRequest();

  run_loop1.Run();
  EXPECT_FALSE(error_msg1);
  ASSERT_TRUE(signals1);
  EXPECT_EQ(R"({"key1":1,"key3":3})",
            ExtractBiddingSignals(signals1.get(), kKeys1));
  auto priority_vector = signals1->GetPerGroupData("name1")->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"foo", 1}}),
            *priority_vector);

  run_loop2.Run();
  EXPECT_FALSE(error_msg2);
  ASSERT_TRUE(signals2);
  EXPECT_EQ(R"({"key2":[2],"key3":[3]})",
            ExtractBiddingSignals(signals2.get(), kKeys2));
  priority_vector = signals2->GetPerGroupData("name2")->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"foo", 2}}),
            *priority_vector);

  ASSERT_NE(signals1, signals2);
  task_environment_.RunUntilIdle();
  EXPECT_THAT(auction_network_events_handler_.GetObservedRequests(),
              testing::UnorderedElementsAre(
                  "Sent URL: " + kUrl1, "Received URL: " + kUrl1,
                  "Completion Status: net::OK", "Sent URL: " + kUrl2,
                  "Received URL: " + kUrl2, "Completion Status: net::OK"));
}

// Test two requests are issued in one request when their joint URL length is
// smaller than either of their limits:
// Request A has a limit of 200.
// Request B has a limit of 200.
// The combined URL length of requests A and B is 208.
TEST_F(TrustedSignalsRequestManagerTest, ScoringSignalsSplitBatchedRequests) {
  // Use partially overlapping keys, to cover both the shared and distinct
  // cases.
  const GURL kRenderUrl1 = GURL("https://foo.test/");
  const std::vector<std::string> kAdComponentRenderUrls1{
      "https://foosub.test/", "https://bazsub.test/"};

  const GURL kRenderUrl2 = GURL("https://bar.test/");
  const std::vector<std::string> kAdComponentRenderUrls2{
      "https://barsub.test/", "https://bazsub.test/"};

  const std::string kUrl1 =
      "https://url.test/"
      "?hostname=publisher&renderUrls=https%3A%2F%2Fbar.test%2F&"
      "adComponentRenderUrls=https%3A%2F%2Fbarsub.test%2F,https%3A%2F%2Fbazsub."
      "test%2F";

  const std::string kUrl2 =
      "https://url.test/"
      "?hostname=publisher&renderUrls=https%3A%2F%2Ffoo.test%2F&"
      "adComponentRenderUrls=https%3A%2F%2Fbazsub.test%2F,https%3A%2F%2Ffoosub."
      "test%2F";

  base::RunLoop run_loop1;
  scoped_refptr<TrustedSignals::Result> signals1;
  std::optional<std::string> error_msg1;

  auto request1 = scoring_request_manager_.RequestScoringSignals(
      kRenderUrl1, kAdComponentRenderUrls1,
      /*max_trusted_scoring_signals_url_length=*/200,
      base::BindOnce(&LoadSignalsCallback, &signals1, &error_msg1,
                     run_loop1.QuitClosure()));

  base::RunLoop run_loop2;
  scoped_refptr<TrustedSignals::Result> signals2;
  std::optional<std::string> error_msg2;
  auto request2 = scoring_request_manager_.RequestScoringSignals(
      kRenderUrl2, kAdComponentRenderUrls2,
      /*max_trusted_scoring_signals_url_length=*/200,
      base::BindOnce(&LoadSignalsCallback, &signals2, &error_msg2,
                     run_loop2.QuitClosure()));

  AddJsonResponse(&url_loader_factory_, GURL(kUrl1), kBaseScoringJson);
  AddJsonResponse(&url_loader_factory_, GURL(kUrl2), kBaseScoringJson);
  scoring_request_manager_.StartBatchedTrustedSignalsRequest();

  run_loop1.Run();
  EXPECT_FALSE(error_msg1);
  ASSERT_TRUE(signals1);
  EXPECT_EQ(R"({"renderURL":{"https://foo.test/":1},)"
            R"("renderUrl":{"https://foo.test/":1},)"
            R"("adComponentRenderURLs":{"https://foosub.test/":2,)"
            R"("https://bazsub.test/":"4"},)"
            R"("adComponentRenderUrls":{"https://foosub.test/":2,)"
            R"("https://bazsub.test/":"4"}})",
            ExtractScoringSignals(signals1.get(), kRenderUrl1,
                                  kAdComponentRenderUrls1));

  run_loop2.Run();
  EXPECT_FALSE(error_msg2);
  ASSERT_TRUE(signals2);
  EXPECT_EQ(R"({"renderURL":{"https://bar.test/":[2]},)"
            R"("renderUrl":{"https://bar.test/":[2]},)"
            R"("adComponentRenderURLs":{"https://barsub.test/":[3],)"
            R"("https://bazsub.test/":"4"},)"
            R"("adComponentRenderUrls":{"https://barsub.test/":[3],)"
            R"("https://bazsub.test/":"4"}})",
            ExtractScoringSignals(signals2.get(), kRenderUrl2,
                                  kAdComponentRenderUrls2));

  ASSERT_NE(signals1, signals2);
  task_environment_.RunUntilIdle();
  EXPECT_THAT(auction_network_events_handler_.GetObservedRequests(),
              testing::UnorderedElementsAre(
                  "Sent URL: " + kUrl1, "Received URL: " + kUrl1,
                  "Completion Status: net::OK", "Sent URL: " + kUrl2,
                  "Received URL: " + kUrl2, "Completion Status: net::OK"));
}

// Test whether three requests are issued in two requests: request A and B can
// be issued together because adding request C will result in an oversized URL.
// Request A has a limit of 0.
// Request B has a limit of 1000.
// Request C has a limit of 130.
// The combined URL length of requests A and B is 131.
// The combined URL length of requests A, B and C is 137.
TEST_F(TrustedSignalsRequestManagerTest,
       BiddingSignalsPartlyJointBatchedRequests1) {
  const std::vector<std::string> kKeys1{"key1", "key3"};
  const std::vector<std::string> kKeys2{"key2", "key3"};
  const std::vector<std::string> kKeys3{"key1", "key2"};

  const std::string kUrl1 =
      "https://url.test/?hostname=publisher"
      "&keys=key1,key2,key3&interestGroupNames=name1,name2"
      "&trusted_bidding_signals_slot_size_param=foo";
  const std::string kUrl2 =
      "https://url.test/?hostname=publisher"
      "&keys=key1,key2&interestGroupNames=name3"
      "&trusted_bidding_signals_slot_size_param=foo";

  base::RunLoop run_loop1;
  scoped_refptr<TrustedSignals::Result> signals1;
  std::optional<std::string> error_msg1;

  auto request1 = bidding_request_manager_.RequestBiddingSignals(
      {"name1"}, kKeys1, /*max_trusted_bidding_signals_url_length=*/0,
      base::BindOnce(&LoadSignalsCallback, &signals1, &error_msg1,
                     run_loop1.QuitClosure()));

  base::RunLoop run_loop2;
  scoped_refptr<TrustedSignals::Result> signals2;
  std::optional<std::string> error_msg2;
  auto request2 = bidding_request_manager_.RequestBiddingSignals(
      {"name2"}, kKeys2, /*max_trusted_bidding_signals_url_length=*/1000,
      base::BindOnce(&LoadSignalsCallback, &signals2, &error_msg2,
                     run_loop2.QuitClosure()));

  base::RunLoop run_loop3;
  scoped_refptr<TrustedSignals::Result> signals3;
  std::optional<std::string> error_msg3;
  auto request3 = bidding_request_manager_.RequestBiddingSignals(
      {"name3"}, kKeys3, /*max_trusted_bidding_signals_url_length=*/130,
      base::BindOnce(&LoadSignalsCallback, &signals3, &error_msg3,
                     run_loop3.QuitClosure()));

  AddBidderJsonResponse(&url_loader_factory_, GURL(kUrl1), kBaseBiddingJson);
  AddBidderJsonResponse(&url_loader_factory_, GURL(kUrl2),
                        R"({"keys":{"key1":[2],"key2":[3]},
                            "perInterestGroupData":
                                {"name3": {"priorityVector": {"foo": 3}}}
                            })");
  bidding_request_manager_.StartBatchedTrustedSignalsRequest();

  run_loop1.Run();
  EXPECT_FALSE(error_msg1);
  ASSERT_TRUE(signals1);
  EXPECT_EQ(R"({"key1":1,"key3":"3"})",
            ExtractBiddingSignals(signals1.get(), kKeys1));
  auto priority_vector = signals1->GetPerGroupData("name1")->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"foo", 1}}),
            *priority_vector);

  run_loop2.Run();
  EXPECT_FALSE(error_msg2);
  ASSERT_TRUE(signals2);
  EXPECT_EQ(R"({"key2":[2],"key3":"3"})",
            ExtractBiddingSignals(signals2.get(), kKeys2));
  priority_vector = signals2->GetPerGroupData("name2")->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"foo", 2}}),
            *priority_vector);

  run_loop3.Run();
  EXPECT_FALSE(error_msg3);
  ASSERT_TRUE(signals3);
  EXPECT_EQ(R"({"key1":[2],"key2":[3]})",
            ExtractBiddingSignals(signals3.get(), kKeys3));
  priority_vector = signals3->GetPerGroupData("name3")->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"foo", 3}}),
            *priority_vector);

  ASSERT_EQ(signals1, signals2);
  ASSERT_NE(signals2, signals3);
  task_environment_.RunUntilIdle();
  EXPECT_THAT(auction_network_events_handler_.GetObservedRequests(),
              testing::UnorderedElementsAre(
                  "Sent URL: " + kUrl1, "Received URL: " + kUrl1,
                  "Completion Status: net::OK", "Sent URL: " + kUrl2,
                  "Received URL: " + kUrl2, "Completion Status: net::OK"));
}

// Test whether three requests are issued in two requests: request A and B can
// be issued together because adding request C will result in an oversized URL.
// Request A has a limit of 0.
// Request B has a limit of 1000.
// Request C has a limit of 200.
// The combined URL length of requests A and B is 208.
// The combined URL length of requests A, B and C is 234.
TEST_F(TrustedSignalsRequestManagerTest,
       ScoringSignalsPartlyJointBatchedRequests1) {
  const GURL kRenderUrl1 = GURL("https://bar.test/");
  const std::vector<std::string> kAdComponentRenderUrls1{
      "https://foosub.test/", "https://bazsub.test/"};

  const GURL kRenderUrl2 = GURL("https://baz.test/");
  const std::vector<std::string> kAdComponentRenderUrls2{
      "https://barsub.test/", "https://bazsub.test/"};

  const GURL kRenderUrl3 = GURL("https://foo.test/");
  const std::vector<std::string> kAdComponentRenderUrls3{
      "https://foosub.test/", "https://barsub.test/"};

  const std::string kUrl1 =
      "https://url.test/"
      "?hostname=publisher&renderUrls=https%3A%2F%2Fbar.test%2F,https%3A%2F%"
      "2Fbaz.test%2F&adComponentRenderUrls=https%3A%2F%2Fbarsub.test%2F,https%"
      "3A%2F%2Fbazsub.test%2F,https%3A%2F%2Ffoosub.test%2F";
  const std::string kUrl2 =
      "https://url.test/"
      "?hostname=publisher&renderUrls=https%3A%2F%2Ffoo.test%2F&"
      "adComponentRenderUrls=https%3A%2F%2Fbarsub.test%2F,https%3A%2F%2Ffoosub."
      "test%2F";

  base::RunLoop run_loop1;
  scoped_refptr<TrustedSignals::Result> signals1;
  std::optional<std::string> error_msg1;

  auto request1 = scoring_request_manager_.RequestScoringSignals(
      kRenderUrl1, kAdComponentRenderUrls1,
      /*max_trusted_scoring_signals_url_length=*/0,
      base::BindOnce(&LoadSignalsCallback, &signals1, &error_msg1,
                     run_loop1.QuitClosure()));

  base::RunLoop run_loop2;
  scoped_refptr<TrustedSignals::Result> signals2;
  std::optional<std::string> error_msg2;
  auto request2 = scoring_request_manager_.RequestScoringSignals(
      kRenderUrl2, kAdComponentRenderUrls2,
      /*max_trusted_scoring_signals_url_length=*/1000,
      base::BindOnce(&LoadSignalsCallback, &signals2, &error_msg2,
                     run_loop2.QuitClosure()));

  base::RunLoop run_loop3;
  scoped_refptr<TrustedSignals::Result> signals3;
  std::optional<std::string> error_msg3;
  auto request3 = scoring_request_manager_.RequestScoringSignals(
      kRenderUrl3, kAdComponentRenderUrls3,
      /*max_trusted_scoring_signals_url_length=*/200,
      base::BindOnce(&LoadSignalsCallback, &signals3, &error_msg3,
                     run_loop3.QuitClosure()));

  AddBidderJsonResponse(&url_loader_factory_, GURL(kUrl1), kBaseScoringJson);
  AddBidderJsonResponse(&url_loader_factory_, GURL(kUrl2), kBaseScoringJson);
  scoring_request_manager_.StartBatchedTrustedSignalsRequest();

  run_loop1.Run();
  EXPECT_FALSE(error_msg1);
  ASSERT_TRUE(signals1);
  EXPECT_EQ(R"({"renderURL":{"https://bar.test/":[2]},)"
            R"("renderUrl":{"https://bar.test/":[2]},)"
            R"("adComponentRenderURLs":{"https://foosub.test/":2,)"
            R"("https://bazsub.test/":"4"},)"
            R"("adComponentRenderUrls":{"https://foosub.test/":2,)"
            R"("https://bazsub.test/":"4"}})",
            ExtractScoringSignals(signals1.get(), kRenderUrl1,
                                  kAdComponentRenderUrls1));

  run_loop2.Run();
  EXPECT_FALSE(error_msg2);
  ASSERT_TRUE(signals2);
  EXPECT_EQ(R"({"renderURL":{"https://baz.test/":null},)"
            R"("renderUrl":{"https://baz.test/":null},)"
            R"("adComponentRenderURLs":{"https://barsub.test/":[3],)"
            R"("https://bazsub.test/":"4"},)"
            R"("adComponentRenderUrls":{"https://barsub.test/":[3],)"
            R"("https://bazsub.test/":"4"}})",
            ExtractScoringSignals(signals2.get(), kRenderUrl2,
                                  kAdComponentRenderUrls2));

  run_loop3.Run();
  EXPECT_FALSE(error_msg3);
  ASSERT_TRUE(signals3);
  EXPECT_EQ(R"({"renderURL":{"https://foo.test/":1},)"
            R"("renderUrl":{"https://foo.test/":1},)"
            R"("adComponentRenderURLs":{"https://foosub.test/":2,)"
            R"("https://barsub.test/":[3]},)"
            R"("adComponentRenderUrls":{"https://foosub.test/":2,)"
            R"("https://barsub.test/":[3]}})",
            ExtractScoringSignals(signals3.get(), kRenderUrl3,
                                  kAdComponentRenderUrls3));

  ASSERT_EQ(signals1, signals2);
  ASSERT_NE(signals2, signals3);
  task_environment_.RunUntilIdle();
  EXPECT_THAT(auction_network_events_handler_.GetObservedRequests(),
              testing::UnorderedElementsAre(
                  "Sent URL: " + kUrl1, "Received URL: " + kUrl1,
                  "Completion Status: net::OK", "Sent URL: " + kUrl2,
                  "Received URL: " + kUrl2, "Completion Status: net::OK"));
}

// Test whether three signal requests are issued in two fetch requests: request
// A is issued individually, because requests A and B will result an oversized
// URL for B's limit.
// Request A has a limit of 0. Request B has a limit of 131.
// Request C has a limit of 131.
// The combined URL length of requests A and B is 143.
// The combined URL length of requests B and C is 131.
TEST_F(TrustedSignalsRequestManagerTest,
       BiddingSignalsPartlyJointBatchedRequests2) {
  const std::vector<std::string> kKeys1{"key1", "key3"};
  const std::vector<std::string> kKeys2{"key2", "key3"};
  const std::vector<std::string> kKeys3{"key1", "key2"};

  const std::string kUrl1 =
      "https://url.test/?hostname=publisher"
      "&keys=key1,key3&interestGroupNames=extremelyLongName"
      "&trusted_bidding_signals_slot_size_param=foo";
  const std::string kUrl2 =
      "https://url.test/?hostname=publisher"
      "&keys=key1,key2,key3&interestGroupNames=name1,name2"
      "&trusted_bidding_signals_slot_size_param=foo";

  base::RunLoop run_loop3;
  scoped_refptr<TrustedSignals::Result> signals3;
  std::optional<std::string> error_msg3;
  auto request3 = bidding_request_manager_.RequestBiddingSignals(
      {"name2"}, kKeys3, /*max_trusted_bidding_signals_url_length=*/131,
      base::BindOnce(&LoadSignalsCallback, &signals3, &error_msg3,
                     run_loop3.QuitClosure()));

  base::RunLoop run_loop1;
  scoped_refptr<TrustedSignals::Result> signals1;
  std::optional<std::string> error_msg1;

  auto request1 = bidding_request_manager_.RequestBiddingSignals(
      {"extremelyLongName"}, kKeys1,
      /*max_trusted_bidding_signals_url_length=*/0,
      base::BindOnce(&LoadSignalsCallback, &signals1, &error_msg1,
                     run_loop1.QuitClosure()));

  base::RunLoop run_loop2;
  scoped_refptr<TrustedSignals::Result> signals2;
  std::optional<std::string> error_msg2;
  auto request2 = bidding_request_manager_.RequestBiddingSignals(
      {"name1"}, kKeys2, /*max_trusted_bidding_signals_url_length=*/131,
      base::BindOnce(&LoadSignalsCallback, &signals2, &error_msg2,
                     run_loop2.QuitClosure()));

  AddBidderJsonResponse(&url_loader_factory_, GURL(kUrl1),
                        R"({"keys":{"key1":[2],"key3":[3]},
                            "perInterestGroupData":
                                {"extremelyLongName":
                                    {"priorityVector": {"foo": 3}}}
                            })");
  AddBidderJsonResponse(&url_loader_factory_, GURL(kUrl2), kBaseBiddingJson);
  bidding_request_manager_.StartBatchedTrustedSignalsRequest();

  run_loop1.Run();
  EXPECT_FALSE(error_msg1);
  ASSERT_TRUE(signals1);
  EXPECT_EQ(R"({"key1":[2],"key3":[3]})",
            ExtractBiddingSignals(signals1.get(), kKeys1));
  auto priority_vector =
      signals1->GetPerGroupData("extremelyLongName")->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"foo", 3}}),
            *priority_vector);

  run_loop2.Run();
  EXPECT_FALSE(error_msg2);
  ASSERT_TRUE(signals2);
  EXPECT_EQ(R"({"key2":[2],"key3":"3"})",
            ExtractBiddingSignals(signals2.get(), kKeys2));
  priority_vector = signals2->GetPerGroupData("name1")->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"foo", 1}}),
            *priority_vector);

  run_loop3.Run();
  EXPECT_FALSE(error_msg3);
  ASSERT_TRUE(signals3);
  EXPECT_EQ(R"({"key1":1,"key2":[2]})",
            ExtractBiddingSignals(signals3.get(), kKeys3));
  priority_vector = signals3->GetPerGroupData("name2")->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"foo", 2}}),
            *priority_vector);

  ASSERT_NE(signals1, signals2);
  ASSERT_EQ(signals2, signals3);
  task_environment_.RunUntilIdle();
  EXPECT_THAT(auction_network_events_handler_.GetObservedRequests(),
              testing::UnorderedElementsAre(
                  "Sent URL: " + kUrl1, "Received URL: " + kUrl1,
                  "Completion Status: net::OK", "Sent URL: " + kUrl2,
                  "Received URL: " + kUrl2, "Completion Status: net::OK"));
}

// Test whether three signal requests are issued in two fetch requests: request
// A is issued individually, because requests A and B will result an oversized
// URL for B's limit.
// Request A has a limit of 0. Request B has a limit of 208.
// Request C has a limit of 208.
// The combined URL length of requests A and B is 221.
// The combined URL length of requests B and C is 208.
TEST_F(TrustedSignalsRequestManagerTest,
       ScoringSignalsPartlyJointBatchedRequests2) {
  const GURL kRenderUrl1 = GURL("https://barExtremelyLong.test/");
  const std::vector<std::string> kAdComponentRenderUrls1{
      "https://foosub.test/", "https://bazsub.test/"};

  const GURL kRenderUrl2 = GURL("https://baz.test/");
  const std::vector<std::string> kAdComponentRenderUrls2{
      "https://barsub.test/", "https://bazsub.test/"};

  const GURL kRenderUrl3 = GURL("https://foo.test/");
  const std::vector<std::string> kAdComponentRenderUrls3{
      "https://foosub.test/", "https://barsub.test/"};

  const std::string kUrl1 =
      "https://url.test/"
      "?hostname=publisher&renderUrls=https%3A%2F%2Fbarextremelylong.test%2F&"
      "adComponentRenderUrls=https%3A%2F%2Fbazsub.test%2F,https%3A%2F%2Ffoosub."
      "test%2F";
  const std::string kUrl2 =
      "https://url.test/"
      "?hostname=publisher&renderUrls=https%3A%2F%2Fbaz.test%2F,https%3A%2F%"
      "2Ffoo.test%2F&adComponentRenderUrls=https%3A%2F%2Fbarsub.test%2F,https%"
      "3A%2F%2Fbazsub.test%2F,https%3A%2F%2Ffoosub.test%2F";

  base::RunLoop run_loop3;
  scoped_refptr<TrustedSignals::Result> signals3;
  std::optional<std::string> error_msg3;
  auto request3 = scoring_request_manager_.RequestScoringSignals(
      kRenderUrl3, kAdComponentRenderUrls3,
      /*max_trusted_scoring_signals_url_length=*/208,
      base::BindOnce(&LoadSignalsCallback, &signals3, &error_msg3,
                     run_loop3.QuitClosure()));

  base::RunLoop run_loop1;
  scoped_refptr<TrustedSignals::Result> signals1;
  std::optional<std::string> error_msg1;

  auto request1 = scoring_request_manager_.RequestScoringSignals(
      kRenderUrl1, kAdComponentRenderUrls1,
      /*max_trusted_scoring_signals_url_length=*/0,
      base::BindOnce(&LoadSignalsCallback, &signals1, &error_msg1,
                     run_loop1.QuitClosure()));

  base::RunLoop run_loop2;
  scoped_refptr<TrustedSignals::Result> signals2;
  std::optional<std::string> error_msg2;
  auto request2 = scoring_request_manager_.RequestScoringSignals(
      kRenderUrl2, kAdComponentRenderUrls2,
      /*max_trusted_scoring_signals_url_length=*/208,
      base::BindOnce(&LoadSignalsCallback, &signals2, &error_msg2,
                     run_loop2.QuitClosure()));

  AddBidderJsonResponse(&url_loader_factory_, GURL(kUrl1), kBaseScoringJson);
  AddBidderJsonResponse(&url_loader_factory_, GURL(kUrl2), kBaseScoringJson);
  scoring_request_manager_.StartBatchedTrustedSignalsRequest();

  run_loop1.Run();
  EXPECT_FALSE(error_msg1);
  ASSERT_TRUE(signals1);
  EXPECT_EQ(R"({"renderURL":{"https://barextremelylong.test/":null},)"
            R"("renderUrl":{"https://barextremelylong.test/":null},)"
            R"("adComponentRenderURLs":{"https://foosub.test/":2,)"
            R"("https://bazsub.test/":"4"},)"
            R"("adComponentRenderUrls":{"https://foosub.test/":2,)"
            R"("https://bazsub.test/":"4"}})",
            ExtractScoringSignals(signals1.get(), kRenderUrl1,
                                  kAdComponentRenderUrls1));

  run_loop2.Run();
  EXPECT_FALSE(error_msg2);
  ASSERT_TRUE(signals2);
  EXPECT_EQ(R"({"renderURL":{"https://baz.test/":null},)"
            R"("renderUrl":{"https://baz.test/":null},)"
            R"("adComponentRenderURLs":{"https://barsub.test/":[3],)"
            R"("https://bazsub.test/":"4"},)"
            R"("adComponentRenderUrls":{"https://barsub.test/":[3],)"
            R"("https://bazsub.test/":"4"}})",
            ExtractScoringSignals(signals2.get(), kRenderUrl2,
                                  kAdComponentRenderUrls2));

  run_loop3.Run();
  EXPECT_FALSE(error_msg3);
  ASSERT_TRUE(signals3);
  EXPECT_EQ(R"({"renderURL":{"https://foo.test/":1},)"
            R"("renderUrl":{"https://foo.test/":1},)"
            R"("adComponentRenderURLs":{"https://foosub.test/":2,)"
            R"("https://barsub.test/":[3]},)"
            R"("adComponentRenderUrls":{"https://foosub.test/":2,)"
            R"("https://barsub.test/":[3]}})",
            ExtractScoringSignals(signals3.get(), kRenderUrl3,
                                  kAdComponentRenderUrls3));

  ASSERT_NE(signals1, signals2);
  ASSERT_EQ(signals2, signals3);
  task_environment_.RunUntilIdle();
  EXPECT_THAT(auction_network_events_handler_.GetObservedRequests(),
              testing::UnorderedElementsAre(
                  "Sent URL: " + kUrl1, "Received URL: " + kUrl1,
                  "Completion Status: net::OK", "Sent URL: " + kUrl2,
                  "Received URL: " + kUrl2, "Completion Status: net::OK"));
}

// Test two identical signal requests with same interest group name and no
// bidder keys will result two separate fetch request.
// Request A has a limit of 104.
// Request B has a limit of 104.
TEST_F(TrustedSignalsRequestManagerTest, BiddingSignalsIdenticalRequests) {
  const std::string kUrl =
      "https://url.test/?hostname=publisher"
      "&interestGroupNames=name"
      "&trusted_bidding_signals_slot_size_param=foo";

  base::RunLoop run_loop1;
  scoped_refptr<TrustedSignals::Result> signals1;
  std::optional<std::string> error_msg1;

  auto request1 = bidding_request_manager_.RequestBiddingSignals(
      {"name"}, std::nullopt, /*max_trusted_bidding_signals_url_length=*/100,
      base::BindOnce(&LoadSignalsCallback, &signals1, &error_msg1,
                     run_loop1.QuitClosure()));

  base::RunLoop run_loop2;
  scoped_refptr<TrustedSignals::Result> signals2;
  std::optional<std::string> error_msg2;
  auto request2 = bidding_request_manager_.RequestBiddingSignals(
      {"name"}, std::nullopt, /*max_trusted_bidding_signals_url_length=*/100,
      base::BindOnce(&LoadSignalsCallback, &signals2, &error_msg2,
                     run_loop2.QuitClosure()));

  AddBidderJsonResponse(&url_loader_factory_, GURL(kUrl),
                        R"({"perInterestGroupData":
                                {"name": {"priorityVector": {"foo": 1}}}
                            })");
  bidding_request_manager_.StartBatchedTrustedSignalsRequest();

  run_loop1.Run();
  EXPECT_FALSE(error_msg1);
  ASSERT_TRUE(signals1);
  auto priority_vector = signals1->GetPerGroupData("name")->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"foo", 1}}),
            *priority_vector);

  run_loop2.Run();
  EXPECT_FALSE(error_msg2);
  ASSERT_TRUE(signals2);
  priority_vector = signals2->GetPerGroupData("name")->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"foo", 1}}),
            *priority_vector);

  ASSERT_NE(signals1, signals2);
  task_environment_.RunUntilIdle();
  EXPECT_THAT(auction_network_events_handler_.GetObservedRequests(),
              testing::UnorderedElementsAre(
                  "Sent URL: " + kUrl, "Received URL: " + kUrl,
                  "Completion Status: net::OK", "Sent URL: " + kUrl,
                  "Received URL: " + kUrl, "Completion Status: net::OK"));
}

// Test two identical signal requests with same render url and no
// ad component urls will result two separate fetch request.
// Request A has a limit of 73.
// Request B has a limit of 73.
TEST_F(TrustedSignalsRequestManagerTest, ScoringSignalsIdenticalRequests) {
  // Use partially overlapping keys, to cover both the shared and distinct
  // cases.
  const GURL kRenderUrl = GURL("https://foo.test/");

  const std::string kUrl =
      "https://url.test/"
      "?hostname=publisher&renderUrls=https%3A%2F%2Ffoo.test%2F";

  base::RunLoop run_loop1;
  scoped_refptr<TrustedSignals::Result> signals1;
  std::optional<std::string> error_msg1;

  auto request1 = scoring_request_manager_.RequestScoringSignals(
      kRenderUrl, {},
      /*max_trusted_scoring_signals_url_length=*/50,
      base::BindOnce(&LoadSignalsCallback, &signals1, &error_msg1,
                     run_loop1.QuitClosure()));

  base::RunLoop run_loop2;
  scoped_refptr<TrustedSignals::Result> signals2;
  std::optional<std::string> error_msg2;
  auto request2 = scoring_request_manager_.RequestScoringSignals(
      kRenderUrl, {},
      /*max_trusted_scoring_signals_url_length=*/50,
      base::BindOnce(&LoadSignalsCallback, &signals2, &error_msg2,
                     run_loop2.QuitClosure()));

  AddJsonResponse(&url_loader_factory_, GURL(kUrl), kBaseScoringJson);
  scoring_request_manager_.StartBatchedTrustedSignalsRequest();

  run_loop1.Run();
  EXPECT_FALSE(error_msg1);
  ASSERT_TRUE(signals1);
  EXPECT_EQ(R"({"renderURL":{"https://foo.test/":1},)"
            R"("renderUrl":{"https://foo.test/":1}})",
            ExtractScoringSignals(signals1.get(), kRenderUrl, {}));

  run_loop2.Run();
  EXPECT_FALSE(error_msg2);
  ASSERT_TRUE(signals2);
  EXPECT_EQ(R"({"renderURL":{"https://foo.test/":1},)"
            R"("renderUrl":{"https://foo.test/":1}})",
            ExtractScoringSignals(signals2.get(), kRenderUrl, {}));

  ASSERT_NE(signals1, signals2);
  task_environment_.RunUntilIdle();
  EXPECT_THAT(auction_network_events_handler_.GetObservedRequests(),
              testing::UnorderedElementsAre(
                  "Sent URL: " + kUrl, "Received URL: " + kUrl,
                  "Completion Status: net::OK", "Sent URL: " + kUrl,
                  "Received URL: " + kUrl, "Completion Status: net::OK"));
}

class TrustedSignalsRequestManagerKVv2Test
    : public TrustedSignalsRequestManagerTest {
 public:
  TrustedSignalsRequestManagerKVv2Test()
      : TrustedSignalsRequestManagerTest(CreateDefaultPublicKey()) {
    feature_list_.InitWithFeatures(
        {blink::features::kFledgeTrustedSignalsKVv2Support}, {});
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  url::Origin joining_origin_{url::Origin::Create(GURL(kJoiningOriginA))};
  blink::mojom::InterestGroup::ExecutionMode execution_mode_{
      blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode};
};

// Make two requests, try to start a network request, then cancel both requests.
// The network request should be cancelled. Only test bidders, since sellers
// have no significant differences in this path.
TEST_F(TrustedSignalsRequestManagerKVv2Test, CancelAllLiveRequests) {
  const std::vector<std::string> kKeys1{"key1"};
  const std::vector<std::string> kKeys2{"key2"};

  auto request1 = bidding_request_manager_.RequestKVv2BiddingSignals(
      {"name1"}, kKeys1, joining_origin_, execution_mode_,
      base::BindOnce(&NeverInvokedLoadSignalsCallback));
  auto request2 = bidding_request_manager_.RequestKVv2BiddingSignals(
      {"name1"}, kKeys2, joining_origin_, execution_mode_,
      base::BindOnce(&NeverInvokedLoadSignalsCallback));

  // Wait for network request to be made, which should include both keys in the
  // URLs.
  bidding_request_manager_.StartBatchedTrustedSignalsRequest();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(url_loader_factory_.IsPending(trusted_signals_url_.spec()));

  // Cancel both requests, which should cancel the network request.
  request1.reset();
  request2.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(url_loader_factory_.GetPendingRequest(0)->client.is_connected());
}

// Make two requests, cancel both, then try to start a network request. No
// requests should be made. Only test bidders, since sellers have no significant
// differences in this path.
TEST_F(TrustedSignalsRequestManagerKVv2Test, CancelAllQueuedRequests) {
  const std::vector<std::string> kKeys1{"key1"};
  const std::vector<std::string> kKeys2{"key2"};

  auto request1 = bidding_request_manager_.RequestKVv2BiddingSignals(
      {"name1"}, kKeys1, joining_origin_, execution_mode_,
      base::BindOnce(&NeverInvokedLoadSignalsCallback));
  auto request2 = bidding_request_manager_.RequestKVv2BiddingSignals(
      {"name1"}, kKeys2, joining_origin_, execution_mode_,
      base::BindOnce(&NeverInvokedLoadSignalsCallback));

  request1.reset();
  request2.reset();

  bidding_request_manager_.StartBatchedTrustedSignalsRequest();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, url_loader_factory_.NumPending());
}

class TrustedSignalsRequestManagerKVv2EmbeddedTest : public testing::Test {
 public:
  TrustedSignalsRequestManagerKVv2EmbeddedTest() {
    response_status_code_ = net::HttpStatusCode::HTTP_OK;

    feature_list_.InitWithFeatures(
        {blink::features::kFledgeTrustedSignalsKVv2Support}, {});

    embedded_test_server_.SetSSLConfig(
        net::EmbeddedTestServer::CERT_TEST_NAMES);
    embedded_test_server_.RegisterRequestHandler(base::BindRepeating(
        &TrustedSignalsRequestManagerKVv2EmbeddedTest::HandleSignalsRequest,
        base::Unretained(this)));
    EXPECT_TRUE(embedded_test_server_.Start());

    bidding_request_manager_ = std::make_unique<TrustedSignalsRequestManager>(
        TrustedSignalsRequestManager::Type::kBiddingSignals,
        url_loader_factory_.get(),
        /*auction_network_events_handler=*/
        auction_network_events_handler_.CreateRemote(),
        /*automatically_send_requests=*/false,
        url::Origin::Create(GURL(kTopLevelOrigin)), TrustedBiddingSignalsUrl(),
        /*experiment_group_id=*/std::nullopt,
        "trusted_bidding_signals_slot_size_param=foo", CreateDefaultPublicKey(),
        v8_helper_.get());

    scoring_request_manager_ = std::make_unique<TrustedSignalsRequestManager>(
        TrustedSignalsRequestManager::Type::kScoringSignals,
        url_loader_factory_.get(),
        /*auction_network_events_handler=*/
        auction_network_events_handler_.CreateRemote(),
        /*automatically_send_requests=*/false,
        url::Origin::Create(GURL(kTopLevelOrigin)), TrustedScoringSignalsUrl(),
        /*experiment_group_id=*/std::nullopt,
        /*trusted_bidding_signals_slot_size_param=*/"",
        CreateDefaultPublicKey(), v8_helper_.get());
  }

  ~TrustedSignalsRequestManagerKVv2EmbeddedTest() override {
    // Wait until idle to ensure all requests have been observed within the
    // `auction_network_events_handler_`.
    task_environment_.RunUntilIdle();

    bidding_request_manager_.reset();
    scoring_request_manager_.reset();
  }

  GURL TrustedBiddingSignalsUrl() const {
    return embedded_test_server_.GetURL(kTrustedKVv2SignalsHost,
                                        kTrustedKVv2BiddingSignalsPath);
  }

  GURL TrustedScoringSignalsUrl() const {
    return embedded_test_server_.GetURL(kTrustedKVv2SignalsHost,
                                        kTrustedKVv2ScoringSignalsPath);
  }

  // Fetch bidding signals and wait for completion. Return nullptr on failure.
  scoped_refptr<TrustedSignals::Result> FetchBiddingSignals(
      const std::string& interest_group_name,
      const std::optional<std::vector<std::string>>&
          trusted_bidding_signals_keys,
      const url::Origin& joining_origin,
      blink::mojom::InterestGroup::ExecutionMode execution_mode) {
    scoped_refptr<TrustedSignals::Result> signals;
    base::RunLoop run_loop;
    auto request = bidding_request_manager_->RequestKVv2BiddingSignals(
        interest_group_name, trusted_bidding_signals_keys, joining_origin,
        execution_mode,
        base::BindOnce(&LoadSignalsCallback, &signals, &error_msg_,
                       run_loop.QuitClosure()));
    bidding_request_manager_->StartBatchedTrustedSignalsRequest();
    run_loop.Run();
    return signals;
  }

  // Fetches scoring signals and waits for completion. Returns nullptr on
  // failure.
  scoped_refptr<TrustedSignals::Result> FetchScoringSignals(
      const GURL& render_url,
      const std::vector<std::string>& ad_component_render_urls,
      const url::Origin& bidder_owner_origin,
      const url::Origin& bidder_joining_origin) {
    scoped_refptr<TrustedSignals::Result> signals;
    base::RunLoop run_loop;
    auto request = scoring_request_manager_->RequestKVv2ScoringSignals(
        render_url, ad_component_render_urls, bidder_owner_origin,
        bidder_joining_origin,
        base::BindOnce(&LoadSignalsCallback, &signals, &error_msg_,
                       run_loop.QuitClosure()));
    scoring_request_manager_->StartBatchedTrustedSignalsRequest();
    run_loop.Run();
    return signals;
  }

  // Return the results of calling TrustedSignals::Result::GetBiddingSignals()
  // with `trusted_bidding_signals_keys`. Return value as a JSON std::string,
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

          if (v8_helper_->ExtractJson(context, value,
                                      /*script_timeout=*/nullptr, &result) !=
              AuctionV8Helper::Result::kSuccess) {
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

          if (v8_helper_->ExtractJson(context, value,
                                      /*script_timeout=*/nullptr, &result) !=
              AuctionV8Helper::Result::kSuccess) {
            result = "JSON extraction failed.";
          }
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  // Set the response status code.
  void SetResponseStatusCode(net::HttpStatusCode code) {
    base::AutoLock auto_lock(lock_);
    response_status_code_ = code;
  }

 protected:
  std::unique_ptr<net::test_server::HttpResponse> HandleSignalsRequest(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url != kTrustedKVv2BiddingSignalsPath &&
        request.relative_url != kTrustedKVv2ScoringSignalsPath) {
      return nullptr;
    }

    base::AutoLock auto_lock(lock_);

    // Decrypt the request.
    auto response_key_config = quiche::ObliviousHttpHeaderKeyConfig::Create(
        kKeyId, EVP_HPKE_DHKEM_X25519_HKDF_SHA256, EVP_HPKE_HKDF_SHA256,
        EVP_HPKE_AES_256_GCM);
    CHECK(response_key_config.ok()) << response_key_config.status();

    auto ohttp_gateway =
        quiche::ObliviousHttpGateway::Create(
            std::string(reinterpret_cast<const char*>(&kTestPrivateKey[0]),
                        sizeof(kTestPrivateKey)),
            response_key_config.value())
            .value();

    auto received_request = ohttp_gateway.DecryptObliviousHttpRequest(
        request.content, kTrustedSignalsKVv2EncryptionRequestMediaType);
    CHECK(received_request.ok()) << received_request.status();

    // Parse request and get interest group names.
    base::span<const uint8_t> body_span =
        base::as_byte_span(received_request->GetPlaintextData());
    base::SpanReader reader(body_span);
    // Skip the first 1 byte since the compression format is always set as
    // uncompressed in this test.
    reader.Skip(1u);
    uint32_t length;
    reader.ReadU32BigEndian(length);
    auto cbor_bytes = reader.Read(length);
    CHECK(cbor_bytes);
    std::optional<cbor::Value> request_body =
        cbor::Reader::Read(base::span(cbor_bytes.value()));
    CHECK(request_body);

    cbor::Value::ArrayValue compression_groups;

    if (request.relative_url == kTrustedKVv2BiddingSignalsPath) {
      std::vector<std::string> interest_group_names;
      for (const auto& partition :
           request_body->GetMap().at(cbor::Value("partitions")).GetArray()) {
        for (const auto& argument :
             partition.GetMap().at(cbor::Value("arguments")).GetArray()) {
          const cbor::Value::MapValue& argument_map = argument.GetMap();
          if (argument_map.at(cbor::Value("tags"))
                  .GetArray()
                  .at(0)
                  .GetString() == "interestGroupNames") {
            for (const auto& data :
                 argument_map.at(cbor::Value("data")).GetArray()) {
              interest_group_names.emplace_back(std::move(data.GetString()));
            }
          }
        }
      }

      // Return different response based on `interest_group_names`.
      int compression_group_id = 0;

      for (const auto& name : interest_group_names) {
        cbor::Value::MapValue compression_group;
        compression_group.try_emplace(cbor::Value("compressionGroupId"),
                                      cbor::Value(compression_group_id));
        compression_group_id++;

        if (name == "name1") {
          compression_group.try_emplace(
              cbor::Value("content"),
              cbor::Value(test::ToCborVector(kKVv2BiddingBase1)));
        } else if (name == "name2") {
          compression_group.try_emplace(
              cbor::Value("content"),
              cbor::Value(test::ToCborVector(kKVv2BiddingBase2)));
        }

        compression_groups.emplace_back(std::move(compression_group));
      }
    } else {
      std::vector<std::string> render_urls;
      for (const auto& partition :
           request_body->GetMap().at(cbor::Value("partitions")).GetArray()) {
        for (const auto& argument :
             partition.GetMap().at(cbor::Value("arguments")).GetArray()) {
          const cbor::Value::MapValue& argument_map = argument.GetMap();
          if (argument_map.at(cbor::Value("tags"))
                  .GetArray()
                  .at(0)
                  .GetString() == "renderUrls") {
            for (const auto& data :
                 argument_map.at(cbor::Value("data")).GetArray()) {
              render_urls.emplace_back(std::move(data.GetString()));
            }
          }
        }
      }

      // Return different response based on `render_urls`.
      int compression_group_id = 0;

      for (const auto& name : render_urls) {
        cbor::Value::MapValue compression_group;

        if (name == "https://bar.test/") {
          compression_group.try_emplace(
              cbor::Value("content"),
              cbor::Value(test::ToCborVector(kKVv2ScoringBase1)));
        } else if (name == "https://baz.test/") {
          compression_group.try_emplace(
              cbor::Value("content"),
              cbor::Value(test::ToCborVector(kKVv2ScoringBase2)));
        } else {
          continue;
        }

        compression_group.try_emplace(cbor::Value("compressionGroupId"),
                                      cbor::Value(compression_group_id));
        compression_group_id++;
        compression_groups.emplace_back(std::move(compression_group));
      }
    }

    cbor::Value::MapValue body_map;
    body_map.try_emplace(cbor::Value("compressionGroups"),
                         cbor::Value(std::move(compression_groups)));

    cbor::Value body_value(std::move(body_map));
    std::optional<std::vector<uint8_t>> maybe_body_bytes =
        cbor::Writer::Write(body_value);
    CHECK(maybe_body_bytes);

    std::string response_body = test::CreateKVv2ResponseBody(
        base::as_string_view(std::move(maybe_body_bytes).value()));

    auto response_context =
        std::move(received_request).value().ReleaseContext();

    // Encrypt the response body.
    auto maybe_response = ohttp_gateway.CreateObliviousHttpResponse(
        response_body, response_context,
        kTrustedSignalsKVv2EncryptionResponseMediaType);
    CHECK(maybe_response.ok()) << maybe_response.status();

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(response_status_code_);
    response->set_content_type(kAdAuctionTrustedSignalsMimeType);
    response->set_content(maybe_response->EncapsulateAndSerialize());
    response->AddCustomHeader("Ad-Auction-Allowed", "true");

    return response;
  }

  // Need to use an IO thread for the TestSharedURLLoaderFactory, which lives
  // on the thread it's created on, to make network requests.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};

  // The fetch helpers store the most recent error message, if any, here.
  std::optional<std::string> error_msg_;
  scoped_refptr<AuctionV8Helper> v8_helper_{
      AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner())};
  base::test::ScopedFeatureList feature_list_;
  TestAuctionNetworkEventsHandler auction_network_events_handler_;
  std::unique_ptr<TrustedSignalsRequestManager> bidding_request_manager_;
  std::unique_ptr<TrustedSignalsRequestManager> scoring_request_manager_;

  url::Origin owner_origin_a_{url::Origin::Create(GURL(kOwnerOriginA))};
  url::Origin owner_origin_b_{url::Origin::Create(GURL(kOwnerOriginB))};
  url::Origin joining_origin_a_{url::Origin::Create(GURL(kJoiningOriginA))};
  url::Origin joining_origin_b_{url::Origin::Create(GURL(kJoiningOriginB))};
  blink::mojom::InterestGroup::ExecutionMode execution_mode_{
      blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode};

  base::Lock lock_;
  net::HttpStatusCode response_status_code_ GUARDED_BY(lock_);

  // URLLoaderFactory that makes real network requests.
  scoped_refptr<network::TestSharedURLLoaderFactory> url_loader_factory_{
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>(
          /*network_service=*/nullptr,
          /*is_trusted=*/true)};

  net::test_server::EmbeddedTestServer embedded_test_server_{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};
};

TEST_F(TrustedSignalsRequestManagerKVv2EmbeddedTest, BiddingSignalsOneRequest) {
  const std::vector<std::string> kKeys{"key2", "key1"};
  scoped_refptr<TrustedSignals::Result> signals =
      FetchBiddingSignals("name1", kKeys, joining_origin_a_, execution_mode_);
  ASSERT_TRUE(signals);
  EXPECT_FALSE(error_msg_.has_value());
  EXPECT_EQ(R"({"key2":[2],"key1":1})",
            ExtractBiddingSignals(signals.get(), kKeys));
  const auto priority_vector =
      signals->GetPerGroupData("name1")->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"foo", 1}}),
            *priority_vector);
}

// Use `kDefaultValue` as the execution mode when adding the signals, where the
// isolation index will be partition ID (1) and compression group ID (0).
TEST_F(TrustedSignalsRequestManagerKVv2EmbeddedTest,
       BiddingSignalsIsolationIndexNotFound) {
  base::RunLoop run_loop;
  scoped_refptr<TrustedSignals::Result> signals;
  std::optional<std::string> error_msg;
  auto request1 = bidding_request_manager_->RequestKVv2BiddingSignals(
      {"name1"}, {{"key1"}}, joining_origin_a_,
      blink::mojom::InterestGroup::ExecutionMode::kDefaultValue,
      base::BindOnce(&LoadSignalsCallback, &signals, &error_msg,
                     run_loop.QuitClosure()));

  bidding_request_manager_->StartBatchedTrustedSignalsRequest();

  run_loop.Run();
  EXPECT_FALSE(signals);
  EXPECT_EQ(error_msg,
            "Failed to locate compression group \"0\" and "
            "partition \"1\" in the result map.");
}

// Build a request by adding two signals with isolation index compression group
// 0, partition 0 and compression group 0, partition 1. The response does
// contain the data for `name2`, but in compressiong group 1 and parititon 0.
TEST_F(TrustedSignalsRequestManagerKVv2EmbeddedTest,
       BiddingSignalsMissingPartition) {
  base::RunLoop run_loop1;
  scoped_refptr<TrustedSignals::Result> signals1;
  std::optional<std::string> error_msg1;
  auto request1 = bidding_request_manager_->RequestKVv2BiddingSignals(
      {"name1"}, {{"key1"}}, joining_origin_a_,
      blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode,
      base::BindOnce(&LoadSignalsCallback, &signals1, &error_msg1,
                     run_loop1.QuitClosure()));

  base::RunLoop run_loop2;
  scoped_refptr<TrustedSignals::Result> signals2;
  std::optional<std::string> error_msg2;
  auto request2 = bidding_request_manager_->RequestKVv2BiddingSignals(
      {"name2"}, {{"key2"}}, joining_origin_a_,
      blink::mojom::InterestGroup::ExecutionMode::kDefaultValue,
      base::BindOnce(&LoadSignalsCallback, &signals2, &error_msg2,
                     run_loop2.QuitClosure()));

  bidding_request_manager_->StartBatchedTrustedSignalsRequest();

  run_loop1.Run();
  ASSERT_TRUE(signals1);
  EXPECT_FALSE(error_msg1.has_value());
  EXPECT_EQ(R"({"key1":1})", ExtractBiddingSignals(signals1.get(), {"key1"}));
  const auto priority_vector =
      signals1->GetPerGroupData("name1")->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"foo", 1}}),
            *priority_vector);

  run_loop2.Run();
  EXPECT_FALSE(signals2);
  EXPECT_EQ(error_msg2,
            "Failed to locate compression group \"0\" and "
            "partition \"1\" in the result map.");
}

// Build a request by adding two signals with isolation index compression group
// 0, partition 0 and compression group 1, partition 1. The response does
// contain the data for `name2`, but in compressiong group 1 and parititon 0.
TEST_F(TrustedSignalsRequestManagerKVv2EmbeddedTest,
       BiddingSignalsMissingCompressionGroup) {
  base::RunLoop run_loop1;
  scoped_refptr<TrustedSignals::Result> signals1;
  std::optional<std::string> error_msg1;
  auto request1 = bidding_request_manager_->RequestKVv2BiddingSignals(
      {"name1"}, {{"key1"}}, joining_origin_a_,
      blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode,
      base::BindOnce(&LoadSignalsCallback, &signals1, &error_msg1,
                     run_loop1.QuitClosure()));

  base::RunLoop run_loop2;
  scoped_refptr<TrustedSignals::Result> signals2;
  std::optional<std::string> error_msg2;
  auto request2 = bidding_request_manager_->RequestKVv2BiddingSignals(
      {"name2"}, {{"key2"}}, joining_origin_b_,
      blink::mojom::InterestGroup::ExecutionMode::kDefaultValue,
      base::BindOnce(&LoadSignalsCallback, &signals2, &error_msg2,
                     run_loop2.QuitClosure()));

  bidding_request_manager_->StartBatchedTrustedSignalsRequest();

  run_loop1.Run();
  ASSERT_TRUE(signals1);
  EXPECT_FALSE(error_msg1.has_value());
  EXPECT_EQ(R"({"key1":1})", ExtractBiddingSignals(signals1.get(), {"key1"}));
  const auto priority_vector =
      signals1->GetPerGroupData("name1")->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"foo", 1}}),
            *priority_vector);

  run_loop2.Run();
  EXPECT_FALSE(signals2);
  EXPECT_EQ(error_msg2,
            "Failed to locate compression group \"1\" and "
            "partition \"1\" in the result map.");
}

TEST_F(TrustedSignalsRequestManagerKVv2EmbeddedTest,
       BiddingSignalsOneRequestNullKeys) {
  scoped_refptr<TrustedSignals::Result> signals =
      FetchBiddingSignals({"name1"},
                          /*trusted_bidding_signals_keys=*/std::nullopt,
                          joining_origin_a_, execution_mode_);
  ASSERT_TRUE(signals);
  EXPECT_FALSE(error_msg_.has_value());
  const auto priority_vector =
      signals->GetPerGroupData("name1")->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"foo", 1}}),
            *priority_vector);
}

TEST_F(TrustedSignalsRequestManagerKVv2EmbeddedTest,
       BiddingSignalsBatchedRequest) {
  // Use partially overlapping keys, to cover both the shared and distinct key
  // cases.
  const std::vector<std::string> kKeys1{"key1", "key2"};
  const std::vector<std::string> kKeys2{"key2", "key3"};

  base::RunLoop run_loop1;
  scoped_refptr<TrustedSignals::Result> signals1;
  std::optional<std::string> error_msg1;
  auto request1 = bidding_request_manager_->RequestKVv2BiddingSignals(
      {"name1"}, kKeys1, joining_origin_a_, execution_mode_,
      base::BindOnce(&LoadSignalsCallback, &signals1, &error_msg1,
                     run_loop1.QuitClosure()));

  base::RunLoop run_loop2;
  scoped_refptr<TrustedSignals::Result> signals2;
  std::optional<std::string> error_msg2;
  auto request2 = bidding_request_manager_->RequestKVv2BiddingSignals(
      {"name2"}, kKeys2, joining_origin_b_, execution_mode_,
      base::BindOnce(&LoadSignalsCallback, &signals2, &error_msg2,
                     run_loop2.QuitClosure()));

  bidding_request_manager_->StartBatchedTrustedSignalsRequest();

  run_loop1.Run();
  EXPECT_FALSE(error_msg1);
  ASSERT_TRUE(signals1);
  EXPECT_EQ(R"({"key1":1,"key2":[2]})",
            ExtractBiddingSignals(signals1.get(), kKeys1));
  auto priority_vector = signals1->GetPerGroupData("name1")->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"foo", 1}}),
            *priority_vector);

  run_loop2.Run();
  EXPECT_FALSE(error_msg2);
  ASSERT_TRUE(signals2);
  EXPECT_EQ(R"({"key2":[2],"key3":"3"})",
            ExtractBiddingSignals(signals2.get(), kKeys2));
  priority_vector = signals2->GetPerGroupData("name2")->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"foo", 2}}),
            *priority_vector);
}

TEST_F(TrustedSignalsRequestManagerKVv2EmbeddedTest,
       BiddingSignalsBatchedRequestError) {
  SetResponseStatusCode(net::HttpStatusCode::HTTP_NOT_FOUND);

  base::RunLoop run_loop1;
  scoped_refptr<TrustedSignals::Result> signals1;
  std::optional<std::string> error_msg1;
  auto request1 = bidding_request_manager_->RequestKVv2BiddingSignals(
      {"name1"}, {{"key1"}}, joining_origin_a_, execution_mode_,
      base::BindOnce(&LoadSignalsCallback, &signals1, &error_msg1,
                     run_loop1.QuitClosure()));

  base::RunLoop run_loop2;
  scoped_refptr<TrustedSignals::Result> signals2;
  std::optional<std::string> error_msg2;
  auto request2 = bidding_request_manager_->RequestKVv2BiddingSignals(
      {"name2"}, {{"key2"}}, joining_origin_b_, execution_mode_,
      base::BindOnce(&LoadSignalsCallback, &signals2, &error_msg2,
                     run_loop2.QuitClosure()));

  bidding_request_manager_->StartBatchedTrustedSignalsRequest();

  const std::string kExpectedError = base::StringPrintf(
      "Failed to load %s HTTP status = 404 "
      "Not Found.",
      TrustedBiddingSignalsUrl().spec().c_str());

  run_loop1.Run();
  EXPECT_FALSE(signals1);
  EXPECT_EQ(kExpectedError, error_msg1);

  run_loop2.Run();
  EXPECT_FALSE(signals2);
  EXPECT_EQ(kExpectedError, error_msg2);
}

TEST_F(TrustedSignalsRequestManagerKVv2EmbeddedTest,
       BiddingSignalsSequentialRequests) {
  // Use partially overlapping keys, to cover both the shared and distinct key
  // cases.
  const std::vector<std::string> kKeys1{"key1", "key2"};
  const std::vector<std::string> kKeys2{"key2", "key3"};

  // Note that these responses use different values for the shared key.
  scoped_refptr<TrustedSignals::Result> signals1 = FetchBiddingSignals(
      {"name1"}, kKeys1, joining_origin_a_, execution_mode_);
  ASSERT_TRUE(signals1);
  EXPECT_FALSE(error_msg_.has_value());
  EXPECT_EQ(R"({"key1":1,"key2":[2]})",
            ExtractBiddingSignals(signals1.get(), kKeys1));
  auto priority_vector = signals1->GetPerGroupData("name1")->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"foo", 1}}),
            *priority_vector);

  scoped_refptr<TrustedSignals::Result> signals2 = FetchBiddingSignals(
      {"name2"}, kKeys2, joining_origin_a_, execution_mode_);
  ASSERT_TRUE(signals2);
  EXPECT_FALSE(error_msg_.has_value());
  EXPECT_EQ(R"({"key2":[2],"key3":"3"})",
            ExtractBiddingSignals(signals2.get(), kKeys2));
  priority_vector = signals2->GetPerGroupData("name2")->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"foo", 2}}),
            *priority_vector);
}

// Test the case where there are multiple network requests live at once.
TEST_F(TrustedSignalsRequestManagerKVv2EmbeddedTest,
       BiddingSignalsSimultaneousNetworkRequests) {
  // Use partially overlapping keys, to cover both the shared and distinct key
  // cases.
  const std::vector<std::string> kKeys1{"key1", "key2"};
  const std::vector<std::string> kKeys2{"key2", "key3"};

  base::RunLoop run_loop1;
  scoped_refptr<TrustedSignals::Result> signals1;
  std::optional<std::string> error_msg1;
  auto request1 = bidding_request_manager_->RequestKVv2BiddingSignals(
      {"name1"}, kKeys1, joining_origin_a_, execution_mode_,
      base::BindOnce(&LoadSignalsCallback, &signals1, &error_msg1,
                     run_loop1.QuitClosure()));

  bidding_request_manager_->StartBatchedTrustedSignalsRequest();

  base::RunLoop run_loop2;
  scoped_refptr<TrustedSignals::Result> signals2;
  std::optional<std::string> error_msg2;
  auto request2 = bidding_request_manager_->RequestKVv2BiddingSignals(
      {"name2"}, kKeys2, joining_origin_a_, execution_mode_,
      base::BindOnce(&LoadSignalsCallback, &signals2, &error_msg2,
                     run_loop2.QuitClosure()));

  bidding_request_manager_->StartBatchedTrustedSignalsRequest();

  base::RunLoop().RunUntilIdle();

  run_loop1.Run();
  EXPECT_FALSE(error_msg1);
  ASSERT_TRUE(signals1);
  EXPECT_EQ(R"({"key1":1,"key2":[2]})",
            ExtractBiddingSignals(signals1.get(), kKeys1));
  auto priority_vector = signals1->GetPerGroupData("name1")->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"foo", 1}}),
            *priority_vector);

  run_loop2.Run();
  EXPECT_FALSE(error_msg2);
  ASSERT_TRUE(signals2);
  EXPECT_EQ(R"({"key2":[2],"key3":"3"})",
            ExtractBiddingSignals(signals2.get(), kKeys2));
  priority_vector = signals2->GetPerGroupData("name2")->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"foo", 2}}),
            *priority_vector);
}

// Make two requests, cancel the first one, then try to start a network request.
// A request should be made, but only for the key in the request that was not
// cancelled. Only test bidders, since sellers have no significant differences
// in this path.
TEST_F(TrustedSignalsRequestManagerKVv2EmbeddedTest, CancelOneRequest) {
  const std::vector<std::string> kKeys1{"key1"};
  const std::vector<std::string> kKeys2{"key2"};

  // The request for `key1` will be cancelled before the network request is
  // created.
  auto request1 = bidding_request_manager_->RequestKVv2BiddingSignals(
      {"name1"}, kKeys1, joining_origin_a_, execution_mode_,
      base::BindOnce(&NeverInvokedLoadSignalsCallback));

  base::RunLoop run_loop2;
  scoped_refptr<TrustedSignals::Result> signals2;
  std::optional<std::string> error_msg2;
  auto request2 = bidding_request_manager_->RequestKVv2BiddingSignals(
      {"name2"}, kKeys2, joining_origin_a_, execution_mode_,
      base::BindOnce(&LoadSignalsCallback, &signals2, &error_msg2,
                     run_loop2.QuitClosure()));

  request1.reset();
  bidding_request_manager_->StartBatchedTrustedSignalsRequest();
  base::RunLoop().RunUntilIdle();

  run_loop2.Run();
  EXPECT_FALSE(error_msg2);
  ASSERT_TRUE(signals2);
  EXPECT_EQ(R"({"key2":[2]})", ExtractBiddingSignals(signals2.get(), kKeys2));
  const auto priority_vector =
      signals2->GetPerGroupData("name2")->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"foo", 2}}),
            *priority_vector);
}

// Make two requests, try to start a network request, then cancel the first one.
// The request that was not cancelled should complete normally. Only test
// bidders, since sellers have no significant differences in this path.
TEST_F(TrustedSignalsRequestManagerKVv2EmbeddedTest, CancelOneLiveRequest) {
  const std::vector<std::string> kKeys1{"key1"};
  const std::vector<std::string> kKeys2{"key2"};

  auto request1 = bidding_request_manager_->RequestKVv2BiddingSignals(
      {"name1"}, kKeys1, joining_origin_a_, execution_mode_,
      base::BindOnce(&NeverInvokedLoadSignalsCallback));

  base::RunLoop run_loop2;
  scoped_refptr<TrustedSignals::Result> signals2;
  std::optional<std::string> error_msg2;
  auto request2 = bidding_request_manager_->RequestKVv2BiddingSignals(
      {"name2"}, kKeys2, joining_origin_b_, execution_mode_,
      base::BindOnce(&LoadSignalsCallback, &signals2, &error_msg2,
                     run_loop2.QuitClosure()));

  // Wait for network request to be made, which should include both keys in the
  // URLs.
  bidding_request_manager_->StartBatchedTrustedSignalsRequest();
  base::RunLoop().RunUntilIdle();

  request1.reset();
  // `request2` should still complete.
  run_loop2.Run();
  EXPECT_FALSE(error_msg2);
  ASSERT_TRUE(signals2);
  EXPECT_EQ(R"({"key2":[2]})", ExtractBiddingSignals(signals2.get(), kKeys2));
  const auto priority_vector =
      signals2->GetPerGroupData("name2")->priority_vector;
  ASSERT_TRUE(priority_vector);
  EXPECT_EQ((TrustedSignals::Result::PriorityVector{{"foo", 2}}),
            *priority_vector);

  // The callback of `request1` should not be invoked, since it was cancelled.
  base::RunLoop().RunUntilIdle();
}

TEST_F(TrustedSignalsRequestManagerKVv2EmbeddedTest, ScoringSignalsOneRequest) {
  const GURL kRenderUrl = GURL("https://bar.test/");
  const std::vector<std::string> kAdComponentRenderUrls{"https://barsub.test/",
                                                        "https://foosub.test/"};

  // URLs are currently added in lexical order.
  scoped_refptr<TrustedSignals::Result> signals = FetchScoringSignals(
      kRenderUrl, kAdComponentRenderUrls, owner_origin_a_, joining_origin_a_);

  ASSERT_TRUE(signals);
  EXPECT_FALSE(error_msg_.has_value());
  EXPECT_EQ(
      R"({"renderURL":{"https://bar.test/":1},)"
      R"("renderUrl":{"https://bar.test/":1},)"
      R"("adComponentRenderURLs":{"https://barsub.test/":2,)"
      R"("https://foosub.test/":[3]},)"
      R"("adComponentRenderUrls":{"https://barsub.test/":2,)"
      R"("https://foosub.test/":[3]}})",
      ExtractScoringSignals(signals.get(), kRenderUrl, kAdComponentRenderUrls));
}

// Use same owner and joining origins for both requests, where the
// isolation indexes will be partition ID (0), compression group ID (0) and
// partition ID (1), compression group ID (0).
TEST_F(TrustedSignalsRequestManagerKVv2EmbeddedTest,
       ScoringSignalsMissingPartition) {
  base::RunLoop run_loop1;
  scoped_refptr<TrustedSignals::Result> signals1;
  std::optional<std::string> error_msg1;
  auto request1 = scoring_request_manager_->RequestKVv2ScoringSignals(
      GURL("https://bar.test/"),
      {"https://barsub.test/", "https://foosub.test/"}, owner_origin_a_,
      joining_origin_a_,
      base::BindOnce(&LoadSignalsCallback, &signals1, &error_msg1,
                     run_loop1.QuitClosure()));

  base::RunLoop run_loop2;
  scoped_refptr<TrustedSignals::Result> signals2;
  std::optional<std::string> error_msg2;
  auto request2 = scoring_request_manager_->RequestKVv2ScoringSignals(
      GURL("https://baz.test/"),
      {"https://bazsub.test/", "https://foosub.test/"}, owner_origin_a_,
      joining_origin_a_,
      base::BindOnce(&LoadSignalsCallback, &signals2, &error_msg2,
                     run_loop2.QuitClosure()));

  scoring_request_manager_->StartBatchedTrustedSignalsRequest();

  run_loop1.Run();
  ASSERT_TRUE(signals1);
  EXPECT_FALSE(error_msg1.has_value());

  run_loop2.Run();
  ASSERT_FALSE(signals2);
  EXPECT_TRUE(error_msg2.has_value());
  EXPECT_EQ(error_msg2,
            "Failed to locate compression group \"0\" and "
            "partition \"1\" in the result map.");
}

// Use different owner and joining origins for all three requests, where the
// isolation indexes will be partition ID (0), compression group ID (0),
// partition ID (0), compression group ID (1) and partition ID (0), compression
// group ID (2).
TEST_F(TrustedSignalsRequestManagerKVv2EmbeddedTest,
       ScoringSignalsMissingCompressionGroup) {
  base::RunLoop run_loop1;
  scoped_refptr<TrustedSignals::Result> signals1;
  std::optional<std::string> error_msg1;
  auto request1 = scoring_request_manager_->RequestKVv2ScoringSignals(
      GURL("https://bar.test/"),
      {"https://barsub.test/", "https://foosub.test/"}, owner_origin_a_,
      joining_origin_a_,
      base::BindOnce(&LoadSignalsCallback, &signals1, &error_msg1,
                     run_loop1.QuitClosure()));

  base::RunLoop run_loop2;
  scoped_refptr<TrustedSignals::Result> signals2;
  std::optional<std::string> error_msg2;
  auto request2 = scoring_request_manager_->RequestKVv2ScoringSignals(
      GURL("https://baz.test/"),
      {"https://bazsub.test/", "https://foosub.test/"}, owner_origin_b_,
      joining_origin_b_,
      base::BindOnce(&LoadSignalsCallback, &signals2, &error_msg2,
                     run_loop2.QuitClosure()));

  base::RunLoop run_loop3;
  scoped_refptr<TrustedSignals::Result> signals3;
  std::optional<std::string> error_msg3;
  auto request3 = scoring_request_manager_->RequestKVv2ScoringSignals(
      GURL("https://qux.test/"),
      {"https://quxsub.test/", "https://foosub.test/"},
      url::Origin::Create(GURL("https://owner-c.test")),
      url::Origin::Create(GURL("https://origin-c.test")),
      base::BindOnce(&LoadSignalsCallback, &signals3, &error_msg3,
                     run_loop3.QuitClosure()));

  scoring_request_manager_->StartBatchedTrustedSignalsRequest();

  run_loop1.Run();
  ASSERT_TRUE(signals1);
  EXPECT_FALSE(error_msg1.has_value());

  run_loop2.Run();
  ASSERT_TRUE(signals2);
  EXPECT_FALSE(error_msg2.has_value());

  run_loop3.Run();
  ASSERT_FALSE(signals3);
  EXPECT_TRUE(error_msg3.has_value());
  EXPECT_EQ(error_msg3,
            "Failed to locate compression group \"2\" and "
            "partition \"0\" in the result map.");
}

TEST_F(TrustedSignalsRequestManagerKVv2EmbeddedTest,
       ScoringSignalsBatchedRequest) {
  const GURL kRenderUrl1 = GURL("https://bar.test/");
  const GURL kRenderUrl2 = GURL("https://baz.test/");
  const std::vector<std::string> kAdComponentRenderUrls1{
      "https://barsub.test/", "https://foosub.test/"};
  const std::vector<std::string> kAdComponentRenderUrls2{
      "https://bazsub.test/", "https://foosub.test/"};

  base::RunLoop run_loop1;
  scoped_refptr<TrustedSignals::Result> signals1;
  std::optional<std::string> error_msg1;
  auto request1 = scoring_request_manager_->RequestKVv2ScoringSignals(
      kRenderUrl1, kAdComponentRenderUrls1, owner_origin_a_, joining_origin_a_,
      base::BindOnce(&LoadSignalsCallback, &signals1, &error_msg1,
                     run_loop1.QuitClosure()));

  base::RunLoop run_loop2;
  scoped_refptr<TrustedSignals::Result> signals2;
  std::optional<std::string> error_msg2;
  auto request2 = scoring_request_manager_->RequestKVv2ScoringSignals(
      kRenderUrl2, kAdComponentRenderUrls2, owner_origin_b_, joining_origin_b_,
      base::BindOnce(&LoadSignalsCallback, &signals2, &error_msg2,
                     run_loop2.QuitClosure()));

  scoring_request_manager_->StartBatchedTrustedSignalsRequest();

  run_loop1.Run();
  ASSERT_TRUE(signals1);
  EXPECT_FALSE(error_msg1.has_value());
  EXPECT_EQ(R"({"renderURL":{"https://bar.test/":1},)"
            R"("renderUrl":{"https://bar.test/":1},)"
            R"("adComponentRenderURLs":{"https://barsub.test/":2,)"
            R"("https://foosub.test/":[3]},)"
            R"("adComponentRenderUrls":{"https://barsub.test/":2,)"
            R"("https://foosub.test/":[3]}})",
            ExtractScoringSignals(signals1.get(), kRenderUrl1,
                                  kAdComponentRenderUrls1));

  run_loop2.Run();
  ASSERT_TRUE(signals2);
  EXPECT_FALSE(error_msg2.has_value());
  EXPECT_EQ(R"({"renderURL":{"https://baz.test/":3},)"
            R"("renderUrl":{"https://baz.test/":3},)"
            R"("adComponentRenderURLs":{"https://bazsub.test/":"4",)"
            R"("https://foosub.test/":[3]},)"
            R"("adComponentRenderUrls":{"https://bazsub.test/":"4",)"
            R"("https://foosub.test/":[3]}})",
            ExtractScoringSignals(signals2.get(), kRenderUrl2,
                                  kAdComponentRenderUrls2));
}

TEST_F(TrustedSignalsRequestManagerKVv2EmbeddedTest,
       ScoringSignalsBatchedRequestError) {
  SetResponseStatusCode(net::HttpStatusCode::HTTP_NOT_FOUND);

  base::RunLoop run_loop1;
  scoped_refptr<TrustedSignals::Result> signals1;
  std::optional<std::string> error_msg1;
  auto request1 = scoring_request_manager_->RequestKVv2ScoringSignals(
      GURL("https://bar.test/"),
      {"https://barsub.test/", "https://foosub.test/"}, owner_origin_a_,
      joining_origin_a_,
      base::BindOnce(&LoadSignalsCallback, &signals1, &error_msg1,
                     run_loop1.QuitClosure()));

  base::RunLoop run_loop2;
  scoped_refptr<TrustedSignals::Result> signals2;
  std::optional<std::string> error_msg2;
  auto request2 = scoring_request_manager_->RequestKVv2ScoringSignals(
      GURL("https://baz.test/"),
      {"https://bazsub.test/", "https://foosub.test/"}, owner_origin_a_,
      joining_origin_a_,
      base::BindOnce(&LoadSignalsCallback, &signals2, &error_msg2,
                     run_loop2.QuitClosure()));

  scoring_request_manager_->StartBatchedTrustedSignalsRequest();

  const std::string kExpectedError = base::StringPrintf(
      "Failed to load %s HTTP status = 404 "
      "Not Found.",
      TrustedScoringSignalsUrl().spec().c_str());

  run_loop1.Run();
  EXPECT_FALSE(signals1);
  EXPECT_EQ(kExpectedError, error_msg1);

  run_loop2.Run();
  EXPECT_FALSE(signals2);
  EXPECT_EQ(kExpectedError, error_msg2);
}

TEST_F(TrustedSignalsRequestManagerKVv2EmbeddedTest,
       ScoringSignalsSequentialRequests) {
  const GURL kRenderUrl1 = GURL("https://bar.test/");
  const GURL kRenderUrl2 = GURL("https://baz.test/");
  const std::vector<std::string> kAdComponentRenderUrls1{
      "https://barsub.test/", "https://foosub.test/"};
  const std::vector<std::string> kAdComponentRenderUrls2{
      "https://bazsub.test/", "https://foosub.test/"};

  scoped_refptr<TrustedSignals::Result> signals1 = FetchScoringSignals(
      kRenderUrl1, kAdComponentRenderUrls1, owner_origin_a_, joining_origin_a_);
  ASSERT_TRUE(signals1);
  EXPECT_FALSE(error_msg_.has_value());
  EXPECT_EQ(R"({"renderURL":{"https://bar.test/":1},)"
            R"("renderUrl":{"https://bar.test/":1},)"
            R"("adComponentRenderURLs":{"https://barsub.test/":2,)"
            R"("https://foosub.test/":[3]},)"
            R"("adComponentRenderUrls":{"https://barsub.test/":2,)"
            R"("https://foosub.test/":[3]}})",
            ExtractScoringSignals(signals1.get(), kRenderUrl1,
                                  kAdComponentRenderUrls1));

  scoped_refptr<TrustedSignals::Result> signals2 = FetchScoringSignals(
      kRenderUrl2, kAdComponentRenderUrls2, owner_origin_b_, joining_origin_b_);
  ASSERT_TRUE(signals2);
  EXPECT_FALSE(error_msg_.has_value());
  EXPECT_EQ(R"({"renderURL":{"https://baz.test/":3},)"
            R"("renderUrl":{"https://baz.test/":3},)"
            R"("adComponentRenderURLs":{"https://bazsub.test/":"4",)"
            R"("https://foosub.test/":[3]},)"
            R"("adComponentRenderUrls":{"https://bazsub.test/":"4",)"
            R"("https://foosub.test/":[3]}})",
            ExtractScoringSignals(signals2.get(), kRenderUrl2,
                                  kAdComponentRenderUrls2));
}

// Test the case where there are multiple network requests live at once.
TEST_F(TrustedSignalsRequestManagerKVv2EmbeddedTest,
       ScoringignalsSimultaneousNetworkRequests) {
  const GURL kRenderUrl1 = GURL("https://bar.test/");
  const GURL kRenderUrl2 = GURL("https://baz.test/");
  const std::vector<std::string> kAdComponentRenderUrls1{
      "https://barsub.test/", "https://foosub.test/"};
  const std::vector<std::string> kAdComponentRenderUrls2{
      "https://bazsub.test/", "https://foosub.test/"};

  base::RunLoop run_loop1;
  scoped_refptr<TrustedSignals::Result> signals1;
  std::optional<std::string> error_msg1;
  auto request1 = scoring_request_manager_->RequestKVv2ScoringSignals(
      GURL("https://bar.test/"),
      {"https://barsub.test/", "https://foosub.test/"}, owner_origin_a_,
      joining_origin_a_,
      base::BindOnce(&LoadSignalsCallback, &signals1, &error_msg1,
                     run_loop1.QuitClosure()));

  scoring_request_manager_->StartBatchedTrustedSignalsRequest();

  base::RunLoop run_loop2;
  scoped_refptr<TrustedSignals::Result> signals2;
  std::optional<std::string> error_msg2;
  auto request2 = scoring_request_manager_->RequestKVv2ScoringSignals(
      GURL("https://baz.test/"),
      {"https://bazsub.test/", "https://foosub.test/"}, owner_origin_a_,
      joining_origin_a_,
      base::BindOnce(&LoadSignalsCallback, &signals2, &error_msg2,
                     run_loop2.QuitClosure()));

  scoring_request_manager_->StartBatchedTrustedSignalsRequest();

  base::RunLoop().RunUntilIdle();

  run_loop1.Run();
  EXPECT_FALSE(error_msg1);
  ASSERT_TRUE(signals1);
  EXPECT_EQ(R"({"renderURL":{"https://bar.test/":1},)"
            R"("renderUrl":{"https://bar.test/":1},)"
            R"("adComponentRenderURLs":{"https://barsub.test/":2,)"
            R"("https://foosub.test/":[3]},)"
            R"("adComponentRenderUrls":{"https://barsub.test/":2,)"
            R"("https://foosub.test/":[3]}})",
            ExtractScoringSignals(signals1.get(), kRenderUrl1,
                                  kAdComponentRenderUrls1));

  run_loop2.Run();
  EXPECT_FALSE(error_msg2);
  ASSERT_TRUE(signals2);
  EXPECT_EQ(R"({"renderURL":{"https://baz.test/":3},)"
            R"("renderUrl":{"https://baz.test/":3},)"
            R"("adComponentRenderURLs":{"https://bazsub.test/":"4",)"
            R"("https://foosub.test/":[3]},)"
            R"("adComponentRenderUrls":{"https://bazsub.test/":"4",)"
            R"("https://foosub.test/":[3]}})",
            ExtractScoringSignals(signals2.get(), kRenderUrl2,
                                  kAdComponentRenderUrls2));
}

}  // namespace
}  // namespace auction_worklet
