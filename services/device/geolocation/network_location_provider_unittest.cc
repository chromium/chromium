// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/network_location_provider.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/repeating_test_future.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/net_errors.h"
#include "services/device/geolocation/fake_position_cache.h"
#include "services/device/geolocation/location_arbitrator.h"
#include "services/device/geolocation/wifi_data_provider.h"
#include "services/device/public/cpp/geolocation/geoposition.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC)
#include "services/device/public/cpp/test/fake_geolocation_manager.h"
#endif

namespace device {

using ::base::test::RepeatingTestFuture;

// Records the most recent position update and counts the number of times
// OnLocationUpdate is called.
struct LocationUpdateListener {
  LocationUpdateListener()
      : callback(base::BindRepeating(&LocationUpdateListener::OnLocationUpdate,
                                     base::Unretained(this))) {}

  void OnLocationUpdate(const LocationProvider* provider,
                        const mojom::Geoposition& position) {
    last_position = position;
    update_count++;
    if (position.error_code != mojom::Geoposition::ErrorCode::NONE)
      error_count++;
  }

  const LocationProvider::LocationProviderUpdateCallback callback;
  mojom::Geoposition last_position;
  int update_count = 0;
  int error_count = 0;
};

// A mock implementation of WifiDataProvider for testing. Adapted from
// http://gears.googlecode.com/svn/trunk/gears/geolocation/geolocation_test.cc
class MockWifiDataProvider : public WifiDataProvider {
 public:
  // Factory method for use with WifiDataProvider::SetFactoryForTesting.
  static WifiDataProvider* GetInstance() {
    CHECK(instance_);
    return instance_;
  }

  static MockWifiDataProvider* CreateInstance() {
    CHECK(!instance_);
    instance_ = new MockWifiDataProvider;
    return instance_;
  }

  MockWifiDataProvider() : start_calls_(0), stop_calls_(0), got_data_(true) {}

  MockWifiDataProvider(const MockWifiDataProvider&) = delete;
  MockWifiDataProvider& operator=(const MockWifiDataProvider&) = delete;

  // WifiDataProvider implementation.
  void StartDataProvider() override { ++start_calls_; }

  void StopDataProvider() override { ++stop_calls_; }

  bool DelayedByPolicy() override { return false; }

  bool GetData(WifiData* data_out) override {
    CHECK(data_out);
    *data_out = data_;
    return got_data_;
  }

  void ForceRescan() override {}

  void SetData(const WifiData& new_data) {
    got_data_ = true;
    const bool differs = data_.DiffersSignificantly(new_data);
    data_ = new_data;
    if (differs)
      this->RunCallbacks();
  }

  void set_got_data(bool got_data) { got_data_ = got_data; }
  int start_calls_;
  int stop_calls_;

 private:
  ~MockWifiDataProvider() override {
    CHECK(this == instance_);
    instance_ = nullptr;
  }

  static MockWifiDataProvider* instance_;

  WifiData data_;
  bool got_data_;
};

MockWifiDataProvider* MockWifiDataProvider::instance_ = nullptr;

// Main test fixture
class GeolocationNetworkProviderTest : public testing::Test {
 public:
  void TearDown() override {
    WifiDataProviderHandle::ResetFactoryForTesting();
    grant_system_permission_by_default_ = true;
  }

  std::unique_ptr<LocationProvider> CreateProvider(
      bool set_permission_granted,
      const std::string& api_key = std::string()) {
#if BUILDFLAG(IS_MAC)
    fake_geolocation_manager_ = std::make_unique<FakeGeolocationManager>();
    auto provider = std::make_unique<NetworkLocationProvider>(
        test_url_loader_factory_.GetSafeWeakWrapper(),
        fake_geolocation_manager_.get(),
        base::SingleThreadTaskRunner::GetCurrentDefault(), api_key,
        &position_cache_);
    // For macOS we must simulate the granting of location permission
    if (grant_system_permission_by_default_) {
      fake_geolocation_manager_->SetSystemPermission(
          LocationSystemPermissionStatus::kAllowed);
      base::RunLoop().RunUntilIdle();
    }
#else
    auto provider = std::make_unique<NetworkLocationProvider>(
        test_url_loader_factory_.GetSafeWeakWrapper(),
        /*geolocation_system_permission_manager=*/nullptr,
        base::SingleThreadTaskRunner::GetCurrentDefault(), api_key,
        &position_cache_);
#endif
    if (set_permission_granted)
      provider->OnPermissionGranted();

    return provider;
  }

