// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/network_location_provider.h"

#include <stddef.h>

#include <memory>
#include <optional>
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
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/net_errors.h"
#include "services/device/geolocation/fake_position_cache.h"
#include "services/device/geolocation/mock_wifi_data_provider.h"
#include "services/device/geolocation/wifi_data_provider.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/cpp/geolocation/geoposition.h"
#include "services/device/public/mojom/geolocation_internals.mojom.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

using ::base::test::RunClosure;
using ::base::test::TestFuture;
using ::testing::NiceMock;

mojom::NetworkLocationDiagnosticsPtr GetNetworkLocationDiagnostics(
    LocationProvider& provider) {
  auto diagnostics = mojom::GeolocationDiagnostics::New();
  provider.FillDiagnostics(*diagnostics);
  return std::move(diagnostics->network_location_diagnostics);
}

// Records the most recent position update and counts the number of times
// OnLocationUpdate is called.
struct LocationUpdateListener {
  LocationUpdateListener()
      : callback(base::BindRepeating(&LocationUpdateListener::OnLocationUpdate,
                                     base::Unretained(this))) {}

  void OnLocationUpdate(const LocationProvider* provider,
                        mojom::GeopositionResultPtr result) {
    last_result = std::move(result);
    update_count++;
    if (last_result->is_error()) {
      error_count++;
    }
  }

  const LocationProvider::LocationProviderUpdateCallback callback;
  mojom::GeopositionResultPtr last_result;
  int update_count = 0;
  int error_count = 0;
};

// Main test fixture
class GeolocationNetworkProviderTest : public testing::Test {
 public:
  void TearDown() override {
    WifiDataProviderHandle::ResetFactoryForTesting();
  }

  std::unique_ptr<LocationProvider> CreateProvider(
      bool set_permission_granted,
      const std::string& api_key = std::string()) {
    auto provider = std::make_unique<NetworkLocationProvider>(
        test_url_loader_factory_.GetSafeWeakWrapper(), api_key,
        &position_cache_, /*internals_updated_closure=*/base::DoNothing(),
        network_request_callback_.Get(), network_response_callback_.Get());

    if (set_permission_granted) {
      provider->OnPermissionGranted();
    }

    return provider;
  }

  NiceMock<base::MockCallback<NetworkLocationProvider::NetworkRequestCallback>>
      network_request_callback_;
  NiceMock<base::MockCallback<NetworkLocationProvider::NetworkResponseCallback>>
      network_response_callback_;

