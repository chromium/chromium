// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/network_location_request.h"

#include <stdint.h>

#include <limits>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/device/geolocation/location_arbitrator.h"
#include "services/device/public/cpp/geolocation/geoposition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace device {
namespace {

const char kNetworkLocationBaseUrl[] =
    "https://www.googleapis.com/geolocation/v1/geolocate";

const char kLocationString[] = "location";
const char kLatitudeString[] = "lat";
const char kLongitudeString[] = "lng";
const char kAccuracyString[] = "accuracy";

enum NetworkLocationRequestEvent {
  // NOTE: Do not renumber these as that would confuse interpretation of
  // previously logged data. When making changes, also update the enum list
  // in tools/metrics/histograms/histograms.xml to keep it in sync.
  NETWORK_LOCATION_REQUEST_EVENT_REQUEST_START = 0,
  NETWORK_LOCATION_REQUEST_EVENT_REQUEST_CANCEL = 1,
  NETWORK_LOCATION_REQUEST_EVENT_RESPONSE_SUCCESS = 2,
  NETWORK_LOCATION_REQUEST_EVENT_RESPONSE_NOT_OK = 3,
  NETWORK_LOCATION_REQUEST_EVENT_RESPONSE_EMPTY = 4,
  NETWORK_LOCATION_REQUEST_EVENT_RESPONSE_MALFORMED = 5,
  NETWORK_LOCATION_REQUEST_EVENT_RESPONSE_INVALID_FIX = 6,

  // NOTE: Add entries only immediately above this line.
  NETWORK_LOCATION_REQUEST_EVENT_COUNT = 7
};

void RecordUmaEvent(NetworkLocationRequestEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Geolocation.NetworkLocationRequest.Event", event,
                            NETWORK_LOCATION_REQUEST_EVENT_COUNT);
}

void RecordUmaResponseCode(int code) {
  base::UmaHistogramSparse("Geolocation.NetworkLocationRequest.ResponseCode",
                           code);
}

void RecordUmaAccessPoints(int count) {
  const int min = 1;
  const int max = 20;
  const int buckets = 21;
  UMA_HISTOGRAM_CUSTOM_COUNTS("Geolocation.NetworkLocationRequest.AccessPoints",
                              count, min, max, buckets);
}

// Local functions

// Returns a URL for a request to the Google Maps geolocation API. If the
// specified |api_key| is not empty, it is escaped and included as a query
// string parameter.
GURL FormRequestURL(const std::string& api_key);

void FormUploadData(const WifiData& wifi_data,
                    const base::Time& wifi_timestamp,
                    std::string* upload_data);

// Attempts to extract a position from the response. Detects and indicates
// various failure cases.
void GetLocationFromResponse(int net_error,
                             int status_code,
                             std::unique_ptr<std::string> response_body,
                             const base::Time& wifi_timestamp,
                             const GURL& server_url,
                             mojom::Geoposition* position);

// Parses the server response body. Returns true if parsing was successful.
// Sets |*position| to the parsed location if a valid fix was received,
// otherwise leaves it unchanged.
bool ParseServerResponse(const std::string& response_body,
                         const base::Time& wifi_timestamp,
                         mojom::Geoposition* position);
void AddWifiData(const WifiData& wifi_data,
                 int age_milliseconds,
                 base::DictionaryValue* request);
}  // namespace

NetworkLocationRequest::NetworkLocationRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& api_key,
    LocationResponseCallback callback)
    : url_loader_factory_(std::move(url_loader_factory)),
      api_key_(api_key),
      location_response_callback_(std::move(callback)) {}

NetworkLocationRequest::~NetworkLocationRequest() = default;

bool NetworkLocationRequest::MakeRequest(
    const WifiData& wifi_data,
    const base::Time& wifi_timestamp,
    const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation) {
  RecordUmaEvent(NETWORK_LOCATION_REQUEST_EVENT_REQUEST_START);
  RecordUmaAccessPoints(wifi_data.access_point_data.size());
  if (url_loader_) {
    DVLOG(1) << "NetworkLocationRequest : Cancelling pending request";
    RecordUmaEvent(NETWORK_LOCATION_REQUEST_EVENT_REQUEST_CANCEL);
    url_loader_.reset();
  }
  wifi_data_ = wifi_data;
  wifi_timestamp_ = wifi_timestamp;

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::CompleteNetworkTrafficAnnotation("network_location_request",
                                            partial_traffic_annotation,
                                            R"(
        semantics {
          description:
            "Obtains geo position based on current IP address and local "
            "network information including Wi-Fi access points (even if you’re "
            "not using them)."
          trigger:
            "Location requests are sent when the page requests them or new "
            "IP address is available."
          data: "Wi-Fi data, IP address."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
      })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = "POST";
  resource_request->url = FormRequestURL(api_key_);
  DCHECK(resource_request->url.is_valid());
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);

  std::string upload_data;
  FormUploadData(wifi_data, wifi_timestamp, &upload_data);
  url_loader_->AttachStringForUpload(upload_data, "application/json");

  request_start_time_ = base::TimeTicks::Now();
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&NetworkLocationRequest::OnRequestComplete,
                     base::Unretained(this)),
      1024 * 1024 /* 1 MiB */);
  return true;
}

