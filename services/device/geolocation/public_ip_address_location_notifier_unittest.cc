// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/public_ip_address_location_notifier.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/device/device_service_test_base.h"
#include "services/device/public/cpp/geolocation/geoposition.h"
#include "services/device/public/mojom/geoposition.mojom.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class PublicIpAddressLocationNotifierTest : public testing::Test {
 protected:
  // Helps test a single call to
  // PublicIpAddressLocationNotifier::QueryNextPositionAfterTimestamp.
  class TestPositionQuery {
   public:
    // Provides a callback suitable to pass to QueryNextPositionAfterTimestamp.
    PublicIpAddressLocationNotifier::QueryNextPositionCallback MakeCallback() {
      return base::BindOnce(&TestPositionQuery::OnQueryNextPositionResponse,
                            base::Unretained(this));
    }

    // Optional. Wait until the callback from MakeCallback() is called.
    void Wait() { loop_.Run(); }

    const base::Optional<mojom::Geoposition>& position() const {
      return position_;
    }

   private:
    void OnQueryNextPositionResponse(const mojom::Geoposition& position) {
      position_ = position;
      loop_.Quit();
    }

    base::RunLoop loop_;
    base::Optional<mojom::Geoposition> position_;
  };

  PublicIpAddressLocationNotifierTest()
      : network_connection_tracker_(
            network::TestNetworkConnectionTracker::CreateInstance()),
        notifier_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_),
            network::TestNetworkConnectionTracker::GetInstance(),
            kTestGeolocationApiKey) {}

  ~PublicIpAddressLocationNotifierTest() override {}

  // Gives a valid JSON reponse to the specified URLFetcher.
  // For disambiguation purposes, the specified |latitude| is included in the
  // response.
  void RespondToFetchWithLatitude(const float latitude) {
    ASSERT_EQ(1, test_url_loader_factory_.NumPending());
    const std::string& request_url =
        test_url_loader_factory_.pending_requests()->back().request.url.spec();
    std::string expected_url =
        "https://www.googleapis.com/geolocation/v1/"
        "geolocate?key=";
    expected_url.append(kTestGeolocationApiKey);
    EXPECT_EQ(expected_url, request_url);

    // Issue a valid response including the specified latitude.
    const char kNetworkResponseFormatString[] =
        R"({
            "accuracy": 100.0,
            "location": {
              "lat": %f,
              "lng": 90.0
            }
          })";
    std::string body =
        base::StringPrintf(kNetworkResponseFormatString, latitude);
    test_url_loader_factory_.AddResponse(request_url, body, net::HTTP_OK);
    task_environment_.RunUntilIdle();
    test_url_loader_factory_.ClearResponses();
  }

  void RespondToFetchWithServerError() {
    ASSERT_EQ(1, test_url_loader_factory_.NumPending());
    const std::string& request_url =
        test_url_loader_factory_.pending_requests()->back().request.url.spec();

    std::string expected_url =
        "https://www.googleapis.com/geolocation/v1/"
        "geolocate?key=";
    expected_url.append(kTestGeolocationApiKey);
    EXPECT_EQ(expected_url, request_url);

    test_url_loader_factory_.AddResponse(request_url, std::string(),
                                         net::HTTP_INTERNAL_SERVER_ERROR);
    task_environment_.RunUntilIdle();
    test_url_loader_factory_.ClearResponses();
  }

  // Expects a non-empty and valid Geoposition, including the specified
  // |latitude|.
  void ExpectValidPosition(const base::Optional<mojom::Geoposition>& position,
                           const float latitude) {
    ASSERT_TRUE(position);
    EXPECT_TRUE(ValidateGeoposition(*position));
    EXPECT_FLOAT_EQ(position->latitude, latitude);
  }

  void ExpectError(const base::Optional<mojom::Geoposition>& position) {
    ASSERT_TRUE(position);
    EXPECT_THAT(position->error_code,
                mojom::Geoposition::ErrorCode::POSITION_UNAVAILABLE);
  }

  // Use a TaskRunner on which we can fast-forward time.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // Test NetworkConnectionTracker instance.
  std::unique_ptr<network::TestNetworkConnectionTracker>
      network_connection_tracker_;

  // Test URLLoaderFactory for handling requests to the geolocation API.
  network::TestURLLoaderFactory test_url_loader_factory_;

  // The object under test.
  PublicIpAddressLocationNotifier notifier_;
};

// Tests that a single initial query makes a URL fetch and returns a position.
TEST_F(PublicIpAddressLocationNotifierTest, SingleQueryReturns) {
  // Make query.
  TestPositionQuery query;
  notifier_.QueryNextPosition(base::Time::Now(),
                              PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
                              query.MakeCallback());

  // Expect a URL fetch & send a valid response.
  RespondToFetchWithLatitude(1.0f);

  // Expect the query to return.
  ExpectValidPosition(query.position(), 1.0f);
}