  bool grant_system_permission_by_default_ = true;

#if BUILDFLAG(IS_MAC)
  std::unique_ptr<FakeGeolocationManager> fake_geolocation_manager_;
#endif

 protected:
  GeolocationNetworkProviderTest()
      : wifi_data_provider_(MockWifiDataProvider::CreateInstance()) {
    // TODO(joth): Really these should be in SetUp, not here, but they take no
    // effect on Mac OS Release builds if done there. I kid not. Figure out why.
    WifiDataProviderHandle::SetFactoryForTesting(
        MockWifiDataProvider::GetInstance);
  }

  static int IndexToChannel(int index) { return index + 4; }

  // Creates wifi data containing the specified number of access points, with
  // some differentiating charactistics in each.
  static WifiData CreateReferenceWifiScanData(int ap_count) {
    WifiData data;
    for (int i = 0; i < ap_count; ++i) {
      AccessPointData ap;
      ap.mac_address =
          base::ASCIIToUTF16(base::StringPrintf("%02d-34-56-78-54-32", i));
      ap.radio_signal_strength = ap_count - i;
      ap.channel = IndexToChannel(i);
      ap.signal_to_noise = i + 42;
      ap.ssid = u"Some nice+network|name\\";
      data.access_point_data.insert(ap);
    }
    return data;
  }

  static WifiData CreateReferenceWifiScanDataWithNoMACAddress(int ap_count) {
    WifiData data;
    for (int i = 0; i < ap_count; ++i) {
      AccessPointData ap;
      ap.radio_signal_strength = ap_count - i;
      ap.channel = IndexToChannel(i);
      ap.signal_to_noise = i + 42;
      ap.ssid = u"Some nice+network|name\\";
      data.access_point_data.insert(ap);
    }
    return data;
  }

  static void CreateReferenceWifiScanDataJson(
      int ap_count,
      int start_index,
      base::Value::List* wifi_access_point_list) {
    std::vector<std::string> wifi_data;
    for (int i = 0; i < ap_count; ++i) {
      base::Value::Dict ap;
      ap.Set("macAddress", base::StringPrintf("%02d-34-56-78-54-32", i));
      ap.Set("signalStrength", start_index + ap_count - i);
      ap.Set("age", 0);
      ap.Set("channel", IndexToChannel(i));
      ap.Set("signalToNoiseRatio", i + 42);
      wifi_access_point_list->Append(std::move(ap));
    }
  }

  static mojom::Geoposition CreateReferencePosition(int id) {
    mojom::Geoposition pos;
    pos.latitude = id;
    pos.longitude = -(id + 1);
    pos.altitude = 2 * id;
    pos.accuracy = 3 * id;
    // Ensure last_position.timestamp be earlier than any future calls to
    // base::time::Now() as well as not old enough to be considered invalid
    // (kLastPositionMaxAgeSeconds)
    pos.timestamp = base::Time::Now() - base::Minutes(5);
    return pos;
  }

  static std::string PrettyJson(const base::Value& value) {
    std::string pretty;
    base::JSONWriter::WriteWithOptions(
        value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &pretty);
    return pretty;
  }

  static std::string PrettyJson(const base::Value::Dict& dict_value) {
    return PrettyJson(base::Value(dict_value.Clone()));
  }

  static testing::AssertionResult JsonGetList(const std::string& field,
                                              const base::Value::Dict& dict,
                                              base::Value::List* output_list) {
    const base::Value::List* list = dict.FindList(field);
    if (!list)
      return testing::AssertionFailure() << "Dictionary " << PrettyJson(dict)
                                         << " is missing list field " << field;
    *output_list = list->Clone();
    return testing::AssertionSuccess();
  }

