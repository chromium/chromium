// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/trusted_bidding_signals_kvv1_requester.h"

#include <string>

#include "base/test/task_environment.h"
#include "content/services/auction_worklet/public/mojom/in_progress_auction_download.mojom.h"
#include "content/services/auction_worklet/worklet_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using Request = TrustedBiddingSignalsKVV1Requester::Request;

constexpr char kTopLevelHost[] = "publisher";
constexpr char kTrustedBiddingSignalsSlotSizeParam[] =
    "trusted_bidding_signals_slot_size_param=foo";
constexpr uint16_t kExperimentId = 123;
constexpr std::string kInterestGroup = "ig1";
const std::vector<std::string> kKeys{"key1"};
constexpr std::string kInterestGroup2 = "ig2";
const std::vector<std::string> kKeys2{"key1", "key2"};
constexpr size_t kMaxSignalsLength = 1000;

struct RequestParams {
  std::string interest_group_name;
  std::vector<std::string> keys;
};

class TrustedBiddingSignalsKVV1RequesterTest : public testing::Test {
 public:
  TrustedBiddingSignalsKVV1RequesterTest() = default;

  ~TrustedBiddingSignalsKVV1RequesterTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;

  network::TestURLLoaderFactory test_url_loader_factory_;

  auction_worklet::TestAuctionNetworkEventsHandler
      auction_network_events_handler_;

  const GURL trusted_signals_url_ = GURL("https://url.test/");
};

TEST_F(TrustedBiddingSignalsKVV1RequesterTest,
       StartBatchedRequestWithNoRequests) {
  TrustedBiddingSignalsKVV1Requester requester;
  std::vector<TrustedBiddingSignalsKVV1Requester::BatchedRequest>
      batched_requests = requester.StartBatchedRequest(
          test_url_loader_factory_, auction_network_events_handler_,
          kTopLevelHost, trusted_signals_url_, kExperimentId,
          kTrustedBiddingSignalsSlotSizeParam);
  EXPECT_TRUE(batched_requests.empty());
}

// Minimal information is populated.
TEST_F(TrustedBiddingSignalsKVV1RequesterTest, StartBatchedRequestBasic) {
  TrustedBiddingSignalsKVV1Requester requester;
  std::unique_ptr<Request> request1 =
      requester.RequestBiddingSignals(kInterestGroup, kKeys, kMaxSignalsLength);
  std::vector<TrustedBiddingSignalsKVV1Requester::BatchedRequest>
      batched_requests = requester.StartBatchedRequest(
          test_url_loader_factory_, auction_network_events_handler_,
          kTopLevelHost, trusted_signals_url_,
          /*experiment_group_id=*/std::nullopt,
          /*trusted_bidding_signals_slot_size_param=*/"");
  ASSERT_EQ(batched_requests.size(), 1u);
  EXPECT_THAT(batched_requests[0].request_ids,
              testing::ElementsAre(request1->request_id()));
  EXPECT_EQ(batched_requests[0].download->url,
            "https://url.test/"
            "?hostname=publisher&keys=key1&interestGroupNames=ig1");
}

TEST_F(TrustedBiddingSignalsKVV1RequesterTest,
       StartBatchedRequestAllFieldsPopulated) {
  TrustedBiddingSignalsKVV1Requester requester;
  std::unique_ptr<Request> request1 =
      requester.RequestBiddingSignals(kInterestGroup, kKeys, kMaxSignalsLength);
  std::vector<TrustedBiddingSignalsKVV1Requester::BatchedRequest>
      batched_requests = requester.StartBatchedRequest(
          test_url_loader_factory_, auction_network_events_handler_,
          kTopLevelHost, trusted_signals_url_, kExperimentId,
          kTrustedBiddingSignalsSlotSizeParam);
  ASSERT_EQ(batched_requests.size(), 1u);
  EXPECT_THAT(batched_requests[0].request_ids,
              testing::ElementsAre(request1->request_id()));
  EXPECT_EQ(batched_requests[0].download->url,
            "https://url.test/"
            "?hostname=publisher&keys=key1&interestGroupNames=ig1&"
            "experimentGroupId=123&trusted_"
            "bidding_signals_slot_size_param=foo");
}

