// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/public_ip_address_geolocator.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/task_environment.h"
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

  // Invokes QueryNextPosition on |public_ip_address_geolocator_|, and runs
  // |done_closure| when the response comes back.
  void QueryNextPosition(base::OnceClosure done_closure) {
    public_ip_address_geolocator_->QueryNextPosition(base::BindOnce(
        &PublicIpAddressGeolocatorTest::OnQueryNextPositionResponse,
        base::Unretained(this), std::move(done_closure)));
  }

  // Callback for QueryNextPosition() that records the result in |result_| and
  // then invokes |done_closure|.
  void OnQueryNextPositionResponse(base::OnceClosure done_closure,
                                   mojom::GeopositionResultPtr result) {
    result_ = std::move(result);
    std::move(done_closure).Run();
  }

  // Result of the latest completed call to QueryNextPosition.
  mojom::GeopositionResultPtr result_;

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
  base::RunLoop loop;
  QueryNextPosition(loop.QuitClosure());

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
  loop.Run();

  ASSERT_TRUE(result_->is_position());
  const auto& position = *result_->get_position();
  EXPECT_THAT(position.accuracy, testing::Eq(100.0));
  EXPECT_THAT(position.latitude, testing::Eq(10.0));
  EXPECT_THAT(position.longitude, testing::Eq(20.0));
  EXPECT_THAT(bad_messages_, testing::IsEmpty());
}

// Tests that multiple overlapping calls to QueryNextPosition result in a
// connection error and reports a bad message.
TEST_F(PublicIpAddressGeolocatorTest, ProhibitedOverlappingCalls) {
  base::RunLoop loop;
  public_ip_address_geolocator_.set_disconnect_handler(loop.QuitClosure());

  // Issue two overlapping calls to QueryNextPosition.
  QueryNextPosition(base::NullCallback());
  QueryNextPosition(base::NullCallback());

  // This terminates only in case of connection error, which we expect.
  loop.Run();

  // Verify that the geolocator reported a bad message.
  EXPECT_THAT(bad_messages_, testing::SizeIs(1));
}

}  // namespace
}  // namespace device