  static testing::AssertionResult JsonFieldEquals(
      const std::string& field,
      const base::Value::Dict& expected,
      const base::Value::Dict& actual) {
    const base::Value* expected_value = expected.Find(field);
    const base::Value* actual_value = actual.Find(field);
    if (!expected_value)
      return testing::AssertionFailure()
             << "Expected dictionary " << PrettyJson(expected)
             << " is missing field " << field;
    if (!actual_value)
      return testing::AssertionFailure()
             << "Actual dictionary " << PrettyJson(actual)
             << " is missing field " << field;
    if (*expected_value != *actual_value)
      return testing::AssertionFailure()
             << "Field " << field
             << " mismatch: " << PrettyJson(*expected_value)
             << " != " << PrettyJson(*actual_value);
    return testing::AssertionSuccess();
  }

  // Checks that current pending request contains valid JSON upload data. The
  // WiFi access points specified in the JSON are validated against the first
  // |expected_wifi_aps| access points, starting from position
  // |wifi_start_index|, that are generated by CreateReferenceWifiScanDataJson.
  void CheckRequestIsValid(int expected_wifi_aps, int wifi_start_index) {
    ASSERT_EQ(1, test_url_loader_factory_.NumPending());
    const network::TestURLLoaderFactory::PendingRequest& pending_request =
        test_url_loader_factory_.pending_requests()->back();
    EXPECT_TRUE(pending_request.client.is_connected());
    std::string upload_data = network::GetUploadData(pending_request.request);
    ASSERT_FALSE(upload_data.empty());

    absl::optional<base::Value> parsed_json =
        base::JSONReader::Read(upload_data);
    ASSERT_TRUE(parsed_json);
    ASSERT_TRUE(parsed_json->is_dict());

    const base::Value::Dict& request_json = parsed_json->GetDict();

    if (expected_wifi_aps) {
      base::Value::List expected_wifi_aps_json;
      CreateReferenceWifiScanDataJson(expected_wifi_aps, wifi_start_index,
                                      &expected_wifi_aps_json);
      EXPECT_EQ(size_t(expected_wifi_aps), expected_wifi_aps_json.size());

      base::Value::List wifi_aps_json;
      ASSERT_TRUE(
          JsonGetList("wifiAccessPoints", request_json, &wifi_aps_json));
      for (size_t i = 0; i < expected_wifi_aps_json.size(); ++i) {
        const base::Value& expected_json_value = expected_wifi_aps_json[i];
        ASSERT_TRUE(expected_json_value.is_dict());
        const base::Value::Dict& expected_json = expected_json_value.GetDict();
        const base::Value& actual_json_value = wifi_aps_json[i];
        ASSERT_TRUE(actual_json_value.is_dict());
        const base::Value::Dict& actual_json = actual_json_value.GetDict();
        ASSERT_TRUE(JsonFieldEquals("macAddress", expected_json, actual_json));
        ASSERT_TRUE(
            JsonFieldEquals("signalStrength", expected_json, actual_json));
        ASSERT_TRUE(JsonFieldEquals("channel", expected_json, actual_json));
        ASSERT_TRUE(
            JsonFieldEquals("signalToNoiseRatio", expected_json, actual_json));
      }
    } else {
      ASSERT_FALSE(request_json.Find("wifiAccessPoints"));
    }
  }

  const base::test::SingleThreadTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  const scoped_refptr<MockWifiDataProvider> wifi_data_provider_;
  FakePositionCache position_cache_;
};

// Tests that fixture members were SetUp correctly.
TEST_F(GeolocationNetworkProviderTest, CreateDestroy) {
  std::unique_ptr<LocationProvider> provider(CreateProvider(true));
  EXPECT_TRUE(provider);
  provider.reset();
  SUCCEED();
}

// Tests that, with an empty api_key, no query string parameter is included in
// the request.
TEST_F(GeolocationNetworkProviderTest, EmptyApiKey) {
  const std::string api_key = "";
  std::unique_ptr<LocationProvider> provider(CreateProvider(true, api_key));
  provider->StartProvider(false);

  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  const GURL& request_url =
      test_url_loader_factory_.pending_requests()->back().request.url;
  EXPECT_FALSE(request_url.has_query());
}

// Tests that, with non-empty api_key, a "key" query string parameter is
// included in the request.
TEST_F(GeolocationNetworkProviderTest, NonEmptyApiKey) {
  const std::string api_key = "something";
  std::unique_ptr<LocationProvider> provider(CreateProvider(true, api_key));
  provider->StartProvider(false);

  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  const GURL& request_url =
      test_url_loader_factory_.pending_requests()->back().request.url;
  EXPECT_TRUE(request_url.has_query());
  EXPECT_TRUE(base::StartsWith(request_url.query_piece(), "key="));
}

// Tests that, after StartProvider(), a TestURLFetcher can be extracted,
// representing a valid request.
TEST_F(GeolocationNetworkProviderTest, StartProvider) {
  std::unique_ptr<LocationProvider> provider(CreateProvider(true));
  provider->StartProvider(false);

  CheckRequestIsValid(0, 0);
}

// Tests that, with a very large number of access points, the set of access
// points represented in the request is truncated to fit within 2048 characters.
TEST_F(GeolocationNetworkProviderTest, StartProviderLongRequest) {
  std::unique_ptr<LocationProvider> provider(CreateProvider(true));
  provider->StartProvider(false);
  // Create Wifi scan data with too many access points.
  const int kFirstScanAps = 20;
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(kFirstScanAps));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  const std::string& request_url =
      test_url_loader_factory_.pending_requests()->back().request.url.spec();
  // The request url should have been shortened to less than 2048 characters
  // in length by not including access points with the lowest signal strength
  // in the request.
  EXPECT_LT(request_url.size(), size_t(2048));
  // Expect only 16 out of 20 original access points.
  CheckRequestIsValid(16, 4);
}