TEST_F(TrustedBiddingSignalsKVV1RequesterTest, StartBatchedRequestTwoRequests) {
  TrustedBiddingSignalsKVV1Requester requester;
  std::unique_ptr<Request> request1 =
      requester.RequestBiddingSignals(kInterestGroup, kKeys, kMaxSignalsLength);
  std::unique_ptr<Request> request2 = requester.RequestBiddingSignals(
      kInterestGroup2, kKeys2, kMaxSignalsLength);
  // We need to get a new request_id each time we get a request.
  EXPECT_NE(request1->request_id(), request2->request_id());
  std::vector<TrustedBiddingSignalsKVV1Requester::BatchedRequest>
      batched_requests = requester.StartBatchedRequest(
          test_url_loader_factory_, auction_network_events_handler_,
          kTopLevelHost, trusted_signals_url_,
          /*experiment_group_id=*/std::nullopt,
          kTrustedBiddingSignalsSlotSizeParam);
  ASSERT_EQ(batched_requests.size(), 1u);
  EXPECT_THAT(batched_requests[0].request_ids,
              testing::UnorderedElementsAre(request1->request_id(),
                                            request2->request_id()));
  EXPECT_EQ(
      batched_requests[0].download->url,
      "https://url.test/"
      "?hostname=publisher&keys=key1,key2&interestGroupNames=ig1,ig2&trusted_"
      "bidding_signals_slot_size_param=foo");
}

TEST_F(TrustedBiddingSignalsKVV1RequesterTest, StartBatchedRequestTwice) {
  TrustedBiddingSignalsKVV1Requester requester;
  std::unique_ptr<Request> request1 =
      requester.RequestBiddingSignals(kInterestGroup, kKeys, kMaxSignalsLength);
  std::vector<TrustedBiddingSignalsKVV1Requester::BatchedRequest>
      batched_requests = requester.StartBatchedRequest(
          test_url_loader_factory_, auction_network_events_handler_,
          kTopLevelHost, trusted_signals_url_,
          /*experiment_group_id=*/std::nullopt,
          kTrustedBiddingSignalsSlotSizeParam);
  ASSERT_EQ(batched_requests.size(), 1u);
  EXPECT_THAT(batched_requests[0].request_ids,
              testing::UnorderedElementsAre(request1->request_id()));
  EXPECT_EQ(batched_requests[0].download->url,
            "https://url.test/"
            "?hostname=publisher&keys=key1&interestGroupNames=ig1&trusted_"
            "bidding_signals_slot_size_param=foo");

  std::unique_ptr<Request> request2 = requester.RequestBiddingSignals(
      kInterestGroup2, kKeys2, kMaxSignalsLength);
  // We need to get a new request_id each time we get a request.
  EXPECT_NE(request1->request_id(), request2->request_id());
  batched_requests = requester.StartBatchedRequest(
      test_url_loader_factory_, auction_network_events_handler_, kTopLevelHost,
      trusted_signals_url_,
      /*experiment_group_id=*/kExperimentId,
      kTrustedBiddingSignalsSlotSizeParam);
  ASSERT_EQ(batched_requests.size(), 1u);
  EXPECT_THAT(batched_requests[0].request_ids,
              testing::UnorderedElementsAre(request2->request_id()));
  EXPECT_EQ(batched_requests[0].download->url,
            "https://url.test/"
            "?hostname=publisher&keys=key1,key2&interestGroupNames=ig2&"
            "experimentGroupId=123&trusted_"
            "bidding_signals_slot_size_param=foo");
}