 protected:
  GeolocationNetworkProviderTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        wifi_data_provider_(MockWifiDataProvider::CreateInstance()) {
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
      mojom::AccessPointData ap;
      ap.mac_address = base::StringPrintf("%02d-34-56-78-54-32", i);
      ap.radio_signal_strength = ap_count - i;
      ap.channel = IndexToChannel(i);
      ap.signal_to_noise = i + 42;
      data.access_point_data.insert(ap);
    }
    return data;
  }

  static WifiData CreateReferenceWifiScanDataWithNoMACAddress(int ap_count) {
    WifiData data;
    for (int i = 0; i < ap_count; ++i) {
      mojom::AccessPointData ap;
      ap.radio_signal_strength = ap_count - i;
      ap.channel = IndexToChannel(i);
      ap.signal_to_noise = i + 42;
      data.access_point_data.insert(ap);
    }
    return data;
  }

  // Creates Wi-Fi data containing the specified number of access points with
  // optional access point properties omitted.
  static WifiData CreateReferenceWifiScanDataWithOnlyMacAddress(int ap_count) {
    WifiData data;
    for (int i = 0; i < ap_count; ++i) {
      mojom::AccessPointData ap;
      ap.mac_address = base::StringPrintf("%02d-34-56-78-54-32", i);
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

  static mojom::GeopositionResultPtr CreateReferencePosition(int id) {
    auto result =
        mojom::GeopositionResult::NewPosition(mojom::Geoposition::New());
    mojom::Geoposition& pos = *result->get_position();
    pos.latitude = id;
    pos.longitude = -(id + 1);
    pos.altitude = 2 * id;
    pos.accuracy = 3 * id;
    // Ensure last_position.timestamp be earlier than any future calls to
    // base::time::Now() as well as not old enough to be considered invalid
    // (kLastPositionMaxAgeSeconds)
    pos.timestamp = base::Time::Now() - base::Minutes(5);
    return result;
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
    if (!list) {
      return testing::AssertionFailure() << "Dictionary " << PrettyJson(dict)
                                         << " is missing list field " << field;
    }
    *output_list = list->Clone();
    return testing::AssertionSuccess();
  }

  static testing::AssertionResult JsonFieldEquals(
      const std::string& field,
      const base::Value::Dict& expected,
      const base::Value::Dict& actual) {
    const base::Value* expected_value = expected.Find(field);
    const base::Value* actual_value = actual.Find(field);
    if (!expected_value) {
      return testing::AssertionFailure()
             << "Expected dictionary " << PrettyJson(expected)
             << " is missing field " << field;
    }
    if (!actual_value) {
      return testing::AssertionFailure()
             << "Actual dictionary " << PrettyJson(actual)
             << " is missing field " << field;
    }
    if (*expected_value != *actual_value) {
      return testing::AssertionFailure()
             << "Field " << field
             << " mismatch: " << PrettyJson(*expected_value)
             << " != " << PrettyJson(*actual_value);
    }
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

    std::optional<base::Value> parsed_json =
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
      EXPECT_EQ(expected_wifi_aps_json.size(), wifi_aps_json.size());
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

  base::test::SingleThreadTaskEnvironment task_environment_;
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

  {
    const mojom::GeopositionResult* result = provider->GetPosition();
    ASSERT_TRUE(result->is_error());
    const mojom::GeopositionError& error = *result->get_error();
    EXPECT_EQ("Did not provide a good position fix", error.error_message);
    EXPECT_TRUE(error.error_technical.empty());
  }

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

  {
    const mojom::GeopositionResult* result = provider->GetPosition();
    ASSERT_TRUE(result->is_position());
    const mojom::Geoposition& position = *result->get_position();
    EXPECT_EQ(51.0, position.latitude);
    EXPECT_EQ(-0.1, position.longitude);
    EXPECT_EQ(1200.4, position.accuracy);
    EXPECT_FALSE(position.timestamp.is_null());
    EXPECT_TRUE(ValidateGeoposition(position));
  }

  // 4. Wifi updated again, with one less AP. This is 'close enough' to the
  // previous scan, so no new request made.
  const int kSecondScanAps = kFirstScanAps - 1;
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(kSecondScanAps));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());

  {
    const mojom::GeopositionResult* result = provider->GetPosition();
    ASSERT_TRUE(result->is_position());
    const mojom::Geoposition& position = *result->get_position();
    EXPECT_EQ(51.0, position.latitude);
    EXPECT_EQ(-0.1, position.longitude);
    EXPECT_TRUE(ValidateGeoposition(position));
  }

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
  {
    const mojom::GeopositionResult* result = provider->GetPosition();
    ASSERT_TRUE(result->is_error());
    const mojom::GeopositionError& error = *result->get_error();
    EXPECT_EQ("Network error. Check DevTools console for more information.",
              error.error_message);
    EXPECT_EQ(
        "Network location provider at 'https://www.googleapis.com/' : "
        "ERR_FAILED.",
        error.error_technical);
  }

  // 7. Wifi scan returns to original set: should be serviced from cache.
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(kFirstScanAps));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());

  {
    const mojom::GeopositionResult* result = provider->GetPosition();
    ASSERT_TRUE(result->is_position());
    const mojom::Geoposition& position = *result->get_position();
    EXPECT_EQ(51.0, position.latitude);
    EXPECT_EQ(-0.1, position.longitude);
    EXPECT_TRUE(ValidateGeoposition(position));
  }
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

  TestFuture<mojom::GeopositionResultPtr> future;
  provider->SetUpdateCallback(
      base::BindLambdaForTesting([&future](const LocationProvider* provider,
                                           mojom::GeopositionResultPtr result) {
        future.SetValue(std::move(result));
      }));
  const std::string& request_url =
      test_url_loader_factory_.pending_requests()->back().request.url.spec();
  test_url_loader_factory_.AddResponse(request_url, std::string(),
                                       net::HTTP_BAD_REQUEST);

  auto result = future.Take();
  ASSERT_TRUE(result->is_error());
  const mojom::GeopositionError& error = *result->get_error();
  EXPECT_EQ(
      "Failed to query location from network service. Check the DevTools "
      "console for more information.",
      error.error_message);
  EXPECT_EQ(
      "Network location provider at 'https://www.googleapis.com/' : Returned "
      "error code 400.",
      error.error_technical);
}