TEST_F(GeolocationNetworkProviderTest, StartProviderNoMacAddress) {
  std::unique_ptr<LocationProvider> provider(CreateProvider(true));
  provider->StartProvider(false);
  // Create Wifi scan data with no MAC Addresses.
  const int kFirstScanAps = 5;
  wifi_data_provider_->SetData(
      CreateReferenceWifiScanDataWithNoMACAddress(kFirstScanAps));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1, test_url_loader_factory_.NumPending());

  test_url_loader_factory_.pending_requests()->back().request.url.spec();

  // Expect only 0 out of 5 original access points. since none of them have
  // MAC Addresses.
  CheckRequestIsValid(0, 0);
}

// Tests that the provider issues the right requests, and provides the right
// GetPosition() results based on the responses, for the following complex
// sequence of Wifi data situations:
// 1. Initial "no fix" response -> provide 'invalid' position.
// 2. Wifi data arrives -> make new request.
// 3. Response has good fix -> provide corresponding position.
// 4. Wifi data changes slightly -> no new request.
// 5. Wifi data changes a lot -> new request.
// 6. Response is error -> provide 'invalid' position.
// 7. Wifi data changes back to (2.) -> no new request, provide cached position.
TEST_F(GeolocationNetworkProviderTest, MultipleWifiScansComplete) {
  std::unique_ptr<LocationProvider> provider(CreateProvider(true));
  provider->StartProvider(false);
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());

  // 1. Complete the network request with bad position fix.
  const std::string& request_url_1 =
      test_url_loader_factory_.pending_requests()->back().request.url.spec();
  const char* kNoFixNetworkResponse =
      "{"
      "  \"status\": \"ZERO_RESULTS\""
      "}";
  test_url_loader_factory_.AddResponse(request_url_1, kNoFixNetworkResponse);
  base::RunLoop().RunUntilIdle();
  test_url_loader_factory_.ClearResponses();

  mojom::Geoposition position = provider->GetPosition();
  EXPECT_FALSE(ValidateGeoposition(position));
  EXPECT_EQ("Did not provide a good position fix", position.error_message);
  EXPECT_TRUE(position.error_technical.empty());

  // 2. Now wifi data arrives -- SetData will notify listeners.
  const int kFirstScanAps = 6;
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(kFirstScanAps));
  base::RunLoop().RunUntilIdle();

  // The request should have the wifi data.
  CheckRequestIsValid(kFirstScanAps, 0);

  // 3. Send a reply with good position fix.
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  const std::string& request_url_2 =
      test_url_loader_factory_.pending_requests()->back().request.url.spec();
  const char* kReferenceNetworkResponse =
      "{"
      "  \"accuracy\": 1200.4,"
      "  \"location\": {"
      "    \"lat\": 51.0,"
      "    \"lng\": -0.1"
      "  }"
      "}";
  test_url_loader_factory_.AddResponse(request_url_2,
                                       kReferenceNetworkResponse);
  base::RunLoop().RunUntilIdle();
  test_url_loader_factory_.ClearResponses();

  position = provider->GetPosition();
  EXPECT_EQ(51.0, position.latitude);
  EXPECT_EQ(-0.1, position.longitude);
  EXPECT_EQ(1200.4, position.accuracy);
  EXPECT_FALSE(position.timestamp.is_null());
  EXPECT_TRUE(ValidateGeoposition(position));

  // 4. Wifi updated again, with one less AP. This is 'close enough' to the
  // previous scan, so no new request made.
  const int kSecondScanAps = kFirstScanAps - 1;
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(kSecondScanAps));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());

  position = provider->GetPosition();
  EXPECT_EQ(51.0, position.latitude);
  EXPECT_EQ(-0.1, position.longitude);
  EXPECT_TRUE(ValidateGeoposition(position));

  // 5. Now a third scan with more than twice the original APs -> new request.
  const int kThirdScanAps = kFirstScanAps * 2 + 1;
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(kThirdScanAps));
  base::RunLoop().RunUntilIdle();
  CheckRequestIsValid(kThirdScanAps, 0);

  // 6. ...reply with a network error.
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  const GURL& request_url_3 =
      test_url_loader_factory_.pending_requests()->back().request.url;
  test_url_loader_factory_.AddResponse(
      request_url_3, network::mojom::URLResponseHead::New(), std::string(),
      network::URLLoaderCompletionStatus(net::ERR_FAILED));
  base::RunLoop().RunUntilIdle();

  // Error means we now no longer have a fix.
  position = provider->GetPosition();
  EXPECT_FALSE(ValidateGeoposition(position));
  EXPECT_EQ("Network error. Check DevTools console for more information.",
            position.error_message);
  EXPECT_EQ(
      "Network location provider at 'https://www.googleapis.com/' : "
      "ERR_FAILED.",
      position.error_technical);

  // 7. Wifi scan returns to original set: should be serviced from cache.
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(kFirstScanAps));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());

  position = provider->GetPosition();
  EXPECT_EQ(51.0, position.latitude);
  EXPECT_EQ(-0.1, position.longitude);
  EXPECT_TRUE(ValidateGeoposition(position));
}

