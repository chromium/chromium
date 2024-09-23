// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/geolocation_provider_impl.h"

#include <iterator>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"
#include "net/base/network_change_notifier.h"
#include "services/device/geolocation/location_provider_manager.h"
#include "services/device/geolocation/position_cache_impl.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/cpp/geolocation/geoposition.h"
#include "services/device/public/cpp/geolocation/location_system_permission_status.h"
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
GeolocationSystemPermissionManager* g_geolocation_system_permission_manager =
    nullptr;
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
    GeolocationSystemPermissionManager* geolocation_system_permission_manager,
    bool use_gms_core_location_provider) {
  if (url_loader_factory)
    g_pending_url_loader_factory.Get() = url_loader_factory->Clone();
  g_api_key.Get() = api_key;
  g_custom_location_provider_callback.Get() = custom_location_provider_getter;
  g_geolocation_system_permission_manager =
      geolocation_system_permission_manager;
  if (use_gms_core_location_provider) {
#if BUILDFLAG(IS_ANDROID)
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_LocationProviderFactory_useGmsCoreLocationProvider(env);
#else
    NOTREACHED_IN_MIGRATION()
        << "GMS core location provider is only available for Android";
#endif
  }
}

// static
void GeolocationProviderImpl::SetGeolocationSystemPermissionManagerForTesting(
    GeolocationSystemPermissionManager* instance_for_testing) {
  g_geolocation_system_permission_manager = instance_for_testing;
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
  internals_observers_.set_disconnect_handler(base::BindRepeating(
      &GeolocationProviderImpl::OnInternalsObserverDisconnected,
      base::Unretained(this)));

#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
  if (features::IsOsLevelGeolocationPermissionSupportEnabled() &&
      g_geolocation_system_permission_manager) {
    observers_ = g_geolocation_system_permission_manager->GetObserverList();
    observers_->AddObserver(this);
    system_permission_status_ =
        g_geolocation_system_permission_manager->GetSystemPermission();
  } else {
    // Some unit tests for this component might not need a fully
    // initialized system permission manager. In these cases, simulate the
    // system permission as 'granted' to proceed with testing location provider
    // logic.
    system_permission_status_ = LocationSystemPermissionStatus::kAllowed;
  }
#endif
}

GeolocationProviderImpl::~GeolocationProviderImpl() {
#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
  if (features::IsOsLevelGeolocationPermissionSupportEnabled() && observers_) {
    observers_->RemoveObserver(this);
  }
#endif
  Stop();
  DCHECK(!location_provider_manager_);
}

void GeolocationProviderImpl::SetLocationProviderManagerForTesting(
    std::unique_ptr<LocationProvider> location_provider_manager) {
  location_provider_manager_ = std::move(location_provider_manager);
}

bool GeolocationProviderImpl::OnGeolocationThread() const {
  return task_runner()->BelongsToCurrentThread();
}

void GeolocationProviderImpl::OnClientsChanged() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (high_accuracy_callbacks_.empty() && low_accuracy_callbacks_.empty()) {
    DCHECK(IsRunning());
    if (!ignore_location_updates_) {
      // We have no more observers, so we clear the cached geoposition so that
      // when the next observer is added we will not provide a stale position.
      result_.reset();
    }
    task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&GeolocationProviderImpl::StopProviders,
                                  base::Unretained(this)));
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
#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
    // Handle system permission states:
    // - kAllowed: Start providers (allows re-entry for accuracy updates).
    // - kDenied: Use previously generated error result (no action here).
    // - kUndetermined: Wait for OnSystemPermissionUpdated() to handle changes
    // (no action here).
    if (features::IsOsLevelGeolocationPermissionSupportEnabled() &&
        system_permission_status_ != LocationSystemPermissionStatus::kAllowed) {
      return;
    }
#endif
    DoStartProvidersOnGeolocationThread();
  }
}

void GeolocationProviderImpl::OnInternalsUpdated() {
  CHECK(OnGeolocationThread());
  if (!diagnostics_enabled_) {
    return;
  }
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GeolocationProviderImpl::NotifyInternalsUpdated,
                     base::Unretained(this),
                     EnableAndGetDiagnosticsOnGeolocationThread()));
}

