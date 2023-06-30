// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/geolocation_provider_impl.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "build/build_config.h"
#include "net/base/network_change_notifier.h"
#include "services/device/geolocation/location_arbitrator.h"
#include "services/device/geolocation/position_cache_impl.h"
#include "services/device/public/cpp/geolocation/geoposition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "services/device/geolocation/geolocation_jni_headers/LocationProviderFactory_jni.h"
#endif

namespace device {

namespace {
base::LazyInstance<CustomLocationProviderCallback>::Leaky
    g_custom_location_provider_callback = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<std::unique_ptr<network::PendingSharedURLLoaderFactory>>::
    Leaky g_pending_url_loader_factory = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<std::string>::Leaky g_api_key = LAZY_INSTANCE_INITIALIZER;
GeolocationManager* g_geolocation_manager = nullptr;
}  // namespace

// static
GeolocationProvider* GeolocationProvider::instance_for_testing_ = nullptr;

// static
GeolocationProvider* GeolocationProvider::GetInstance() {
  if (instance_for_testing_) {
    return instance_for_testing_;
  }
  return GeolocationProviderImpl::GetInstance();
}

// static
void GeolocationProvider::SetInstanceForTesting(
    GeolocationProvider* instance_for_testing) {
  instance_for_testing_ = instance_for_testing;
}

// static
void GeolocationProviderImpl::SetGeolocationConfiguration(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& api_key,
    const CustomLocationProviderCallback& custom_location_provider_getter,
    GeolocationManager* geolocation_manager,
    bool use_gms_core_location_provider) {
  if (url_loader_factory)
    g_pending_url_loader_factory.Get() = url_loader_factory->Clone();
  g_api_key.Get() = api_key;
  g_custom_location_provider_callback.Get() = custom_location_provider_getter;
  g_geolocation_manager = geolocation_manager;
  if (use_gms_core_location_provider) {
#if BUILDFLAG(IS_ANDROID)
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_LocationProviderFactory_useGmsCoreLocationProvider(env);
#else
    NOTREACHED() << "GMS core location provider is only available for Android";
#endif
  }
}

base::CallbackListSubscription
GeolocationProviderImpl::AddLocationUpdateCallback(
    const LocationUpdateCallback& callback,
    bool enable_high_accuracy) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  base::CallbackListSubscription subscription;
  if (enable_high_accuracy) {
    subscription = high_accuracy_callbacks_.Add(callback);
  } else {
    subscription = low_accuracy_callbacks_.Add(callback);
  }

  OnClientsChanged();
  if (result_) {
    callback.Run(*result_);
  }

  return subscription;
}

bool GeolocationProviderImpl::HighAccuracyLocationInUse() {
  return !high_accuracy_callbacks_.empty();
}

void GeolocationProviderImpl::OverrideLocationForTesting(
    mojom::GeopositionResultPtr result) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  ignore_location_updates_ = true;
  NotifyClients(std::move(result));
}

void GeolocationProviderImpl::OnLocationUpdate(
    const LocationProvider* provider,
    mojom::GeopositionResultPtr result) {
  DCHECK(OnGeolocationThread());
  // Will be true only in testing.
  if (ignore_location_updates_)
    return;
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GeolocationProviderImpl::NotifyClients,
                                base::Unretained(this), std::move(result)));
}

// static
GeolocationProviderImpl* GeolocationProviderImpl::GetInstance() {
  return base::Singleton<GeolocationProviderImpl>::get();
}

void GeolocationProviderImpl::BindGeolocationControlReceiver(
    mojo::PendingReceiver<mojom::GeolocationControl> receiver) {
  // The |control_receiver_| has been bound already here means that
  // more than one GeolocationPermissionContext in chrome tried to bind to
  // Device Service. We only bind the first receiver. See more info in
  // geolocation_control.mojom.
  if (!control_receiver_.is_bound()) {
    control_receiver_.Bind(std::move(receiver));
  }
}