// Tests that, if no Wifi scan data is available at startup, the provider
// doesn't initiate a request, until Wifi data later becomes available.
TEST_F(GeolocationNetworkProviderTest, NoRequestOnStartupUntilWifiData) {
  LocationUpdateListener listener;
  wifi_data_provider_->set_got_data(false);  // No initial Wifi data.
  std::unique_ptr<LocationProvider> provider(CreateProvider(true));
  provider->StartProvider(false);

  provider->SetUpdateCallback(listener.callback);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, test_url_loader_factory_.NumPending())
      << "Network request should not be created right away on startup when "
         "wifi data has not yet arrived";

  // Now Wifi data becomes available.
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(1));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
}

// Tests that, even if a request is already in flight, new wifi data results in
// a new request being sent.
TEST_F(GeolocationNetworkProviderTest, NewDataReplacesExistingNetworkRequest) {
  // Send initial request with empty data
  std::unique_ptr<LocationProvider> provider(CreateProvider(true));
  provider->StartProvider(false);
  CheckRequestIsValid(0, 0);

  // Now wifi data arrives; new request should be sent.
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(4));
  base::RunLoop().RunUntilIdle();
  CheckRequestIsValid(4, 0);
}

// Tests that, if user geolocation permission hasn't been granted during
// startup, the provider doesn't initiate a request until it is notified of the
// user granting permission.
TEST_F(GeolocationNetworkProviderTest, NetworkRequestDeferredForPermission) {
  std::unique_ptr<LocationProvider> provider(CreateProvider(false));
  provider->StartProvider(false);
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());

  provider->OnPermissionGranted();
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
}

// Tests that, even if new Wifi data arrives, the provider doesn't initiate its
// first request unless & until the user grants permission.
TEST_F(GeolocationNetworkProviderTest,
       NetworkRequestWithWifiDataDeferredForPermission) {
  std::unique_ptr<LocationProvider> provider(CreateProvider(false));
  provider->StartProvider(false);
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());

  static const int kScanCount = 4;
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(kScanCount));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());

  provider->OnPermissionGranted();
  CheckRequestIsValid(kScanCount, 0);
}