void GeolocationProviderImpl::OnNetworkLocationRequested(
    std::vector<mojom::AccessPointDataPtr> request) {
  CHECK(OnGeolocationThread());
  if (!diagnostics_enabled_) {
    return;
  }
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GeolocationProviderImpl::NotifyNetworkLocationRequested,
                     base::Unretained(this), std::move(request)));
}

void GeolocationProviderImpl::OnNetworkLocationReceived(
    mojom::NetworkLocationResponsePtr response) {
  CHECK(OnGeolocationThread());
  if (!diagnostics_enabled_) {
    return;
  }
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GeolocationProviderImpl::NotifyNetworkLocationReceived,
                     base::Unretained(this), std::move(response)));
}

void GeolocationProviderImpl::OnInternalsObserverDisconnected(
    mojo::RemoteSetElementId element_id) {
  CHECK(main_task_runner_->BelongsToCurrentThread());
  if (IsRunning() && internals_observers_.empty()) {
    // Disable diagnostics when the last observer has disconnected.
    // Using `base::Unretained` is safe here because `task_runner()` is
    // bound to this `GeolocationProviderImpl`.
    task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &GeolocationProviderImpl::DisableDiagnosticsOnGeolocationThread,
            base::Unretained(this)));
  }
}

void GeolocationProviderImpl::StopProviders() {
  DCHECK(OnGeolocationThread());
  DCHECK(location_provider_manager_);
  GEOLOCATION_LOG(DEBUG) << "Stop provider.";
  location_provider_manager_->StopProvider();
  OnInternalsUpdated();
}

void GeolocationProviderImpl::StartProviders(bool enable_high_accuracy,
                                             bool enable_diagnostics) {
  DCHECK(OnGeolocationThread());
  DCHECK(location_provider_manager_);
  GEOLOCATION_LOG(DEBUG) << "Start provider: high_accuracy="
                         << enable_high_accuracy;
  location_provider_manager_->StartProvider(enable_high_accuracy);
  if (enable_diagnostics) {
    // Enable diagnostics in the case where internals observers are added before
    // the provider is started.
    diagnostics_enabled_ = true;
  }
  OnInternalsUpdated();
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
  DCHECK(location_provider_manager_);
  location_provider_manager_->OnPermissionGranted();
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

void GeolocationProviderImpl::NotifyInternalsUpdated(
    mojom::GeolocationDiagnosticsPtr diagnostics) {
  CHECK(main_task_runner_->BelongsToCurrentThread());
  CHECK(diagnostics);
  for (auto& observer : internals_observers_) {
    observer->OnDiagnosticsChanged(diagnostics.Clone());
  }
}

void GeolocationProviderImpl::NotifyNetworkLocationRequested(
    std::vector<mojom::AccessPointDataPtr> request) {
  CHECK(main_task_runner_->BelongsToCurrentThread());
  for (auto& observer : internals_observers_) {
    observer->OnNetworkLocationRequested(mojo::Clone(request));
  }
}

void GeolocationProviderImpl::NotifyNetworkLocationReceived(
    mojom::NetworkLocationResponsePtr response) {
  CHECK(main_task_runner_->BelongsToCurrentThread());
  for (auto& observer : internals_observers_) {
    observer->OnNetworkLocationReceived(response.Clone());
  }
}

void GeolocationProviderImpl::Init() {
  DCHECK(OnGeolocationThread());

  if (location_provider_manager_) {
    return;
  }

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
  location_provider_manager_ = std::make_unique<LocationProviderManager>(
      g_custom_location_provider_callback.Get(),
      g_geolocation_system_permission_manager, std::move(url_loader_factory),
      g_api_key.Get(),
      std::make_unique<PositionCacheImpl>(
          base::DefaultTickClock::GetInstance()),
      base::BindRepeating(&GeolocationProviderImpl::OnInternalsUpdated,
                          base::Unretained(this)),
      base::BindRepeating(&GeolocationProviderImpl::OnNetworkLocationRequested,
                          base::Unretained(this)),
      base::BindRepeating(&GeolocationProviderImpl::OnNetworkLocationReceived,
                          base::Unretained(this)));
  location_provider_manager_->SetUpdateCallback(callback);
}

void GeolocationProviderImpl::CleanUp() {
  DCHECK(OnGeolocationThread());
  location_provider_manager_.reset();
}

void GeolocationProviderImpl::AddInternalsObserver(
    mojo::PendingRemote<mojom::GeolocationInternalsObserver> observer,
    AddInternalsObserverCallback callback) {
  CHECK(main_task_runner_->BelongsToCurrentThread());

  if (!base::FeatureList::IsEnabled(
          features::kGeolocationDiagnosticsObserver)) {
    std::move(callback).Run(nullptr);
    return;
  }
  internals_observers_.Add(std::move(observer));
  if (!location_provider_manager_) {
    std::move(callback).Run(nullptr);
    return;
  }

  // Using `base::Unretained` is safe here because |task_runner()| is
  // bound to `GeolocationProviderImpl`.
  task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &GeolocationProviderImpl::EnableAndGetDiagnosticsOnGeolocationThread,
          base::Unretained(this)),
      std::move(callback));
}

