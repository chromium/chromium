// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/location_arbitrator.h"

#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "services/device/geolocation/network_location_provider.h"
#include "services/device/geolocation/wifi_polling_policy.h"
#include "services/device/public/cpp/geolocation/geoposition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace device {

// To avoid oscillations, set this to twice the expected update interval of a
// a GPS-type location provider (in case it misses a beat) plus a little.
const base::TimeDelta LocationArbitrator::kFixStaleTimeoutTimeDelta =
    base::TimeDelta::FromSeconds(11);

LocationArbitrator::LocationArbitrator(
    const CustomLocationProviderCallback& custom_location_provider_getter,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& api_key,
    std::unique_ptr<PositionCache> position_cache)
    : custom_location_provider_getter_(custom_location_provider_getter),
      url_loader_factory_(url_loader_factory),
      api_key_(api_key),
      position_provider_(nullptr),
      is_permission_granted_(false),
      position_cache_(std::move(position_cache)),
      is_running_(false) {}

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
    mojom::Geoposition position;
    position.error_code = mojom::Geoposition::ErrorCode::POSITION_UNAVAILABLE;
    arbitrator_update_callback_.Run(this, position);
    return;
  }
  for (const auto& provider : providers_) {
    provider->StartProvider(enable_high_accuracy_);
  }
}

void LocationArbitrator::StopProvider() {
  // Reset the reference location state (provider+position)
  // so that future starts use fresh locations from
  // the newly constructed providers.
  position_provider_ = nullptr;
  position_ = mojom::Geoposition();

  providers_.clear();
  is_running_ = false;
}

void LocationArbitrator::RegisterProvider(
    std::unique_ptr<LocationProvider> provider) {
  if (!provider)
    return;
  provider->SetUpdateCallback(base::Bind(&LocationArbitrator::OnLocationUpdate,
                                         base::Unretained(this)));
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

  if (url_loader_factory_) {
    RegisterProvider(NewNetworkLocationProvider(url_loader_factory_, api_key_));
  }
}

void LocationArbitrator::OnLocationUpdate(
    const LocationProvider* provider,
    const mojom::Geoposition& new_position) {
  DCHECK(ValidateGeoposition(new_position) ||
         new_position.error_code != mojom::Geoposition::ErrorCode::NONE);
  if (!IsNewPositionBetter(position_, new_position,
                           provider == position_provider_))
    return;
  position_provider_ = provider;
  position_ = new_position;
  arbitrator_update_callback_.Run(this, position_);
}

const mojom::Geoposition& LocationArbitrator::GetPosition() {
  return position_;
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
#if defined(OS_ANDROID)
  // Android uses its own SystemLocationProvider.
  return nullptr;
#else
  return std::make_unique<NetworkLocationProvider>(
      std::move(url_loader_factory), api_key, position_cache_.get());
#endif
}

std::unique_ptr<LocationProvider>
LocationArbitrator::NewSystemLocationProvider() {
#if defined(OS_MACOSX) || defined(OS_LINUX) || defined(OS_FUCHSIA)
  return nullptr;
#else
  return device::NewSystemLocationProvider();
#endif
}

base::Time LocationArbitrator::GetTimeNow() const {
  return base::Time::Now();
}

bool LocationArbitrator::IsNewPositionBetter(
    const mojom::Geoposition& old_position,
    const mojom::Geoposition& new_position,
    bool from_same_provider) const {
  // Updates location_info if it's better than what we currently have,
  // or if it's a newer update from the same provider.
  if (!ValidateGeoposition(old_position)) {
    // Older location wasn't locked.
    return true;
  }
  if (ValidateGeoposition(new_position)) {
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