TEST_F(GeolocationNetworkProviderTest, NetworkRequestServiceBadRequest) {
  std::unique_ptr<LocationProvider> provider(CreateProvider(true));
  provider->StartProvider(false);
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());

  RepeatingTestFuture<mojom::GeopositionPtr> future;
  provider->SetUpdateCallback(
      base::BindLambdaForTesting([&future](const LocationProvider* provider,
                                           const mojom::Geoposition& position) {
        future.AddValue(position.Clone());
      }));
  const std::string& request_url =
      test_url_loader_factory_.pending_requests()->back().request.url.spec();
  test_url_loader_factory_.AddResponse(request_url, std::string(),
                                       net::HTTP_BAD_REQUEST);

  auto position = future.Take();
  EXPECT_EQ(
      "Failed to query location from network service. Check the DevTools "
      "console for more information.",
      position->error_message);
  EXPECT_EQ(
      "Network location provider at 'https://www.googleapis.com/' : Returned "
      "error code 400.",
      position->error_technical);
}

TEST_F(GeolocationNetworkProviderTest, NetworkRequestResponseMalformed) {
  std::unique_ptr<LocationProvider> provider(CreateProvider(true));
  provider->StartProvider(false);
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());

  RepeatingTestFuture<mojom::GeopositionPtr> future;
  provider->SetUpdateCallback(
      base::BindLambdaForTesting([&future](const LocationProvider* provider,
                                           const mojom::Geoposition& position) {
        future.AddValue(position.Clone());
      }));
  const std::string& request_url =
      test_url_loader_factory_.pending_requests()->back().request.url.spec();
  const char* kMalformedResponse =
      "{"
      "  \"status MALFORMED\""
      "}";
  test_url_loader_factory_.AddResponse(request_url, kMalformedResponse);

  auto position = future.Take();
  EXPECT_EQ("Response was malformed", position->error_message);
  EXPECT_TRUE(position->error_technical.empty());
}

#if BUILDFLAG(IS_MAC)
// Tests that, callbacks and network requests are never made until we have
// system location permission.
TEST_F(GeolocationNetworkProviderTest, MacOSSystemPermissionsTest) {
  // Do not grant system permission when creating the provider.
  grant_system_permission_by_default_ = false;

  LocationUpdateListener listener;
  mojom::Geoposition last_position = CreateReferencePosition(0);
  EXPECT_TRUE(ValidateGeoposition(last_position));
  // Set up a fake cached position so the NetworkLocationProvider would be able
  // to call the update callback if permission was allowed.
  position_cache_.SetLastUsedNetworkPosition(last_position);

  wifi_data_provider_->set_got_data(false);

  std::unique_ptr<LocationProvider> provider(
      CreateProvider(/*set_permission_granted=*/false));
  provider->StartProvider(/*high_accuracy=*/false);
  provider->SetUpdateCallback(listener.callback);

  // Under normal circumstances, when there is no initial wifi data
  // RequestPosition is not called until a few seconds after the provider is
  // started to allow time for the wifi scan to complete. To avoid waiting,
  // grant permissions once the provider is running to cause RequestPosition to
  // be called immediately.
  provider->OnPermissionGranted();

  // Ensure there was an error callback.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, listener.error_count);

  // Now try to make a request for new wifi data.
  wifi_data_provider_->set_got_data(true);
  provider->StopProvider();
  provider->StartProvider(false);

  // Normally when starting the provider a network request should be sent
  // out. This is tested in other tests. However, when we do not have system
  // permission we should not send out any requests.
  ASSERT_EQ(0, test_url_loader_factory_.NumPending());

  // Now wifi data arrives; new request will try to send but should not
  // because we do not have permission.
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(2));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(0, test_url_loader_factory_.NumPending());

  // Ensure that we immediately make a new network request to acquire the
  // location when permission is granted.
  static_cast<NetworkLocationProvider*>(provider.get())
      ->OnSystemPermissionUpdated(LocationSystemPermissionStatus::kAllowed);
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());

  // Clear pending requests for later testing.
  const std::string& request_url =
      test_url_loader_factory_.pending_requests()->back().request.url.spec();
  const char* kReferenceNetworkResponse =
      R"({
        "accuracy": 1200.4,
        "location": {
          "lat": 51.0,
          "lng": -0.1
        }
      })";
  test_url_loader_factory_.AddResponse(request_url, kReferenceNetworkResponse);
  base::RunLoop().RunUntilIdle();
  test_url_loader_factory_.ClearResponses();

  // Ensure more network requests are not sent out when permission is denied
  // again.
  static_cast<NetworkLocationProvider*>(provider.get())
      ->OnSystemPermissionUpdated(LocationSystemPermissionStatus::kDenied);
  provider->StartProvider(false);
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(4));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(0, test_url_loader_factory_.NumPending());
}
#endif

