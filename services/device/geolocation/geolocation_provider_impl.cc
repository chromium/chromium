// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/geolocation_provider_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "net/base/network_change_notifier.h"
#include "services/device/geolocation/location_arbitrator.h"
#include "services/device/geolocation/position_cache_impl.h"
#include "services/device/public/cpp/geolocation/geoposition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "services/device/geolocation/geolocation_jni_headers/LocationProviderFactory_jni.h"
#endif

namespace device {

namespace {
base::LazyInstance<CustomLocationProviderCallback>::Leaky
    g_custom_location_provider_callback = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<std::unique_ptr<network::SharedURLLoaderFactoryInfo>>::Leaky
    g_url_loader_factory_info = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<std::string>::Leaky g_api_key = LAZY_INSTANCE_INITIALIZER;
}  // namespace

// static
GeolocationProvider* GeolocationProvider::GetInstance() {
  return GeolocationProviderImpl::GetInstance();
}

// static
void GeolocationProviderImpl::SetGeolocationConfiguration(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& api_key,
    const CustomLocationProviderCallback& custom_location_provider_getter,
    bool use_gms_core_location_provider) {
  if (url_loader_factory)
    g_url_loader_factory_info.Get() = url_loader_factory->Clone();
  g_api_key.Get() = api_key;
  g_custom_location_provider_callback.Get() = custom_location_provider_getter;
  if (use_gms_core_location_provider) {
#if defined(OS_ANDROID)
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_LocationProviderFactory_useGmsCoreLocationProvider(env);
#else
    NOTREACHED() << "GMS core location provider is only available for Android";
#endif
  }
}

std::unique_ptr<GeolocationProvider::Subscription>
GeolocationProviderImpl::AddLocationUpdateCallback(
    const LocationUpdateCallback& callback,
    bool enable_high_accuracy) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  std::unique_ptr<GeolocationProvider::Subscription> subscription;
  if (enable_high_accuracy) {
    subscription = high_accuracy_callbacks_.Add(callback);
  } else {
    subscription = low_accuracy_callbacks_.Add(callback);
  }

  OnClientsChanged();
  if (ValidateGeoposition(position_) ||
      position_.error_code != mojom::Geoposition::ErrorCode::NONE) {
    callback.Run(position_);
  }

  return subscription;
}

bool GeolocationProviderImpl::HighAccuracyLocationInUse() {
  return !high_accuracy_callbacks_.empty();
}

void GeolocationProviderImpl::OverrideLocationForTesting(
    const mojom::Geoposition& position) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  ignore_location_updates_ = true;
  NotifyClients(position);
}

void GeolocationProviderImpl::OnLocationUpdate(
    const LocationProvider* provider,
    const mojom::Geoposition& position) {
  DCHECK(OnGeolocationThread());
  // Will be true only in testing.
  if (ignore_location_updates_)
    return;
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GeolocationProviderImpl::NotifyClients,
                                base::Unretained(this), position));
}

// static
GeolocationProviderImpl* GeolocationProviderImpl::GetInstance() {
  return base::Singleton<GeolocationProviderImpl>::get();
}

void GeolocationProviderImpl::BindGeolocationControlReceiver(
    mojo::PendingReceiver<mojom::GeolocationControl> receiver) {
  // The |receiver_| has been bound already here means that more than one
  // GeolocationPermissionContext in chrome tried to bind to Device Service.
  // We only bind the first receiver. See more info in
  // geolocation_control.mojom.
  if (!receiver_.is_bound())
    receiver_.Bind(std::move(receiver));
}

void GeolocationProviderImpl::UserDidOptIntoLocationServices() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  bool was_permission_granted = user_did_opt_into_location_services_;
  user_did_opt_into_location_services_ = true;
  if (IsRunning() && !was_permission_granted)
    InformProvidersPermissionGranted();
}