TEST_F(GeolocationNetworkProviderTest, NetworkRequestResponseMalformed) {
  std::unique_ptr<LocationProvider> provider(CreateProvider(true));
  provider->StartProvider(false);
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());

  TestFuture<mojom::GeopositionResultPtr> future;
  provider->SetUpdateCallback(
      base::BindLambdaForTesting([&future](const LocationProvider* provider,
                                           mojom::GeopositionResultPtr result) {
        future.SetValue(std::move(result));
      }));
  const std::string& request_url =
      test_url_loader_factory_.pending_requests()->back().request.url.spec();
  const char* kMalformedResponse =
      "{"
      "  \"status MALFORMED\""
      "}";
  test_url_loader_factory_.AddResponse(request_url, kMalformedResponse);

  auto result = future.Take();
  ASSERT_TRUE(result->is_error());
  const mojom::GeopositionError& error = *result->get_error();
  EXPECT_EQ("Response was malformed", error.error_message);
  EXPECT_TRUE(error.error_technical.empty());
}

// Tests that the provider's last position cache delegate is correctly used to
// cache the most recent network position estimate, and that this estimate is
// not lost when the provider is torn down and recreated.
TEST_F(GeolocationNetworkProviderTest, LastPositionCache) {
  std::unique_ptr<LocationProvider> provider(CreateProvider(true));
  provider->StartProvider(false);

  // Check that the provider is initialized with a nullptr position.
  EXPECT_FALSE(provider->GetPosition());

  // Check that the cached value is also nullptr.
  EXPECT_FALSE(position_cache_.GetLastUsedNetworkPosition());

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
  {
    const mojom::GeopositionResult* result = provider->GetPosition();
    ASSERT_TRUE(result && result->is_position());
    const auto& position = *result->get_position();
    EXPECT_EQ(51.0, position.latitude);
    EXPECT_EQ(-0.1, position.longitude);
    EXPECT_EQ(1200.4, position.accuracy);
    EXPECT_FALSE(position.timestamp.is_null());
    EXPECT_TRUE(ValidateGeoposition(position));
  }

  // Shut down the provider. This typically happens whenever there are no active
  // Geolocation API calls.
  provider->StopProvider();
  provider = nullptr;

  // The cache preserves the last estimate while the provider is inactive.
  {
    const mojom::GeopositionResult* result =
        position_cache_.GetLastUsedNetworkPosition();
    ASSERT_TRUE(result && result->is_position());
    const auto& position = *result->get_position();
    EXPECT_EQ(51.0, position.latitude);
    EXPECT_EQ(-0.1, position.longitude);
    EXPECT_EQ(1200.4, position.accuracy);
    EXPECT_FALSE(position.timestamp.is_null());
    EXPECT_TRUE(ValidateGeoposition(position));
  }

  // Restart the provider.
  provider = CreateProvider(true);
  provider->StartProvider(false);

  // Check that the most recent position estimate is retained.
  {
    const mojom::GeopositionResult* result = provider->GetPosition();
    ASSERT_TRUE(result && result->is_position());
    const auto& position = *result->get_position();
    EXPECT_EQ(51.0, position.latitude);
    EXPECT_EQ(-0.1, position.longitude);
    EXPECT_EQ(1200.4, position.accuracy);
    EXPECT_FALSE(position.timestamp.is_null());
    EXPECT_TRUE(ValidateGeoposition(position));
  }
}