TEST_F(TrustedBiddingSignalsKVV1RequesterTest,
       StartBatchedRequestIdenticalRequests) {
  TrustedBiddingSignalsKVV1Requester requester;
  std::unique_ptr<Request> request1 =
      requester.RequestBiddingSignals(kInterestGroup, kKeys, kMaxSignalsLength);
  std::unique_ptr<Request> request2 =
      requester.RequestBiddingSignals(kInterestGroup, kKeys, kMaxSignalsLength);
  // We need to get a new request_id each time we get a request.
  EXPECT_NE(request1->request_id(), request2->request_id());
  std::vector<TrustedBiddingSignalsKVV1Requester::BatchedRequest>
      batched_requests = requester.StartBatchedRequest(
          test_url_loader_factory_, auction_network_events_handler_,
          kTopLevelHost, trusted_signals_url_,
          /*experiment_group_id=*/std::nullopt,
          kTrustedBiddingSignalsSlotSizeParam);
  ASSERT_EQ(batched_requests.size(), 1u);
  EXPECT_THAT(batched_requests[0].request_ids,
              testing::UnorderedElementsAre(request1->request_id(),
                                            request2->request_id()));
  EXPECT_EQ(batched_requests[0].download->url,
            "https://url.test/"
            "?hostname=publisher&keys=key1&interestGroupNames=ig1&trusted_"
            "bidding_signals_slot_size_param=foo");
}

TEST_F(TrustedBiddingSignalsKVV1RequesterTest,
       StartBatchedRequestWithSplitRequest) {
  TrustedBiddingSignalsKVV1Requester requester;
  const std::string kUrl1 =
      "https://url.test/?hostname=publisher"
      "&keys=key1&interestGroupNames=ig1"
      "&trusted_bidding_signals_slot_size_param=foo";
  const std::string kUrl2 =
      "https://url.test/?hostname=publisher"
      "&keys=key1,key2&interestGroupNames=ig2"
      "&trusted_bidding_signals_slot_size_param=foo";

  const std::string kUrlShared =
      "https://url.test/?hostname=publisher"
      "&keys=key1,key2&interestGroupNames=ig1,ig2"
      "&trusted_bidding_signals_slot_size_param=foo";

  for (bool require_split : {true, false}) {
    SCOPED_TRACE(require_split);
    std::unique_ptr<Request> request1 = requester.RequestBiddingSignals(
        kInterestGroup, kKeys, /*max_trusted_bidding_signals_url_length=*/200);
    size_t max_signals_length = kUrlShared.size() - require_split;
    std::unique_ptr<Request> request2 = requester.RequestBiddingSignals(
        kInterestGroup2, kKeys2, max_signals_length);

    std::vector<TrustedBiddingSignalsKVV1Requester::BatchedRequest>
        batched_requests = requester.StartBatchedRequest(
            test_url_loader_factory_, auction_network_events_handler_,
            kTopLevelHost, trusted_signals_url_,
            /*experiment_group_id=*/std::nullopt,
            kTrustedBiddingSignalsSlotSizeParam);
    if (require_split) {
      ASSERT_EQ(batched_requests.size(), 2u);
      EXPECT_THAT(batched_requests[0].request_ids,
                  testing::ElementsAre(request1->request_id()));
      EXPECT_EQ(batched_requests[0].download->url, kUrl1);
      EXPECT_THAT(batched_requests[1].request_ids,
                  testing::ElementsAre(request2->request_id()));
      EXPECT_EQ(batched_requests[1].download->url, kUrl2);
    } else {
      ASSERT_EQ(batched_requests.size(), 1u);
      EXPECT_THAT(
          batched_requests[0].request_ids,
          testing::ElementsAre(request1->request_id(), request2->request_id()));
      EXPECT_EQ(batched_requests[0].download->url, kUrlShared);
    }
  }
}