// Tests that the provider's last position cache delegate is correctly used to
// cache the most recent network position estimate, and that this estimate is
// not lost when the provider is torn down and recreated.
TEST_F(GeolocationNetworkProviderTest, LastPositionCache) {
  std::unique_ptr<LocationProvider> provider(CreateProvider(true));
  provider->StartProvider(false);

  // Check that the provider is initialized with an invalid position.
  mojom::Geoposition position = provider->GetPosition();
  EXPECT_FALSE(ValidateGeoposition(position));

  // Check that the cached value is also invalid.
  position = position_cache_.GetLastUsedNetworkPosition();
  EXPECT_FALSE(ValidateGeoposition(position));

  // Now wifi data arrives -- SetData will notify listeners.
  const int kFirstScanAps = 6;
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(kFirstScanAps));
  base::RunLoop().RunUntilIdle();
  // The request should have the wifi data.
  CheckRequestIsValid(kFirstScanAps, 0);

  // Send a reply with good position fix.
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  const std::string& request_url =
      test_url_loader_factory_.pending_requests()->back().request.url.spec();
  const char* kReferenceNetworkResponse =
      "{"
      "  \"accuracy\": 1200.4,"
      "  \"location\": {"
      "    \"lat\": 51.0,"
      "    \"lng\": -0.1"
      "  }"
      "}";
  test_url_loader_factory_.AddResponse(request_url, kReferenceNetworkResponse);
  base::RunLoop().RunUntilIdle();

  // The provider should return the position as the current best estimate.
  position = provider->GetPosition();
  EXPECT_EQ(51.0, position.latitude);
  EXPECT_EQ(-0.1, position.longitude);
  EXPECT_EQ(1200.4, position.accuracy);
  EXPECT_FALSE(position.timestamp.is_null());
  EXPECT_TRUE(ValidateGeoposition(position));

  // Shut down the provider. This typically happens whenever there are no active
  // Geolocation API calls.
  provider->StopProvider();
  provider = nullptr;

  // The cache preserves the last estimate while the provider is inactive.
  position = position_cache_.GetLastUsedNetworkPosition();
  EXPECT_EQ(51.0, position.latitude);
  EXPECT_EQ(-0.1, position.longitude);
  EXPECT_EQ(1200.4, position.accuracy);
  EXPECT_FALSE(position.timestamp.is_null());
  EXPECT_TRUE(ValidateGeoposition(position));

  // Restart the provider.
  provider = CreateProvider(true);
  provider->StartProvider(false);

  // Check that the most recent position estimate is retained.
  position = provider->GetPosition();
  EXPECT_EQ(51.0, position.latitude);
  EXPECT_EQ(-0.1, position.longitude);
  EXPECT_EQ(1200.4, position.accuracy);
  EXPECT_FALSE(position.timestamp.is_null());
  EXPECT_TRUE(ValidateGeoposition(position));
}