// Tests that when the last network position estimate is sufficiently recent and
// we do not expect to receive a fresh estimate soon (no new wifi data available
// and no pending geolocation service request) then the provider may return the
// last position instead of waiting to acquire a fresh estimate.
TEST_F(GeolocationNetworkProviderTest, LastPositionCacheUsed) {
  LocationUpdateListener listener;

  // Seed the last position cache with a valid geoposition value and the
  // timestamp set to the current time.
  mojom::GeopositionResultPtr last_result = CreateReferencePosition(0);
  ASSERT_TRUE(last_result->is_position());
  EXPECT_TRUE(ValidateGeoposition(*last_result->get_position()));
  position_cache_.SetLastUsedNetworkPosition(*last_result);

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
  ASSERT_TRUE(listener.last_result && listener.last_result->is_position());
  const mojom::Geoposition& listener_last_position =
      *listener.last_result->get_position();
  EXPECT_TRUE(ValidateGeoposition(listener_last_position));
  const mojom::Geoposition& last_position = *last_result->get_position();
  EXPECT_EQ(last_position.latitude, listener_last_position.latitude);
  EXPECT_EQ(last_position.longitude, listener_last_position.longitude);
  EXPECT_EQ(last_position.accuracy, listener_last_position.accuracy);
  EXPECT_LT(last_position.timestamp, listener_last_position.timestamp);
}

// Tests that the last network position estimate is not returned if the
// estimate is too old.
TEST_F(GeolocationNetworkProviderTest, LastPositionNotUsedTooOld) {
  LocationUpdateListener listener;

  // Seed the last position cache with a geoposition value with the timestamp
  // set to 20 minutes ago.
  mojom::GeopositionResultPtr last_result = CreateReferencePosition(0);
  ASSERT_TRUE(last_result->is_position());
  auto& last_position = *last_result->get_position();
  last_position.timestamp = base::Time::Now() - base::Minutes(20);
  EXPECT_TRUE(ValidateGeoposition(*last_result->get_position()));
  position_cache_.SetLastUsedNetworkPosition(*last_result);

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
  EXPECT_FALSE(listener.last_result);
}

// Tests that the last network position estimate is not returned if there is
// new wifi data or a pending geolocation service request.
TEST_F(GeolocationNetworkProviderTest, LastPositionNotUsedNewData) {
  LocationUpdateListener listener;

  // Seed the last position cache with a valid geoposition value. The timestamp
  // of the cached position is set to the current time.
  mojom::GeopositionResultPtr last_result = CreateReferencePosition(0);
  ASSERT_TRUE(last_result->is_position());
  EXPECT_TRUE(ValidateGeoposition(*last_result->get_position()));
  position_cache_.SetLastUsedNetworkPosition(*last_result);

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
  EXPECT_FALSE(listener.last_result);

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
  EXPECT_FALSE(listener.last_result);

  // Check that a network request is pending.
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
}

TEST_F(GeolocationNetworkProviderTest, DiagnosticsEmpty) {
  auto provider = CreateProvider(/*set_permission_granted=*/true);
  auto diagnostics = GetNetworkLocationDiagnostics(*provider);
  ASSERT_TRUE(diagnostics);
  EXPECT_TRUE(diagnostics->access_point_data.empty());
  EXPECT_FALSE(diagnostics->wifi_timestamp);
}

TEST_F(GeolocationNetworkProviderTest, DiagnosticsNoAccessPoints) {
  wifi_data_provider_->set_got_data(false);  // No initial Wi-Fi data.
  auto provider = CreateProvider(/*set_permission_granted=*/true);
  provider->StartProvider(/*high_accuracy=*/false);

  // A Wi-Fi scan completes without finding any access points.
  wifi_data_provider_->SetData(/*new_data=*/{});
  base::RunLoop().RunUntilIdle();

  auto diagnostics = GetNetworkLocationDiagnostics(*provider);
  ASSERT_TRUE(diagnostics);
  EXPECT_TRUE(diagnostics->access_point_data.empty());
  EXPECT_FALSE(diagnostics->wifi_timestamp);
}

TEST_F(GeolocationNetworkProviderTest, DiagnosticsAccessPointData) {
  wifi_data_provider_->set_got_data(false);  // No initial Wi-Fi data.
  auto provider = CreateProvider(/*set_permission_granted=*/true);
  provider->StartProvider(/*high_accuracy=*/false);

  // A Wi-Fi scan completes after finding access points.
  base::RunLoop loop;
  EXPECT_CALL(network_request_callback_, Run)
      .WillOnce(RunClosure(loop.QuitClosure()));
  constexpr size_t kApCount = 6;
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(kApCount));
  const auto wifi_time = base::Time::Now();
  loop.Run();

  auto diagnostics = GetNetworkLocationDiagnostics(*provider);
  ASSERT_TRUE(diagnostics);
  EXPECT_EQ(kApCount, diagnostics->access_point_data.size());
  ASSERT_TRUE(diagnostics->wifi_timestamp);
  EXPECT_EQ(wifi_time, *diagnostics->wifi_timestamp);
}