void NetworkLocationRequest::OnRequestComplete(
    std::unique_ptr<std::string> data) {
  int net_error = url_loader_->NetError();

  int response_code = 0;
  if (url_loader_->ResponseInfo())
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  RecordUmaResponseCode(response_code);

  mojom::Geoposition position;
  GetLocationFromResponse(net_error, response_code, std::move(data),
                          wifi_timestamp_, url_loader_->GetFinalURL(),
                          &position);

  bool server_error =
      net_error != net::OK || (response_code >= 500 && response_code < 600);
  if (!server_error) {
    const base::TimeDelta request_time =
        base::TimeTicks::Now() - request_start_time_;

    UMA_HISTOGRAM_CUSTOM_TIMES("Net.Wifi.LbsLatency", request_time,
                               base::TimeDelta::FromMilliseconds(1),
                               base::TimeDelta::FromSeconds(10), 100);
  }

  url_loader_.reset();

  DVLOG(1) << "NetworkLocationRequest::OnURLFetchComplete() : run callback.";
  location_response_callback_.Run(position, server_error, wifi_data_);
}

// Local functions.
namespace {

struct AccessPointLess {
  bool operator()(const AccessPointData* ap1,
                  const AccessPointData* ap2) const {
    return ap2->radio_signal_strength < ap1->radio_signal_strength;
  }
};

GURL FormRequestURL(const std::string& api_key) {
  GURL url(kNetworkLocationBaseUrl);
  if (!api_key.empty()) {
    std::string query(url.query());
    if (!query.empty())
      query += "&";
    query += "key=" + net::EscapeQueryParamValue(api_key, true);
    GURL::Replacements replacements;
    replacements.SetQueryStr(query);
    return url.ReplaceComponents(replacements);
  }
  return url;
}

void FormUploadData(const WifiData& wifi_data,
                    const base::Time& wifi_timestamp,
                    std::string* upload_data) {
  int age = std::numeric_limits<int32_t>::min();  // Invalid so AddInteger()
                                                  // will ignore.
  if (!wifi_timestamp.is_null()) {
    // Convert absolute timestamps into a relative age.
    int64_t delta_ms = (base::Time::Now() - wifi_timestamp).InMilliseconds();
    if (delta_ms >= 0 && delta_ms < std::numeric_limits<int32_t>::max())
      age = static_cast<int>(delta_ms);
  }

  base::DictionaryValue request;
  AddWifiData(wifi_data, age, &request);
  base::JSONWriter::Write(request, upload_data);
}

void AddString(const std::string& property_name,
               const std::string& value,
               base::DictionaryValue* dict) {
  DCHECK(dict);
  if (!value.empty())
    dict->SetString(property_name, value);
}

void AddInteger(const std::string& property_name,
                int value,
                base::DictionaryValue* dict) {
  DCHECK(dict);
  if (value != std::numeric_limits<int32_t>::min())
    dict->SetInteger(property_name, value);
}

void AddWifiData(const WifiData& wifi_data,
                 int age_milliseconds,
                 base::DictionaryValue* request) {
  DCHECK(request);

  if (wifi_data.access_point_data.empty())
    return;

  typedef std::multiset<const AccessPointData*, AccessPointLess> AccessPointSet;
  AccessPointSet access_points_by_signal_strength;

  for (const auto& ap_data : wifi_data.access_point_data)
    access_points_by_signal_strength.insert(&ap_data);

  auto wifi_access_point_list = std::make_unique<base::ListValue>();
  for (auto* ap_data : access_points_by_signal_strength) {
    auto wifi_dict = std::make_unique<base::DictionaryValue>();
    auto macAddress = base::UTF16ToUTF8(ap_data->mac_address);
    if (macAddress.empty())
      continue;
    AddString("macAddress", macAddress, wifi_dict.get());
    AddInteger("signalStrength", ap_data->radio_signal_strength,
               wifi_dict.get());
    AddInteger("age", age_milliseconds, wifi_dict.get());
    AddInteger("channel", ap_data->channel, wifi_dict.get());
    AddInteger("signalToNoiseRatio", ap_data->signal_to_noise, wifi_dict.get());
    wifi_access_point_list->Append(std::move(wifi_dict));
  }
  if (!wifi_access_point_list->empty())
    request->Set("wifiAccessPoints", std::move(wifi_access_point_list));
}

void FormatPositionError(const GURL& server_url,
                         const std::string& message,
                         mojom::Geoposition* position) {
  position->error_code = mojom::Geoposition::ErrorCode::POSITION_UNAVAILABLE;
  position->error_message = "Network location provider at '";
  position->error_message += server_url.GetOrigin().spec();
  position->error_message += "' : ";
  position->error_message += message;
  position->error_message += ".";
  VLOG(1) << "NetworkLocationRequest::GetLocationFromResponse() : "
          << position->error_message;
}

void GetLocationFromResponse(int net_error,
                             int status_code,
                             std::unique_ptr<std::string> response_body,
                             const base::Time& wifi_timestamp,
                             const GURL& server_url,
                             mojom::Geoposition* position) {
  DCHECK(position);

  // HttpPost can fail for a number of reasons. Most likely this is because
  // we're offline, or there was no response.
  if (net_error != net::OK) {
    FormatPositionError(server_url, "No response received", position);
    RecordUmaEvent(NETWORK_LOCATION_REQUEST_EVENT_RESPONSE_EMPTY);
    return;
  }

  if (status_code != 200) {  // HTTP OK.
    std::string message = "Returned error code ";
    message += base::NumberToString(status_code);
    FormatPositionError(server_url, message, position);
    RecordUmaEvent(NETWORK_LOCATION_REQUEST_EVENT_RESPONSE_NOT_OK);
    return;
  }

  // We use the timestamp from the wifi data that was used to generate
  // this position fix.
  DCHECK(response_body);
  if (!ParseServerResponse(*response_body, wifi_timestamp, position)) {
    // We failed to parse the repsonse.
    FormatPositionError(server_url, "Response was malformed", position);
    RecordUmaEvent(NETWORK_LOCATION_REQUEST_EVENT_RESPONSE_MALFORMED);
    return;
  }

  // The response was successfully parsed, but it may not be a valid
  // position fix.
  if (!ValidateGeoposition(*position)) {
    FormatPositionError(server_url, "Did not provide a good position fix",
                        position);
    RecordUmaEvent(NETWORK_LOCATION_REQUEST_EVENT_RESPONSE_INVALID_FIX);
    return;
  }

  RecordUmaEvent(NETWORK_LOCATION_REQUEST_EVENT_RESPONSE_SUCCESS);
}

// Numeric values without a decimal point have type integer and IsDouble() will
// return false. This is convenience function for detecting integer or floating
// point numeric values. Note that isIntegral() includes boolean values, which
// is not what we want.
bool GetAsDouble(const base::DictionaryValue& object,
                 const std::string& property_name,
                 double* out) {
  DCHECK(out);
  const base::Value* value = NULL;
  if (!object.Get(property_name, &value))
    return false;
  int value_as_int;
  DCHECK(value);
  if (value->GetAsInteger(&value_as_int)) {
    *out = value_as_int;
    return true;
  }
  return value->GetAsDouble(out);
}

bool ParseServerResponse(const std::string& response_body,
                         const base::Time& wifi_timestamp,
                         mojom::Geoposition* position) {
  DCHECK(position);
  DCHECK(!ValidateGeoposition(*position));
  DCHECK(position->error_code == mojom::Geoposition::ErrorCode::NONE);
  DCHECK(!wifi_timestamp.is_null());

  if (response_body.empty()) {
    LOG(WARNING) << "ParseServerResponse() : Response was empty.";
    return false;
  }
  DVLOG(1) << "ParseServerResponse() : Parsing response " << response_body;

  // Parse the response, ignoring comments.
  std::string error_msg;
  std::unique_ptr<base::Value> response_value =
      base::JSONReader::ReadAndReturnErrorDeprecated(
          response_body, base::JSON_PARSE_RFC, NULL, &error_msg);
  if (response_value == NULL) {
    LOG(WARNING) << "ParseServerResponse() : JSONReader failed : " << error_msg;
    return false;
  }

  if (!response_value->is_dict()) {
    VLOG(1) << "ParseServerResponse() : Unexpected response type "
            << response_value->type();
    return false;
  }
  const base::DictionaryValue* response_object =
      static_cast<base::DictionaryValue*>(response_value.get());

  // Get the location
  const base::Value* location_value = NULL;
  if (!response_object->Get(kLocationString, &location_value)) {
    VLOG(1) << "ParseServerResponse() : Missing location attribute.";
    // GLS returns a response with no location property to represent
    // no fix available; return true to indicate successful parse.
    return true;
  }
  DCHECK(location_value);

  if (!location_value->is_dict()) {
    if (!location_value->is_none()) {
      VLOG(1) << "ParseServerResponse() : Unexpected location type "
              << location_value->type();
      // If the network provider was unable to provide a position fix, it should
      // return a HTTP 200, with "location" : null. Otherwise it's an error.
      return false;
    }
    return true;  // Successfully parsed response containing no fix.
  }
  const base::DictionaryValue* location_object =
      static_cast<const base::DictionaryValue*>(location_value);

  // latitude and longitude fields are always required.
  double latitude = 0;
  double longitude = 0;
  if (!GetAsDouble(*location_object, kLatitudeString, &latitude) ||
      !GetAsDouble(*location_object, kLongitudeString, &longitude)) {
    VLOG(1) << "ParseServerResponse() : location lacks lat and/or long.";
    return false;
  }
  // All error paths covered: now start actually modifying postion.
  position->latitude = latitude;
  position->longitude = longitude;
  position->timestamp = wifi_timestamp;

  // Other fields are optional.
  GetAsDouble(*response_object, kAccuracyString, &position->accuracy);

  return true;
}

}  // namespace

}  // namespace device