// Tests that when the last network position estimate is sufficiently recent and
// we do not expect to receive a fresh estimate soon (no new wifi data available
// and no pending geolocation service request) then the provider may return the
// last position instead of waiting to acquire a fresh estimate.
TEST_F(GeolocationNetworkProviderTest, LastPositionCacheUsed) {
  LocationUpdateListener listener;

  // Seed the last position cache with a valid geoposition value and the
  // timestamp set to the current time.
  mojom::Geoposition last_position = CreateReferencePosition(0);
  EXPECT_TRUE(ValidateGeoposition(last_position));
  position_cache_.SetLastUsedNetworkPosition(last_position);

  // Simulate no initial wifi data.
  wifi_data_provider_->set_got_data(false);

  // Start the provider without geolocation permission.
  std::unique_ptr<LocationProvider> provider(CreateProvider(false));
  provider->StartProvider(false);

  // Register a location update callback. The listener will count how many times
  // OnLocationUpdate is called.
  provider->SetUpdateCallback(listener.callback);

  // Under normal circumstances, when there is no initial wifi data
  // RequestPosition is not called until a few seconds after the provider is
  // started to allow time for the wifi scan to complete. To avoid waiting,
  // grant permissions once the provider is running to cause RequestPosition to
  // be called immediately.
  provider->OnPermissionGranted();

  base::RunLoop().RunUntilIdle();

  // Check that the listener received the position update and that the position
  // is the same as the seeded value except for the timestamp, which should be
  // newer.
  EXPECT_EQ(1, listener.update_count);
  EXPECT_TRUE(ValidateGeoposition(listener.last_position));
  EXPECT_EQ(last_position.latitude, listener.last_position.latitude);
  EXPECT_EQ(last_position.longitude, listener.last_position.longitude);
  EXPECT_EQ(last_position.accuracy, listener.last_position.accuracy);
  EXPECT_LT(last_position.timestamp, listener.last_position.timestamp);
}

// Tests that the last network position estimate is not returned if the
// estimate is too old.
TEST_F(GeolocationNetworkProviderTest, LastPositionNotUsedTooOld) {
  LocationUpdateListener listener;

  // Seed the last position cache with a geoposition value with the timestamp
  // set to 20 minutes ago.
  mojom::Geoposition last_position = CreateReferencePosition(0);
  last_position.timestamp = base::Time::Now() - base::Minutes(20);
  EXPECT_TRUE(ValidateGeoposition(last_position));
  position_cache_.SetLastUsedNetworkPosition(last_position);

  // Simulate no initial wifi data.
  wifi_data_provider_->set_got_data(false);

  // Start the provider without geolocation permission.
  std::unique_ptr<LocationProvider> provider(CreateProvider(false));
  provider->StartProvider(false);

  // Register a location update callback. The listener will count how many times
  // OnLocationUpdate is called.
  provider->SetUpdateCallback(listener.callback);

  // Under normal circumstances, when there is no initial wifi data
  // RequestPosition is not called until a few seconds after the provider is
  // started to allow time for the wifi scan to complete. To avoid waiting,
  // grant permissions once the provider is running to cause RequestPosition to
  // be called immediately.
  provider->OnPermissionGranted();

  base::RunLoop().RunUntilIdle();

  // Check that the listener received no updates.
  EXPECT_EQ(0, listener.update_count);
  EXPECT_FALSE(ValidateGeoposition(listener.last_position));
}

// Tests that the last network position estimate is not returned if there is
// new wifi data or a pending geolocation service request.
TEST_F(GeolocationNetworkProviderTest, LastPositionNotUsedNewData) {
  LocationUpdateListener listener;

  // Seed the last position cache with a valid geoposition value. The timestamp
  // of the cached position is set to the current time.
  mojom::Geoposition last_position = CreateReferencePosition(0);
  EXPECT_TRUE(ValidateGeoposition(last_position));
  position_cache_.SetLastUsedNetworkPosition(last_position);

  // Simulate a completed wifi scan.
  const int kFirstScanAps = 6;
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(kFirstScanAps));

  // Create the provider without permissions enabled.
  std::unique_ptr<LocationProvider> provider(CreateProvider(false));

  // Register a location update callback. The callback will count how many times
  // OnLocationUpdate is called.
  provider->SetUpdateCallback(listener.callback);

  // Start the provider.
  provider->StartProvider(false);
  base::RunLoop().RunUntilIdle();

  // The listener should not receive any updates. There is a valid cached value
  // but it should not be sent while we have pending wifi data.
  EXPECT_EQ(0, listener.update_count);
  EXPECT_FALSE(ValidateGeoposition(listener.last_position));

  // Check that there is no pending network request.
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());

  // Simulate no new wifi data.
  wifi_data_provider_->set_got_data(false);

  // Grant permission to allow the network request to proceed.
  provider->OnPermissionGranted();
  base::RunLoop().RunUntilIdle();

  // The listener should still not receive any updates. There is a valid cached
  // value and no new wifi data, but the cached value should not be sent while
  // we have a pending request to the geolocation service.
  EXPECT_EQ(0, listener.update_count);
  EXPECT_FALSE(ValidateGeoposition(listener.last_position));

  // Check that a network request is pending.
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
}

}  // namespace device