void GeolocationProviderImpl::SimulateInternalsUpdatedForTesting() {
  CHECK(main_task_runner_->BelongsToCurrentThread());
  CHECK(IsRunning());
  // Using `base::Unretained` is safe here because `task_runner()` is
  // bound to `GeolocationProviderImpl`.
  task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&GeolocationProviderImpl::OnInternalsUpdated,
                                base::Unretained(this)));
}

mojom::GeolocationDiagnosticsPtr
GeolocationProviderImpl::EnableAndGetDiagnosticsOnGeolocationThread() {
  CHECK(OnGeolocationThread());
  // Enable diagnostics in the case where an internals observer is added after
  // the provider is started.
  diagnostics_enabled_ = true;

  mojom::GeolocationDiagnosticsPtr result =
      mojom::GeolocationDiagnostics::New();
  location_provider_manager_->FillDiagnostics(*result);
  return result;
}

void GeolocationProviderImpl::DisableDiagnosticsOnGeolocationThread() {
  CHECK(OnGeolocationThread());
  // Disable diagnostics when the last internals observer has disconnected.
  diagnostics_enabled_ = false;
}

#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
void GeolocationProviderImpl::OnSystemPermissionUpdated(
    LocationSystemPermissionStatus new_status) {
  CHECK(main_task_runner_->BelongsToCurrentThread());
  if (new_status == LocationSystemPermissionStatus::kAllowed) {
    GEOLOCATION_LOG(DEBUG) << "New system permission state is kAllowed";
    if (!high_accuracy_callbacks_.empty() || !low_accuracy_callbacks_.empty()) {
      DoStartProvidersOnGeolocationThread();
    }
  } else if (new_status == LocationSystemPermissionStatus::kDenied) {
    GEOLOCATION_LOG(DEBUG) << "New system permission state is kDenied";
    NotifyClientsSystemPermissionDenied();
  } else {
    // System permission state reset to kUndetermined: Treat as if permission
    // was denied. This state transition is unusual in normal operation. It
    // likely indicates manual intervention for testing purposes. Since this
    // simulates a lack of permission, handle it as 'kDenied' for consistent
    // logic.
    GEOLOCATION_LOG(DEBUG) << "New system permission state is kUndetermined";
    NotifyClientsSystemPermissionDenied();
  }

  system_permission_status_ = new_status;
}

void GeolocationProviderImpl::NotifyClientsSystemPermissionDenied() {
  CHECK(main_task_runner_->BelongsToCurrentThread());
  auto error_result =
      mojom::GeopositionResult::NewError(mojom::GeopositionError::New(
          mojom::GeopositionErrorCode::kPermissionDenied,
          kSystemPermissionDeniedErrorMessage, ""));
  NotifyClients(std::move(error_result));
}
#endif

void GeolocationProviderImpl::DoStartProvidersOnGeolocationThread() {
  CHECK(main_task_runner_->BelongsToCurrentThread());
  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GeolocationProviderImpl::StartProviders,
                     base::Unretained(this), !high_accuracy_callbacks_.empty(),
                     !internals_observers_.empty()));
}

}  // namespace device