GeolocationProviderImpl::GeolocationProviderImpl()
    : base::Thread("Geolocation"),
      user_did_opt_into_location_services_(false),
      ignore_location_updates_(false),
      main_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  high_accuracy_callbacks_.set_removal_callback(base::Bind(
      &GeolocationProviderImpl::OnClientsChanged, base::Unretained(this)));
  low_accuracy_callbacks_.set_removal_callback(base::Bind(
      &GeolocationProviderImpl::OnClientsChanged, base::Unretained(this)));
}

GeolocationProviderImpl::~GeolocationProviderImpl() {
  Stop();
  DCHECK(!arbitrator_);
}

void GeolocationProviderImpl::SetArbitratorForTesting(
    std::unique_ptr<LocationProvider> arbitrator) {
  arbitrator_ = std::move(arbitrator);
}

bool GeolocationProviderImpl::OnGeolocationThread() const {
  return task_runner()->BelongsToCurrentThread();
}

void GeolocationProviderImpl::OnClientsChanged() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  base::Closure task;
  if (high_accuracy_callbacks_.empty() && low_accuracy_callbacks_.empty()) {
    DCHECK(IsRunning());
    if (!ignore_location_updates_) {
      // We have no more observers, so we clear the cached geoposition so that
      // when the next observer is added we will not provide a stale position.
      position_ = mojom::Geoposition();
    }
    task = base::Bind(&GeolocationProviderImpl::StopProviders,
                      base::Unretained(this));
  } else {
    if (!IsRunning()) {
      Start();
      if (user_did_opt_into_location_services_)
        InformProvidersPermissionGranted();
    }
    // Determine a set of options that satisfies all clients.
    bool enable_high_accuracy = !high_accuracy_callbacks_.empty();

    // Send the current options to the providers as they may have changed.
    task = base::Bind(&GeolocationProviderImpl::StartProviders,
                      base::Unretained(this), enable_high_accuracy);
  }

  task_runner()->PostTask(FROM_HERE, task);
}

void GeolocationProviderImpl::StopProviders() {
  DCHECK(OnGeolocationThread());
  DCHECK(arbitrator_);
  arbitrator_->StopProvider();
}

void GeolocationProviderImpl::StartProviders(bool enable_high_accuracy) {
  DCHECK(OnGeolocationThread());
  DCHECK(arbitrator_);
  arbitrator_->StartProvider(enable_high_accuracy);
}

void GeolocationProviderImpl::InformProvidersPermissionGranted() {
  DCHECK(IsRunning());
  if (!OnGeolocationThread()) {
    task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &GeolocationProviderImpl::InformProvidersPermissionGranted,
            base::Unretained(this)));
    return;
  }
  DCHECK(OnGeolocationThread());
  DCHECK(arbitrator_);
  arbitrator_->OnPermissionGranted();
}

void GeolocationProviderImpl::NotifyClients(
    const mojom::Geoposition& position) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(ValidateGeoposition(position) ||
         position.error_code != mojom::Geoposition::ErrorCode::NONE);
  position_ = position;
  high_accuracy_callbacks_.Notify(position_);
  low_accuracy_callbacks_.Notify(position_);
}

void GeolocationProviderImpl::Init() {
  DCHECK(OnGeolocationThread());

  if (arbitrator_)
    return;

  LocationProvider::LocationProviderUpdateCallback callback = base::Bind(
      &GeolocationProviderImpl::OnLocationUpdate, base::Unretained(this));

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory;
  if (g_url_loader_factory_info.Get()) {
    url_loader_factory = network::SharedURLLoaderFactory::Create(
        std::move(g_url_loader_factory_info.Get()));
  }

  DCHECK(!net::NetworkChangeNotifier::CreateIfNeeded())
      << "PositionCacheImpl needs a global NetworkChangeNotifier";
  arbitrator_ = std::make_unique<LocationArbitrator>(
      g_custom_location_provider_callback.Get(), std::move(url_loader_factory),
      g_api_key.Get(),
      std::make_unique<PositionCacheImpl>(
          base::DefaultTickClock::GetInstance()));
  arbitrator_->SetUpdateCallback(callback);
}

void GeolocationProviderImpl::CleanUp() {
  DCHECK(OnGeolocationThread());
  arbitrator_.reset();
}

}  // namespace device