// Tests that a second query asking for an older timestamp gets a cached result.
TEST_F(PublicIpAddressLocationNotifierTest, OlderQueryReturnsCached) {
  const auto time = base::Time::Now();

  // Initial query.
  TestPositionQuery query_1;
  notifier_.QueryNextPosition(time, PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
                              query_1.MakeCallback());
  RespondToFetchWithLatitude(1.0f);
  ExpectValidPosition(query_1.position(), 1.0f);

  // Second query for an earlier time.
  TestPositionQuery query_2;
  notifier_.QueryNextPosition(time - base::TimeDelta::FromMinutes(5),
                              PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
                              query_2.MakeCallback());
  // Expect a cached result, so no new network request.
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());
  // Expect the same result as query_1.
  ExpectValidPosition(query_2.position(), 1.0f);
}

// Tests that a subsequent query seeking a newer geoposition does not return,
// until a network change occurs.
TEST_F(PublicIpAddressLocationNotifierTest,
       SubsequentQueryWaitsForNetworkChange) {
  // Initial query.
  TestPositionQuery query_1;
  notifier_.QueryNextPosition(base::Time::Now(),
                              PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
                              query_1.MakeCallback());
  RespondToFetchWithLatitude(1.0f);
  ExpectValidPosition(query_1.position(), 1.0f);

  // Second query seeking a position newer than the result of query_1.
  TestPositionQuery query_2;
  notifier_.QueryNextPosition(query_1.position()->timestamp,
                              PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
                              query_2.MakeCallback());
  // Expect no network request or callback.
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());
  EXPECT_FALSE(query_2.position().has_value());

  // Fake a network change notification.
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_UNKNOWN);
  // Wait for the notifier to complete its delayed reaction.
  task_environment_.FastForwardUntilNoTasksRemain();

  // Now expect a network request and query_2 to return.
  RespondToFetchWithLatitude(2.0f);
  ExpectValidPosition(query_2.position(), 2.0f);
}

// Tests that multiple network changes in a short time result in only one
// network request.
TEST_F(PublicIpAddressLocationNotifierTest,
       ConsecutiveNetworkChangesRequestsOnlyOnce) {
  // Initial query.
  TestPositionQuery query_1;
  notifier_.QueryNextPosition(base::Time::Now(),
                              PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
                              query_1.MakeCallback());
  RespondToFetchWithLatitude(1.0f);
  ExpectValidPosition(query_1.position(), 1.0f);

  // Second query seeking a position newer than the result of query_1.
  TestPositionQuery query_2;
  notifier_.QueryNextPosition(query_1.position()->timestamp,
                              PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
                              query_2.MakeCallback());
  // Expect no network request or callback since network has not changed.
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());
  EXPECT_FALSE(query_2.position().has_value());

  // Fake several consecutive network changes notification.
  for (int i = 0; i < 10; ++i) {
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_UNKNOWN);
    task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(5));
  }
  // Expect still no network request or callback.
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());
  EXPECT_FALSE(query_2.position().has_value());

  // Wait longer.
  task_environment_.FastForwardUntilNoTasksRemain();

  // Now expect a network request & query_2 to return.
  RespondToFetchWithLatitude(2.0f);
  ExpectValidPosition(query_2.position(), 2.0f);
}

// Tests multiple waiting queries.
TEST_F(PublicIpAddressLocationNotifierTest, MutipleWaitingQueries) {
  // Initial query.
  TestPositionQuery query_1;
  notifier_.QueryNextPosition(base::Time::Now(),
                              PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
                              query_1.MakeCallback());
  RespondToFetchWithLatitude(1.0f);
  ExpectValidPosition(query_1.position(), 1.0f);

  // Multiple queries seeking positions newer than the result of query_1.
  TestPositionQuery query_2;
  notifier_.QueryNextPosition(query_1.position()->timestamp,
                              PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
                              query_2.MakeCallback());
  TestPositionQuery query_3;
  notifier_.QueryNextPosition(query_1.position()->timestamp,
                              PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
                              query_3.MakeCallback());

  // Expect no network requests or callback since network has not changed.
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());
  EXPECT_FALSE(query_2.position().has_value());
  EXPECT_FALSE(query_3.position().has_value());

  // Fake a network change notification.
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_UNKNOWN);
  // Wait for the notifier to complete its delayed reaction.
  task_environment_.FastForwardUntilNoTasksRemain();

  // Now expect a network request & fake a valid response.
  RespondToFetchWithLatitude(2.0f);
  // Expect all queries to now return.
  ExpectValidPosition(query_2.position(), 2.0f);
  ExpectValidPosition(query_3.position(), 2.0f);
}

// Tests that server error is propogated to the client.
TEST_F(PublicIpAddressLocationNotifierTest, ServerError) {
  // Make query.
  TestPositionQuery query;
  notifier_.QueryNextPosition(base::Time::Now(),
                              PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
                              query.MakeCallback());
  // Expect a URL fetch & send a valid response.
  RespondToFetchWithServerError();
  // Expect the query to return.
  ExpectError(query.position());
}

}  // namespace device