TEST_F(TrustedBiddingSignalsKVV1RequesterTest,
       StartBatchedRequestWithSplitRequestAfterVariousNumberOfRequests) {
  TrustedBiddingSignalsKVV1Requester requester;
  const std::vector<RequestParams> kRequestParams = {{"ig1", {"key1", "key2"}},
                                                     {"ig2", {}},
                                                     {"ig3", {"key1", "key3"}},
                                                     {"ig4", {"key1", "key4"}}};

  struct SplitTestCase {
    size_t max_signals_length;
    std::vector<std::string> expected_urls;
    std::vector<std::vector<size_t>> expected_index_split;
  };

  const SplitTestCase kTests[] = {
      // Each requests gets batched individually.
      {119,
       {"https://url.test/?hostname=publisher"
        "&keys=key1,key2&interestGroupNames=ig1"
        "&trusted_bidding_signals_slot_size_param=foo",
        "https://url.test/?hostname=publisher"
        "&interestGroupNames=ig2"
        "&trusted_bidding_signals_slot_size_param=foo",
        "https://url.test/?hostname=publisher"
        "&keys=key1,key3&interestGroupNames=ig3"
        "&trusted_bidding_signals_slot_size_param=foo",
        "https://url.test/?hostname=publisher"
        "&keys=key1,key4&interestGroupNames=ig4"
        "&trusted_bidding_signals_slot_size_param=foo"},
       {{0}, {1}, {2}, {3}}},
      // Requests get batched in pairs.
      {129,
       {"https://url.test/?hostname=publisher"
        "&keys=key1,key2&interestGroupNames=ig1,ig2"
        "&trusted_bidding_signals_slot_size_param=foo",
        "https://url.test/?hostname=publisher"
        "&keys=key1,key3,key4&interestGroupNames=ig3,ig4"
        "&trusted_bidding_signals_slot_size_param=foo"},
       {{0, 1}, {2, 3}}},
      // Requests get split into a batch of 3 and a batch of 1.
      {133,
       {"https://url.test/?hostname=publisher"
        "&keys=key1,key2,key3&interestGroupNames=ig1,ig2,ig3"
        "&trusted_bidding_signals_slot_size_param=foo",
        "https://url.test/?hostname=publisher"
        "&keys=key1,key4&interestGroupNames=ig4"
        "&trusted_bidding_signals_slot_size_param=foo"},
       {{0, 1, 2}, {3}}}};

  std::set<size_t> seen_request_ids;

  for (const SplitTestCase& test : kTests) {
    SCOPED_TRACE(test.max_signals_length);
    std::vector<std::unique_ptr<Request>> requests;
    for (const RequestParams& request_params : kRequestParams) {
      requests.push_back(requester.RequestBiddingSignals(
          request_params.interest_group_name, request_params.keys,
          test.max_signals_length));
      EXPECT_THAT(
          seen_request_ids,
          testing::Not(testing::Contains(requests.back()->request_id())));
      seen_request_ids.insert(requests.back()->request_id());
    }
    std::vector<TrustedBiddingSignalsKVV1Requester::BatchedRequest>
        batched_requests = requester.StartBatchedRequest(
            test_url_loader_factory_, auction_network_events_handler_,
            kTopLevelHost, trusted_signals_url_,
            /*experiment_group_id=*/std::nullopt,
            kTrustedBiddingSignalsSlotSizeParam);

    ASSERT_EQ(batched_requests.size(), test.expected_index_split.size());
    for (size_t batch_index = 0; batch_index < batched_requests.size();
         ++batch_index) {
      std::vector<size_t> expected_request_ids;
      for (size_t split_index : test.expected_index_split[batch_index]) {
        expected_request_ids.push_back(requests[split_index]->request_id());
      }
      EXPECT_THAT(batched_requests[batch_index].request_ids,
                  testing::UnorderedElementsAreArray(expected_request_ids));
      EXPECT_EQ(batched_requests[batch_index].download->url,
                test.expected_urls[batch_index]);
    }
  }
}

