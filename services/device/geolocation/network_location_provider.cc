// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/network_location_provider.h"

#include <iterator>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/device/geolocation/position_cache.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/cpp/geolocation/geoposition.h"
#include "services/device/public/cpp/geolocation/network_location_request_source.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(IS_APPLE)
#include "services/device/public/cpp/device_features.h"
#endif

namespace device {
namespace {
// The maximum period of time we'll wait for a complete set of wifi data
// before sending the request.
const int kDataCompleteWaitSeconds = 2;

// The maximum age of a cached network location estimate before it can no longer
// be returned as a fresh estimate. This should be at least as long as the
// longest polling interval used by the WifiDataProvider.
const int kLastPositionMaxAgeSeconds = 10 * 60;  // 10 minutes

}  // namespace

// NetworkLocationProvider
NetworkLocationProvider::NetworkLocationProvider(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& api_key,
    PositionCache* position_cache,
    base::RepeatingClosure internals_updated_closure,
    NetworkRequestCallback network_request_callback,
    NetworkResponseCallback network_response_callback)
    : wifi_data_update_callback_(
          base::BindRepeating(&NetworkLocationProvider::OnWifiDataUpdate,
                              base::Unretained(this))),
      is_wifi_data_complete_(false),
      position_cache_(position_cache),
      is_permission_granted_(false),
      is_new_data_available_(false),
      request_(new NetworkLocationRequest(
          std::move(url_loader_factory),
          api_key,
          base::BindRepeating(&NetworkLocationProvider::OnLocationResponse,
                              base::Unretained(this)))),
      internals_updated_closure_(std::move(internals_updated_closure)),
      network_request_callback_(std::move(network_request_callback)),
      network_response_callback_(std::move(network_response_callback)) {
  DCHECK(position_cache_);
  CHECK(internals_updated_closure_);
  CHECK(network_request_callback_);
  CHECK(network_response_callback_);
}

NetworkLocationProvider::~NetworkLocationProvider() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (is_started_) {
    StopProvider();
  }
}

void NetworkLocationProvider::FillDiagnostics(
    mojom::GeolocationDiagnostics& diagnostics) {
  if (is_started_) {
    if (high_accuracy_) {
      diagnostics.provider_state =
          mojom::GeolocationDiagnostics::ProviderState::kHighAccuracy;
    } else {
      diagnostics.provider_state =
          mojom::GeolocationDiagnostics::ProviderState::kLowAccuracy;
    }
  } else {
    diagnostics.provider_state =
        mojom::GeolocationDiagnostics::ProviderState::kStopped;
  }
  diagnostics.network_location_diagnostics =
      mojom::NetworkLocationDiagnostics::New();
  base::ranges::transform(
      wifi_data_.access_point_data,
      std::back_inserter(
          diagnostics.network_location_diagnostics->access_point_data),
      [](const auto& access_point) { return access_point.Clone(); });
  if (!wifi_timestamp_.is_null()) {
    diagnostics.network_location_diagnostics->wifi_timestamp = wifi_timestamp_;
  }
}

void NetworkLocationProvider::SetUpdateCallback(
    const LocationProvider::LocationProviderUpdateCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  location_provider_update_callback_ = callback;
}

void NetworkLocationProvider::OnPermissionGranted() {
  const bool was_permission_granted = is_permission_granted_;
  is_permission_granted_ = true;
  if (!was_permission_granted && is_started_) {
    RequestPosition();
    internals_updated_closure_.Run();
  }
}

void NetworkLocationProvider::OnWifiDataUpdate() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(is_started_);
  is_wifi_data_complete_ = wifi_data_provider_handle_->GetData(&wifi_data_);
  if (is_wifi_data_complete_) {
    wifi_timestamp_ = base::Time::Now();
    is_new_data_available_ = true;
  }

  // When RequestPosition is called, the most recent wifi data is sent to the
  // geolocation service. If the wifi data is incomplete but a cached estimate
  // is available, the cached estimate may be returned instead.
  //
  // If no wifi data is available or the data is incomplete, it may mean the
  // provider is still performing the wifi scan. In this case we should wait
  // for the scan to complete rather than return cached data.
  //
  // A lack of wifi data may also mean the scan is delayed due to the wifi
  // scanning policy. This delay can vary based on how frequently the wifi
  // data changes, but is on the order of a few seconds to several minutes.
  // In this case it is better to call RequestPosition and return a cached
  // position estimate if it is available.
  bool delayed = wifi_data_provider_handle_->DelayedByPolicy();
  GEOLOCATION_LOG(DEBUG)
      << "New Wi-Fi data is available: is_wifi_data_complete_="
      << is_wifi_data_complete_ << " delayed=" << delayed;
  if (is_wifi_data_complete_ || delayed)
    RequestPosition();

  internals_updated_closure_.Run();
}

void NetworkLocationProvider::OnLocationResponse(
    mojom::GeopositionResultPtr result,
    bool server_error,
    const WifiData& wifi_data,
    mojom::NetworkLocationResponsePtr response_data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  GEOLOCATION_LOG(DEBUG) << "Got new position";
  // Record the position and update our cache.
  position_cache_->SetLastUsedNetworkPosition(*result);
  if (result->is_position() && ValidateGeoposition(*result->get_position())) {
    position_cache_->CachePosition(wifi_data, *result->get_position());
  }

  // Let listeners know that we now have a position available.
  if (!location_provider_update_callback_.is_null()) {
    location_provider_update_callback_.Run(this, std::move(result));
  }
  internals_updated_closure_.Run();

  if (base::FeatureList::IsEnabled(features::kGeolocationDiagnosticsObserver)) {
    network_response_callback_.Run(std::move(response_data));
  }
}

