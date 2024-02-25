// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/location_arbitrator.h"

#include <map>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "services/device/geolocation/network_location_provider.h"
#include "services/device/geolocation/wifi_polling_policy.h"
#include "services/device/public/cpp/geolocation/geoposition.h"
#include "services/device/public/mojom/geolocation_internals.mojom.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace device {

// To avoid oscillations, set this to twice the expected update interval of a
// a GPS-type location provider (in case it misses a beat) plus a little.
const base::TimeDelta LocationArbitrator::kFixStaleTimeoutTimeDelta =
    base::Seconds(11);

LocationArbitrator::LocationArbitrator(
    CustomLocationProviderCallback custom_location_provider_getter,
    GeolocationManager* geolocation_manager,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner,
    const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory,
    const std::string& api_key,
    std::unique_ptr<PositionCache> position_cache,
    base::RepeatingClosure internals_updated_closure,
    NetworkLocationProvider::NetworkRequestCallback network_request_callback,
    NetworkLocationProvider::NetworkResponseCallback network_response_callback)
    : custom_location_provider_getter_(
          std::move(custom_location_provider_getter)),
      geolocation_manager_(geolocation_manager),
      main_task_runner_(main_task_runner),
      url_loader_factory_(url_loader_factory),
      api_key_(api_key),
      position_cache_(std::move(position_cache)),
      internals_updated_closure_(std::move(internals_updated_closure)),
      network_request_callback_(std::move(network_request_callback)),
      network_response_callback_(std::move(network_response_callback)) {}

LocationArbitrator::~LocationArbitrator() {
  // Release the global wifi polling policy state.
  WifiPollingPolicy::Shutdown();
}

bool LocationArbitrator::HasPermissionBeenGrantedForTest() const {
  return is_permission_granted_;
}

void LocationArbitrator::OnPermissionGranted() {
  is_permission_granted_ = true;
  for (const auto& provider : providers_)
    provider->OnPermissionGranted();
}

void LocationArbitrator::StartProvider(bool enable_high_accuracy) {
  is_running_ = true;
  enable_high_accuracy_ = enable_high_accuracy;

  if (providers_.empty()) {
    RegisterProviders();
  }
  DoStartProviders();
}

void LocationArbitrator::DoStartProviders() {
  if (providers_.empty()) {
    // If no providers are available, we report an error to avoid
    // callers waiting indefinitely for a reply.
    arbitrator_update_callback_.Run(
        this, mojom::GeopositionResult::NewError(mojom::GeopositionError::New(
                  mojom::GeopositionErrorCode::kPositionUnavailable, "", "")));
    return;
  }
  for (const auto& provider : providers_) {
    provider->StartProvider(enable_high_accuracy_);
  }
}

void LocationArbitrator::StopProvider() {
  // Reset the reference location state (provider+result)
  // so that future starts use fresh locations from
  // the newly constructed providers.
  position_provider_ = nullptr;
  result_.reset();

  providers_.clear();
  is_running_ = false;
}

void LocationArbitrator::RegisterProvider(
    std::unique_ptr<LocationProvider> provider) {
  if (!provider)
    return;
  provider->SetUpdateCallback(base::BindRepeating(
      &LocationArbitrator::OnLocationUpdate, base::Unretained(this)));
  if (is_permission_granted_)
    provider->OnPermissionGranted();
  providers_.push_back(std::move(provider));
}

void LocationArbitrator::RegisterProviders() {
  if (custom_location_provider_getter_) {
    auto custom_provider = custom_location_provider_getter_.Run();
    if (custom_provider) {
      RegisterProvider(std::move(custom_provider));
      return;
    }
  }

  auto system_provider = NewSystemLocationProvider();
  if (system_provider) {
    RegisterProvider(std::move(system_provider));
    return;
  }

  if (url_loader_factory_)
    RegisterProvider(NewNetworkLocationProvider(url_loader_factory_, api_key_));
}

void LocationArbitrator::OnLocationUpdate(
    const LocationProvider* provider,
    mojom::GeopositionResultPtr new_result) {
  DCHECK(new_result);
  DCHECK(new_result->is_error() ||
         new_result->is_position() &&
             ValidateGeoposition(*new_result->get_position()));
  if (result_ && !IsNewPositionBetter(*result_, *new_result,
                                      provider == position_provider_)) {
    return;
  }
  position_provider_ = provider;
  result_ = std::move(new_result);
  arbitrator_update_callback_.Run(this, result_.Clone());
}

const mojom::GeopositionResult* LocationArbitrator::GetPosition() {
  return result_.get();
}

void LocationArbitrator::FillDiagnostics(
    mojom::GeolocationDiagnostics& diagnostics) {
  if (!is_running_ || providers_.empty()) {
    diagnostics.provider_state =
        mojom::GeolocationDiagnostics::ProviderState::kStopped;
    return;
  }
  for (auto& provider : providers_) {
    provider->FillDiagnostics(diagnostics);
  }
  if (position_cache_) {
    diagnostics.position_cache_diagnostics =
        mojom::PositionCacheDiagnostics::New();
    position_cache_->FillDiagnostics(*diagnostics.position_cache_diagnostics);
  }
  if (WifiPollingPolicy::IsInitialized()) {
    diagnostics.wifi_polling_policy_diagnostics =
        mojom::WifiPollingPolicyDiagnostics::New();
    WifiPollingPolicy::Get()->FillDiagnostics(
        *diagnostics.wifi_polling_policy_diagnostics);
  }
}

void LocationArbitrator::SetUpdateCallback(
    const LocationProviderUpdateCallback& callback) {
  DCHECK(!callback.is_null());
  arbitrator_update_callback_ = callback;
}

std::unique_ptr<LocationProvider>
LocationArbitrator::NewNetworkLocationProvider(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& api_key) {
  DCHECK(url_loader_factory);
#if BUILDFLAG(IS_ANDROID)
  // Android uses its own SystemLocationProvider.
  return nullptr;
#else
  return std::make_unique<NetworkLocationProvider>(
      std::move(url_loader_factory), geolocation_manager_, main_task_runner_,
      api_key, position_cache_.get(), internals_updated_closure_,
      network_request_callback_, network_response_callback_);
#endif
}

std::unique_ptr<LocationProvider>
LocationArbitrator::NewSystemLocationProvider() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
  return nullptr;
#else
  return device::NewSystemLocationProvider(main_task_runner_,
                                           geolocation_manager_);
#endif
}

base::Time LocationArbitrator::GetTimeNow() const {
  return base::Time::Now();
}

bool LocationArbitrator::IsNewPositionBetter(
    const mojom::GeopositionResult& old_result,
    const mojom::GeopositionResult& new_result,
    bool from_same_provider) const {
  // Updates location_info if it's better than what we currently have,
  // or if it's a newer update from the same provider.
  if (old_result.is_error()) {
    // Older location wasn't locked.
    return true;
  }
  const mojom::Geoposition& old_position = *old_result.get_position();
  if (new_result.is_position()) {
    const mojom::Geoposition& new_position = *new_result.get_position();
    // New location is locked, let's check if it's any better.
    if (old_position.accuracy >= new_position.accuracy) {
      // Accuracy is better.
      return true;
    } else if (from_same_provider) {
      // Same provider, fresher location.
      return true;
    } else if (GetTimeNow() - old_position.timestamp >
               kFixStaleTimeoutTimeDelta) {
      // Existing fix is stale.
      return true;
    }
  }
  return false;
}

}  // namespace device