TEST_F(GeolocationNetworkProviderTest, DiagnosticsAccessPointDataMissingKeys) {
  wifi_data_provider_->set_got_data(false);  // No initial Wi-Fi data.
  auto provider = CreateProvider(/*set_permission_granted=*/true);
  provider->StartProvider(/*high_accuracy=*/false);

  // A Wi-Fi scan completes after finding access points.
  base::RunLoop loop;
  EXPECT_CALL(network_request_callback_, Run)
      .WillOnce(RunClosure(loop.QuitClosure()));
  constexpr size_t kApCount = 6;
  wifi_data_provider_->SetData(
      CreateReferenceWifiScanDataWithOnlyMacAddress(kApCount));
  const auto wifi_time = base::Time::Now();
  loop.Run();

  auto diagnostics = GetNetworkLocationDiagnostics(*provider);
  ASSERT_TRUE(diagnostics);
  EXPECT_EQ(kApCount, diagnostics->access_point_data.size());
  ASSERT_TRUE(diagnostics->wifi_timestamp);
  EXPECT_EQ(wifi_time, *diagnostics->wifi_timestamp);
}

TEST_F(GeolocationNetworkProviderTest, DiagnosticsNetworkRequestResponse) {
  wifi_data_provider_->set_got_data(false);  // No initial Wi-Fi data.
  auto provider = CreateProvider(/*set_permission_granted=*/true);
  provider->StartProvider(/*high_accuracy=*/false);

  // Updating `wifi_data_provider_` with new data causes a network request to
  // be created. Check that the diagnostics callback is called.
  TestFuture<std::vector<mojom::AccessPointDataPtr>> request_future;
  EXPECT_CALL(network_request_callback_, Run).WillOnce([&](auto request) {
    request_future.SetValue(std::move(request));
  });
  constexpr size_t kApCount = 6;
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(kApCount));
  EXPECT_EQ(request_future.Get().size(), kApCount);
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());

  // Simulate a network response and check that the diagnostics callback is
  // called.
  TestFuture<mojom::NetworkLocationResponsePtr> response_future;
  EXPECT_CALL(network_response_callback_, Run).WillOnce([&](auto response) {
    response_future.SetValue(std::move(response));
  });
  constexpr char kReferenceNetworkResponse[] =
      "{"
      "  \"accuracy\": 1200.4,"
      "  \"location\": {"
      "    \"lat\": 51.0,"
      "    \"lng\": -0.1"
      "  }"
      "}";
  test_url_loader_factory_.AddResponse(
      test_url_loader_factory_.pending_requests()->back().request.url.spec(),
      kReferenceNetworkResponse);
  ASSERT_TRUE(response_future.Get());
  EXPECT_DOUBLE_EQ(response_future.Get()->latitude, 51.0);
  EXPECT_DOUBLE_EQ(response_future.Get()->longitude, -0.1);
  ASSERT_TRUE(response_future.Get()->accuracy.has_value());
  EXPECT_DOUBLE_EQ(response_future.Get()->accuracy.value(), 1200.4);
}

TEST_F(GeolocationNetworkProviderTest, DiagnosticsEmptyNetworkRequest) {
  wifi_data_provider_->set_got_data(false);  // No initial Wi-Fi data.
  auto provider = CreateProvider(/*set_permission_granted=*/true);
  provider->StartProvider(/*high_accuracy=*/false);

  // First simulate a non-empty Wi-Fi update. The WifiDataProvider will only
  // notify the NetworkLocationProvider if the updated access point list is
  // significantly different from the previous list, so we want to start from
  // a non-empty list.
  base::RunLoop loop;
  EXPECT_CALL(network_request_callback_, Run)
      .WillOnce(RunClosure(loop.QuitClosure()));
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(/*ap_count=*/6));
  loop.Run();
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());

  // Simulate an empty Wi-Fi data update. This can happen if Wi-Fi is
  // unavailable or there are no nearby access points. Check that no access
  // point data is included in the request.
  TestFuture<std::vector<mojom::AccessPointDataPtr>> request_future;
  EXPECT_CALL(network_request_callback_, Run).WillOnce([&](auto request) {
    request_future.SetValue(std::move(request));
  });
  wifi_data_provider_->SetData({});
  EXPECT_TRUE(request_future.Get().empty());
}