void NetworkLocationProvider::StartProvider(bool high_accuracy) {
  GEOLOCATION_LOG(DEBUG) << "Start provider: high_accuracy=" << high_accuracy;
  DCHECK(thread_checker_.CalledOnValidThread());

  high_accuracy_ = high_accuracy;

  if (is_started_) {
    return;
  }
  is_started_ = true;

  // Registers a callback with the data provider.
  // Releasing the handle will automatically unregister the callback.
  wifi_data_provider_handle_ =
      WifiDataProviderHandle::CreateHandle(&wifi_data_update_callback_);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&NetworkLocationProvider::RequestPosition,
                     weak_factory_.GetWeakPtr()),
      base::Seconds(kDataCompleteWaitSeconds));

  OnWifiDataUpdate();
}

void NetworkLocationProvider::StopProvider() {
  GEOLOCATION_LOG(DEBUG) << "Stop provider";
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(is_started_);
  wifi_data_provider_handle_ = nullptr;
  is_started_ = false;
  weak_factory_.InvalidateWeakPtrs();
}

const mojom::GeopositionResult* NetworkLocationProvider::GetPosition() {
  return position_cache_->GetLastUsedNetworkPosition();
}

void NetworkLocationProvider::RequestPosition() {
  DCHECK(thread_checker_.CalledOnValidThread());
  GEOLOCATION_LOG(DEBUG) << "Request position: is_new_data_available_="
                         << is_new_data_available_ << " is_wifi_data_complete_="
                         << is_wifi_data_complete_;

  // The wifi polling policy may require us to wait for several minutes before
  // fresh wifi data is available. To ensure we can return a position estimate
  // quickly when the network location provider is the primary provider, allow
  // a cached value to be returned under certain conditions.
  //
  // If we have a sufficiently recent network location estimate and we do not
  // expect to receive a new one soon (i.e., no new wifi data is available and
  // there is no pending network request), report the last network position
  // estimate as if it were a fresh estimate.
  const mojom::GeopositionResult* last_result =
      position_cache_->GetLastUsedNetworkPosition();
  if (!is_new_data_available_ && !request_->is_request_pending() &&
      last_result && last_result->is_position() &&
      ValidateGeoposition(*last_result->get_position())) {
    base::Time now = base::Time::Now();
    base::TimeDelta last_position_age =
        now - last_result->get_position()->timestamp;
    if (last_position_age.InSeconds() < kLastPositionMaxAgeSeconds &&
        !location_provider_update_callback_.is_null()) {
      GEOLOCATION_LOG(DEBUG)
          << "Updating the last network position timestamp to the current time";
      // Update the timestamp to the current time.
      mojom::GeopositionResultPtr result = last_result->Clone();
      result->get_position()->timestamp = now;
      location_provider_update_callback_.Run(this, std::move(result));
    }
  }

  if (!is_new_data_available_ || !is_wifi_data_complete_)
    return;
  DCHECK(!wifi_timestamp_.is_null())
      << "|wifi_timestamp_| must be set before looking up position";

  const mojom::Geoposition* cached_position =
      position_cache_->FindPosition(wifi_data_);

  if (cached_position) {
    auto position = cached_position->Clone();
    // The timestamp of a position fix is determined by the timestamp
    // of the source data update. (The value of position.timestamp from
    // the cache could be from weeks ago!)
    position->timestamp = wifi_timestamp_;
    auto result =
        device::mojom::GeopositionResult::NewPosition(std::move(position));
    is_new_data_available_ = false;

    GEOLOCATION_LOG(DEBUG) << "Updating the cached WiFi position: ";
    // Record the position.
    position_cache_->SetLastUsedNetworkPosition(*result);

    // Let listeners know that we now have a position available.
    if (!location_provider_update_callback_.is_null())
      location_provider_update_callback_.Run(this, std::move(result));

    return;
  }
  // Don't send network requests until authorized. http://crbug.com/39171
  if (!is_permission_granted_)
    return;

  is_new_data_available_ = false;

  // TODO(joth): Rather than cancel pending requests, we should create a new
  // NetworkLocationRequest for each and hold a set of pending requests.
  DLOG_IF(WARNING, request_->is_request_pending())
      << "NetworkLocationProvider - pre-empting pending network request "
         "with new data. Wifi APs: "
      << wifi_data_.access_point_data.size();

  net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation =
      net::DefinePartialNetworkTrafficAnnotation("network_location_provider",
                                                 "network_location_request",
                                                 R"(
      semantics {
        sender: "Network Location Provider"
      }
      policy {
        setting:
          "Users can control this feature via the Location setting under "
          "'Privacy', 'Content Settings', 'Location'."
        chrome_policy {
          DefaultGeolocationSetting {
            DefaultGeolocationSetting: 2
          }
        }
      })");
  request_->MakeRequest(wifi_data_, wifi_timestamp_, partial_traffic_annotation,
                        NetworkLocationRequestSource::kNetworkLocationProvider);

  if (base::FeatureList::IsEnabled(features::kGeolocationDiagnosticsObserver)) {
    network_request_callback_.Run(request_->GetRequestDataForDiagnostics());
  }
}

}  // namespace device