TEST_F(TrustedBiddingSignalsKVV1RequesterTest,
       ZeroMaxSignalsLengthMeansInfiniteLength) {
  TrustedBiddingSignalsKVV1Requester requester;
  const std::vector<RequestParams> kRequestParams = {{"ig1", {"key1", "key2"}},
                                                     {"ig2", {}},
                                                     {"ig3", {"key1", "key3"}},
                                                     {"ig4", {"key1", "key4"}}};
  const std::string kExpectedUrl =
      "https://url.test/?hostname=publisher"
      "&keys=key1,key2,key3,key4&interestGroupNames=ig1,ig2,ig3,ig4"
      "&trusted_bidding_signals_slot_size_param=foo";

  std::set<size_t> seen_request_ids;

  std::vector<std::unique_ptr<Request>> requests;
  for (const RequestParams& request_params : kRequestParams) {
    requests.push_back(requester.RequestBiddingSignals(
        request_params.interest_group_name, request_params.keys,
        /*max_trusted_bidding_signals_url_length=*/0));
    EXPECT_THAT(seen_request_ids,
                testing::Not(testing::Contains(requests.back()->request_id())));
    seen_request_ids.insert(requests.back()->request_id());
  }

  std::vector<TrustedBiddingSignalsKVV1Requester::BatchedRequest>
      batched_requests = requester.StartBatchedRequest(
          test_url_loader_factory_, auction_network_events_handler_,
          kTopLevelHost, trusted_signals_url_,
          /*experiment_group_id=*/std::nullopt,
          kTrustedBiddingSignalsSlotSizeParam);

  ASSERT_EQ(batched_requests.size(), 1u);
  EXPECT_THAT(batched_requests[0].request_ids,
              testing::UnorderedElementsAre(
                  requests[0]->request_id(), requests[1]->request_id(),
                  requests[2]->request_id(), requests[3]->request_id()));
  EXPECT_EQ(batched_requests[0].download->url, kExpectedUrl);
}

TEST_F(TrustedBiddingSignalsKVV1RequesterTest,
       DestroyingTheRequestRemovesTheRequest) {
  TrustedBiddingSignalsKVV1Requester requester;
  const std::vector<RequestParams> kRequestParams = {{"ig1", {"key1", "key2"}},
                                                     {"ig2", {}},
                                                     {"ig3", {"key1", "key3"}},
                                                     {"ig4", {"key1", "key4"}}};
  std::vector<std::string> urls_with_ig_i_missing{
      "https://url.test/?hostname=publisher"
      "&keys=key1,key3,key4&interestGroupNames=ig2,ig3,ig4"
      "&trusted_bidding_signals_slot_size_param=foo",
      "https://url.test/?hostname=publisher"
      "&keys=key1,key2,key3,key4&interestGroupNames=ig1,ig3,ig4"
      "&trusted_bidding_signals_slot_size_param=foo",
      "https://url.test/?hostname=publisher"
      "&keys=key1,key2,key4&interestGroupNames=ig1,ig2,ig4"
      "&trusted_bidding_signals_slot_size_param=foo",
      "https://url.test/?hostname=publisher"
      "&keys=key1,key2,key3&interestGroupNames=ig1,ig2,ig3"
      "&trusted_bidding_signals_slot_size_param=foo"};

  for (size_t ig_to_destroy = 0; ig_to_destroy < kRequestParams.size();
       ++ig_to_destroy) {
    SCOPED_TRACE(ig_to_destroy);
    std::vector<std::unique_ptr<Request>> requests;
    for (const RequestParams& request_params : kRequestParams) {
      requests.push_back(requester.RequestBiddingSignals(
          request_params.interest_group_name, request_params.keys,
          kMaxSignalsLength));
    }
    requests.erase(requests.begin() + ig_to_destroy);
    ASSERT_EQ(requests.size(), 3u);

    std::vector<TrustedBiddingSignalsKVV1Requester::BatchedRequest>
        batched_requests = requester.StartBatchedRequest(
            test_url_loader_factory_, auction_network_events_handler_,
            kTopLevelHost, trusted_signals_url_,
            /*experiment_group_id=*/std::nullopt,
            kTrustedBiddingSignalsSlotSizeParam);

    ASSERT_EQ(batched_requests.size(), 1u);
    EXPECT_THAT(batched_requests[0].request_ids,
                testing::UnorderedElementsAre(requests[0]->request_id(),
                                              requests[1]->request_id(),
                                              requests[2]->request_id()));
    EXPECT_EQ(batched_requests[0].download->url,
              urls_with_ig_i_missing[ig_to_destroy]);
  }
}