void GeolocationProviderImpl::BindGeolocationInternalsReceiver(
    mojo::PendingReceiver<mojom::GeolocationInternals> receiver) {
  internals_receivers_.Add(this, std::move(receiver));
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
      main_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  high_accuracy_callbacks_.set_removal_callback(base::BindRepeating(
      &GeolocationProviderImpl::OnClientsChanged, base::Unretained(this)));
  low_accuracy_callbacks_.set_removal_callback(base::BindRepeating(
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
  base::OnceClosure task;
  if (high_accuracy_callbacks_.empty() && low_accuracy_callbacks_.empty()) {
    DCHECK(IsRunning());
    if (!ignore_location_updates_) {
      // We have no more observers, so we clear the cached geoposition so that
      // when the next observer is added we will not provide a stale position.
      result_.reset();
    }
    task = base::BindOnce(&GeolocationProviderImpl::StopProviders,
                          base::Unretained(this));
  } else {
    if (!IsRunning()) {
      base::Thread::Options options;
#if BUILDFLAG(IS_APPLE)
      options.message_pump_type = base::MessagePumpType::NS_RUNLOOP;
#endif
      StartWithOptions(std::move(options));
      if (user_did_opt_into_location_services_)
        InformProvidersPermissionGranted();
    }
    // Determine a set of options that satisfies all clients.
    bool enable_high_accuracy = !high_accuracy_callbacks_.empty();

    // Send the current options to the providers as they may have changed.
    task = base::BindOnce(&GeolocationProviderImpl::StartProviders,
                          base::Unretained(this), enable_high_accuracy);
  }

  task_runner()->PostTask(FROM_HERE, std::move(task));
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
    mojom::GeopositionResultPtr result) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(result);
  if (result->is_position() && !ValidateGeoposition(*result->get_position())) {
    return;
  }
  result_ = std::move(result);
  high_accuracy_callbacks_.Notify(*result_);
  low_accuracy_callbacks_.Notify(*result_);
}

void GeolocationProviderImpl::Init() {
  DCHECK(OnGeolocationThread());

  if (arbitrator_)
    return;

  LocationProvider::LocationProviderUpdateCallback callback =
      base::BindRepeating(&GeolocationProviderImpl::OnLocationUpdate,
                          base::Unretained(this));

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory;
  if (g_pending_url_loader_factory.Get()) {
    url_loader_factory = network::SharedURLLoaderFactory::Create(
        std::move(g_pending_url_loader_factory.Get()));
  }

  DCHECK(!net::NetworkChangeNotifier::CreateIfNeeded())
      << "PositionCacheImpl needs a global NetworkChangeNotifier";
  arbitrator_ = std::make_unique<LocationArbitrator>(
      g_custom_location_provider_callback.Get(), g_geolocation_manager,
      main_task_runner_, std::move(url_loader_factory), g_api_key.Get(),
      std::make_unique<PositionCacheImpl>(
          base::DefaultTickClock::GetInstance()));
  arbitrator_->SetUpdateCallback(callback);
}

void GeolocationProviderImpl::CleanUp() {
  DCHECK(OnGeolocationThread());
  arbitrator_.reset();
}

void GeolocationProviderImpl::GetDiagnostics(GetDiagnosticsCallback callback) {
  CHECK(main_task_runner_->BelongsToCurrentThread());

  if (!arbitrator_) {
    std::move(callback).Run(nullptr);
    return;
  }

  // Using `base::Unretained` is safe here because |task_runner()| is
  // bound to `GeolocationProviderImpl`.
  task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &GeolocationProviderImpl::GetInternalsDataOnGeolocationThread,
          base::Unretained(this)),
      std::move(callback));

  return;
}

mojom::GeolocationDiagnosticsPtr
GeolocationProviderImpl::GetInternalsDataOnGeolocationThread() {
  CHECK(OnGeolocationThread());
  mojom::GeolocationDiagnosticsPtr result =
      mojom::GeolocationDiagnostics::New();
  arbitrator_->FillDiagnostics(*result);
  return result;
}

}  // namespace device