TEST_F(GeolocationNetworkProviderTest, DiagnosticsNoAccuracyInNetworkResponse) {
  wifi_data_provider_->set_got_data(false);  // No initial Wi-Fi data.
  auto provider = CreateProvider(/*set_permission_granted=*/true);
  provider->StartProvider(/*high_accuracy=*/false);

  // Simulate a Wi-Fi data update so a network request is created.
  base::RunLoop loop;
  EXPECT_CALL(network_request_callback_, Run)
      .WillOnce(RunClosure(loop.QuitClosure()));
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(/*ap_count=*/6));
  loop.Run();
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());

  // Simulate a network response containing a location but no accuracy. Check
  // that the response includes the location and the accuracy is nullopt.
  TestFuture<mojom::NetworkLocationResponsePtr> response_future;
  EXPECT_CALL(network_response_callback_, Run).WillOnce([&](auto response) {
    response_future.SetValue(std::move(response));
  });
  constexpr char kNoAccuracyNetworkResponse[] =
      "{"
      "  \"location\": {"
      "    \"lat\": 51.0,"
      "    \"lng\": -0.1"
      "  }"
      "}";
  test_url_loader_factory_.AddResponse(
      test_url_loader_factory_.pending_requests()->back().request.url.spec(),
      kNoAccuracyNetworkResponse);
  ASSERT_TRUE(response_future.Get());
  EXPECT_DOUBLE_EQ(response_future.Get()->latitude, 51.0);
  EXPECT_DOUBLE_EQ(response_future.Get()->longitude, -0.1);
  EXPECT_FALSE(response_future.Get()->accuracy.has_value());
}

TEST_F(GeolocationNetworkProviderTest, DiagnosticsNoLocationInNetworkResponse) {
  wifi_data_provider_->set_got_data(false);  // No initial Wi-Fi data.
  auto provider = CreateProvider(/*set_permission_granted=*/true);
  provider->StartProvider(/*high_accuracy=*/false);

  // Simulate a Wi-Fi data update so a network request is created.
  base::RunLoop loop;
  EXPECT_CALL(network_request_callback_, Run)
      .WillOnce(RunClosure(loop.QuitClosure()));
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(/*ap_count=*/6));
  loop.Run();
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());

  // Simulate an invalid network response. The diagnostics callback is called
  // with nullptr.
  TestFuture<mojom::NetworkLocationResponsePtr> response_future;
  EXPECT_CALL(network_response_callback_, Run).WillOnce([&](auto response) {
    response_future.SetValue(std::move(response));
  });
  constexpr char kBadNetworkResponse[] = "{ \"bad\": 123 }";
  test_url_loader_factory_.AddResponse(
      test_url_loader_factory_.pending_requests()->back().request.url.spec(),
      kBadNetworkResponse);
  EXPECT_FALSE(response_future.Get());
}

TEST_F(GeolocationNetworkProviderTest, DiagnosticsObserverDisabled) {
  // Disable the diagnostics observer feature.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kGeolocationDiagnosticsObserver});

  wifi_data_provider_->set_got_data(false);  // No initial Wi-Fi data.
  auto provider = CreateProvider(/*set_permission_granted=*/true);
  provider->StartProvider(/*high_accuracy=*/false);

  // Simulate a Wi-Fi data update so a network request is created. The
  // diagnostics callback is not called.
  EXPECT_CALL(network_request_callback_, Run).Times(0);
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(/*ap_count=*/6));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());

  // Simulate a network response. The diagnostics callback is not called.
  EXPECT_CALL(network_response_callback_, Run).Times(0);
  constexpr char kReferenceNetworkResponse[] =
      "{"
      "  \"accuracy\": 1200.4,"
      "  \"location\": {"
      "    \"lat\": 51.0,"
      "    \"lng\": -0.1"
      "  }"
      "}";
  test_url_loader_factory_.AddResponse(
      test_url_loader_factory_.pending_requests()->back().request.url.spec(),
      kReferenceNetworkResponse);
  base::RunLoop().RunUntilIdle();
}

}  // namespace device
