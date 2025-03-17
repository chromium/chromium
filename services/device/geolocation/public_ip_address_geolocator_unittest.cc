// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/public_ip_address_geolocator.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "mojo/public/cpp/system/functions.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/device/public/mojom/geolocation_client_id.mojom.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {
namespace {

const char kTestGeolocationApiKey[] = "";
using ::base::test::TestFuture;

class PublicIpAddressGeolocatorTest : public testing::Test {
 public:
  PublicIpAddressGeolocatorTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        network_connection_tracker_(
            network::TestNetworkConnectionTracker::CreateInstance()) {
    notifier_ = std::make_unique<PublicIpAddressLocationNotifier>(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        network::TestNetworkConnectionTracker::GetInstance(),
        kTestGeolocationApiKey);
  }

  PublicIpAddressGeolocatorTest(const PublicIpAddressGeolocatorTest&) = delete;
  PublicIpAddressGeolocatorTest& operator=(
      const PublicIpAddressGeolocatorTest&) = delete;

  ~PublicIpAddressGeolocatorTest() override {}

 protected:
  void SetUp() override {
    // Intercept Mojo bad-message errors.
    mojo::SetDefaultProcessErrorHandler(
        base::BindRepeating(&PublicIpAddressGeolocatorTest::OnMojoBadMessage,
                            base::Unretained(this)));

    receiver_set_.Add(
        std::make_unique<PublicIpAddressGeolocator>(
            PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS, notifier_.get(),
            mojom::GeolocationClientId::kForTesting,
            base::BindRepeating(
                &PublicIpAddressGeolocatorTest::OnGeolocatorBadMessage,
                base::Unretained(this))),
        public_ip_address_geolocator_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    // Stop intercepting Mojo bad-message errors.
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());
  }

  // Deal with mojo bad message.
  void OnMojoBadMessage(const std::string& error) {
    bad_messages_.push_back(error);
  }

  // Deal with PublicIpAddressGeolocator bad message.
  void OnGeolocatorBadMessage(const std::string& message) {
    receiver_set_.ReportBadMessage(message);
  }

  // UniqueReceiverSet to mojom::Geolocation.
  mojo::UniqueReceiverSet<mojom::Geolocation> receiver_set_;

  // Test task runner.
  base::test::TaskEnvironment task_environment_;

  // List of any Mojo bad-message errors raised.
  std::vector<std::string> bad_messages_;

  // Test NetworkConnectionTracker for PublicIpAddressLocationNotifier.
  std::unique_ptr<network::TestNetworkConnectionTracker>
      network_connection_tracker_;

  // PublicIpAddressGeolocator requires a notifier.
  std::unique_ptr<PublicIpAddressLocationNotifier> notifier_;

  // The object under test.
  mojo::Remote<mojom::Geolocation> public_ip_address_geolocator_;

  // Test URLLoaderFactory for handling requests to the geolocation API.
  network::TestURLLoaderFactory test_url_loader_factory_;
};

// Basic test of a client invoking QueryNextPosition.
TEST_F(PublicIpAddressGeolocatorTest, BindAndQuery) {
  // Invoke QueryNextPosition.
  TestFuture<mojom::GeopositionResultPtr> update_future;
  public_ip_address_geolocator_->QueryNextPosition(update_future.GetCallback());
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  const std::string& request_url =
      test_url_loader_factory_.pending_requests()->back().request.url.spec();
  EXPECT_TRUE(
      base::StartsWith("https://www.googleapis.com/geolocation/v1/geolocate",
                       request_url, base::CompareCase::SENSITIVE));

  // Issue a valid response.
  test_url_loader_factory_.AddResponse(request_url, R"({
        "accuracy": 100.0,
        "location": {
          "lat": 10.0,
          "lng": 20.0
        }
      })",
                                       net::HTTP_OK);

  // Wait for QueryNextPosition to return.
  auto result = update_future.Take();

  ASSERT_TRUE(result->is_position());
  const auto& position = *result->get_position();
  EXPECT_THAT(position.accuracy, testing::Eq(100.0));
  EXPECT_THAT(position.latitude, testing::Eq(10.0));
  EXPECT_THAT(position.longitude, testing::Eq(20.0));
  EXPECT_THAT(bad_messages_, testing::IsEmpty());
}

// Tests that multiple overlapping calls to QueryNextPosition result in a
// connection error and reports a bad message.
TEST_F(PublicIpAddressGeolocatorTest, ProhibitedOverlappingCalls) {
  TestFuture<void> disconnect_handler_future;
  TestFuture<mojom::GeopositionResultPtr> never_fired_future;
  public_ip_address_geolocator_.set_disconnect_handler(
      disconnect_handler_future.GetCallback());

  // Issue two overlapping calls to QueryNextPosition.
  public_ip_address_geolocator_->QueryNextPosition(
      never_fired_future.GetCallback());
  public_ip_address_geolocator_->QueryNextPosition(
      never_fired_future.GetCallback());

  // This terminates only in case of connection error, which we expect.
  EXPECT_TRUE(disconnect_handler_future.Wait());
  EXPECT_FALSE(never_fired_future.IsReady());

  // Verify that the geolocator reported a bad message.
  EXPECT_THAT(bad_messages_, testing::SizeIs(1));
}

}  // namespace
}  // namespace device