TEST_F(TrustedBiddingSignalsKVV1RequesterTest,
       DestroyingAllRequestsRemovesAllRequests) {
  TrustedBiddingSignalsKVV1Requester requester;
  const std::vector<RequestParams> kRequestParams = {{"ig1", {"key1", "key2"}},
                                                     {"ig2", {}},
                                                     {"ig3", {"key1", "key3"}},
                                                     {"ig4", {"key1", "key4"}}};

  std::vector<std::unique_ptr<Request>> requests;
  for (const RequestParams& request_params : kRequestParams) {
    requests.push_back(requester.RequestBiddingSignals(
        request_params.interest_group_name, request_params.keys,
        kMaxSignalsLength));
  }
  requests.clear();

  std::vector<TrustedBiddingSignalsKVV1Requester::BatchedRequest>
      batched_requests = requester.StartBatchedRequest(
          test_url_loader_factory_, auction_network_events_handler_,
          kTopLevelHost, trusted_signals_url_,
          /*experiment_group_id=*/std::nullopt,
          kTrustedBiddingSignalsSlotSizeParam);

  ASSERT_EQ(batched_requests.size(), 0u);
}

TEST_F(TrustedBiddingSignalsKVV1RequesterTest,
       DestroyingAnIdenticalRequestDoesNotDestroyOtherRequest) {
  TrustedBiddingSignalsKVV1Requester requester;
  std::unique_ptr<Request> request1 =
      requester.RequestBiddingSignals(kInterestGroup, kKeys, kMaxSignalsLength);
  std::unique_ptr<Request> request2 =
      requester.RequestBiddingSignals(kInterestGroup, kKeys, kMaxSignalsLength);
  // We need to get a new request_id each time we get a request.
  EXPECT_NE(request1->request_id(), request2->request_id());
  request2.reset();
  std::vector<TrustedBiddingSignalsKVV1Requester::BatchedRequest>
      batched_requests = requester.StartBatchedRequest(
          test_url_loader_factory_, auction_network_events_handler_,
          kTopLevelHost, trusted_signals_url_,
          /*experiment_group_id=*/std::nullopt,
          kTrustedBiddingSignalsSlotSizeParam);
  ASSERT_EQ(batched_requests.size(), 1u);
  EXPECT_THAT(batched_requests[0].request_ids,
              testing::UnorderedElementsAre(request1->request_id()));
  EXPECT_EQ(batched_requests[0].download->url,
            "https://url.test/"
            "?hostname=publisher&keys=key1&interestGroupNames=ig1&trusted_"
            "bidding_signals_slot_size_param=foo");
}

// Make sure we don't crash when we destroy a Request after StartBatchedRequest.
TEST_F(TrustedBiddingSignalsKVV1RequesterTest,
       DestroyRequestAfterStartBatchedRequest) {
  TrustedBiddingSignalsKVV1Requester requester;
  std::unique_ptr<Request> request =
      requester.RequestBiddingSignals(kInterestGroup, kKeys, kMaxSignalsLength);
  std::vector<TrustedBiddingSignalsKVV1Requester::BatchedRequest>
      batched_requests = requester.StartBatchedRequest(
          test_url_loader_factory_, auction_network_events_handler_,
          kTopLevelHost, trusted_signals_url_, kExperimentId,
          kTrustedBiddingSignalsSlotSizeParam);
  size_t request_id = request->request_id();
  request.reset();
  ASSERT_EQ(batched_requests.size(), 1u);
  EXPECT_THAT(batched_requests[0].request_ids,
              testing::ElementsAre(request_id));
  EXPECT_EQ(batched_requests[0].download->url,
            "https://url.test/"
            "?hostname=publisher&keys=key1&interestGroupNames=ig1&"
            "experimentGroupId=123&trusted_"
            "bidding_signals_slot_size_param=foo");
}

}  // namespace content
