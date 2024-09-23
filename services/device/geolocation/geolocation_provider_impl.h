// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_GEOLOCATION_PROVIDER_IMPL_H_
#define SERVICES_DEVICE_GEOLOCATION_GEOLOCATION_PROVIDER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/callback_forward.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/device/geolocation/geolocation_provider.h"
#include "services/device/public/cpp/geolocation/geolocation_system_permission_manager.h"
#include "services/device/public/cpp/geolocation/location_provider.h"
#include "services/device/public/cpp/geolocation/location_system_permission_status.h"
#include "services/device/public/mojom/geolocation_control.mojom.h"
#include "services/device/public/mojom/geolocation_internals.mojom.h"
#include "services/device/public/mojom/geoposition.mojom.h"

namespace base {
template <typename Type>
struct DefaultSingletonTraits;
class SingleThreadTaskRunner;
}  // namespace base

namespace network {
class SharedURLLoaderFactory;
}

namespace device {

// Callback that returns the embedder's custom location provider. This callback
// is provided to the Device Service by its embedder.
using CustomLocationProviderCallback =
    base::RepeatingCallback<std::unique_ptr<LocationProvider>()>;

// This class implements the GeolocationProvider interface, which is the main
// API of the geolocation subsystem. Clients subscribe for location updates
// with AddLocationUpdateCallback and cancel their subscription by destroying
// the returned subscription object.
//
// It also implements GeolocationSystemPermissionManager::PermissionObserver on
// supported platforms. Monitors system permission changes to manage the
// LocationProviderManager and report permission errors to clients.
//
// THREADING
//
// GeolocationProviderImpl is constructed on the main thread and its public
// methods (except OnLocationUpdate) must also be called on the main thread.
//
// GeolocationProviderImpl extends base::Thread. This thread, called the
// "geolocation thread", runs background tasks when acquiring new position
// estimates.
//
// GeolocationProviderImpl owns one or more LocationProvider implementations
// which generate new position estimates. LocationProviders must only run on
// the geolocation thread. Providers report new position estimates by calling
// OnLocationUpdate on the geolocation thread.
class GeolocationProviderImpl
    : public GeolocationProvider,
      public mojom::GeolocationControl,
      public mojom::GeolocationInternals,
      public base::Thread
#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
    ,
      public GeolocationSystemPermissionManager::PermissionObserver
#endif
{
 public:
  // GeolocationProvider implementation:
  base::CallbackListSubscription AddLocationUpdateCallback(
      const LocationUpdateCallback& callback,
      bool enable_high_accuracy) override;
  bool HighAccuracyLocationInUse() override;
  void OverrideLocationForTesting(mojom::GeopositionResultPtr result) override;

  // Callback from the LocationProviderManager. Public for testing.
  void OnLocationUpdate(const LocationProvider* provider,
                        mojom::GeopositionResultPtr result);

  // Gets a pointer to the singleton instance of the location relayer, which
  // is in turn bound to the browser's global context objects. This must only be
  // called on the UI thread so that the GeolocationProviderImpl is always
  // instantiated on the same thread. Ownership is NOT returned.
  static GeolocationProviderImpl* GetInstance();

  GeolocationProviderImpl(const GeolocationProviderImpl&) = delete;
  GeolocationProviderImpl& operator=(const GeolocationProviderImpl&) = delete;

  // Optional: Provide global configuration to Geolocation. Should be called
  // before using Init() on the singleton GetInstance().
  // |url_loader_factory| : a factory to use for network geolocation requests.
  // |api_key| : a Google API key for network geolocation requests.
  // |custom_location_provider_getter| : a callback which returns a custom
  // location provider from embedder.
  // |geolocation_system_permission_manager| : An object that holds the macOS
  // CLLocationManager object in order to avoid multiple initializations. Should
  // be a nullptr on all other platforms. |use_gms_core_location_provider| : For
  // android only, a flag indicates whether using the GMS core location
  // provider.
  static void SetGeolocationConfiguration(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& api_key,
      const CustomLocationProviderCallback& custom_location_provider_getter,
      GeolocationSystemPermissionManager* geolocation_system_permission_manager,
      bool use_gms_core_location_provider);

  static void SetGeolocationSystemPermissionManagerForTesting(
      GeolocationSystemPermissionManager* instance_for_testing);

  void BindGeolocationControlReceiver(
      mojo::PendingReceiver<mojom::GeolocationControl> receiver);

  void BindGeolocationInternalsReceiver(
      mojo::PendingReceiver<mojom::GeolocationInternals> receiver);

  // mojom::GeolocationControl implementation:
  void UserDidOptIntoLocationServices() override;

  bool user_did_opt_into_location_services_for_testing() {
    return user_did_opt_into_location_services_;
  }

  void clear_user_did_opt_into_location_services_for_testing() {
    user_did_opt_into_location_services_ = false;
  }

  // Safe to call while there are no GeolocationProviderImpl clients
  // registered.
  void SetLocationProviderManagerForTesting(
      std::unique_ptr<LocationProvider> location_provider_manager);

  // mojom::GeolocationInternals implementation:
  void AddInternalsObserver(
      mojo::PendingRemote<mojom::GeolocationInternalsObserver> observer,
      AddInternalsObserverCallback callback) override;

  // Calls OnInternalsUpdated on the geolocation thread to simulate updated
  // diagnostics in tests.
  void SimulateInternalsUpdatedForTesting();

#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
  // GeolocationSystemPermissionManager::PermissionObserver implementation.
  void OnSystemPermissionUpdated(
      LocationSystemPermissionStatus new_status) override;
#endif

  static constexpr char kSystemPermissionDeniedErrorMessage[] =
      "User has not allowed access to system location.";

 private:
  friend struct base::DefaultSingletonTraits<GeolocationProviderImpl>;
  GeolocationProviderImpl();
  ~GeolocationProviderImpl() override;

  bool OnGeolocationThread() const;

  // Start and stop providers as needed when clients are added or removed.
  void OnClientsChanged();

  // Stops the providers when there are no more registered clients. Note that
  // once the Geolocation thread is started, it will stay alive (but sitting
  // idle without any pending messages).
  void StopProviders();

  // Starts the geolocation providers or updates their options (delegates to
  // location_provider_manager). If `enable_diagnostics` is true, also enables
  // geolocation diagnostics.
  void StartProviders(bool enable_high_accuracy, bool enable_diagnostics);

  // Updates the providers on the geolocation thread, which must be running.
  void InformProvidersPermissionGranted();

  // Notifies all registered clients that a position update is available.
  void NotifyClients(mojom::GeopositionResultPtr result);

  // Thread
  void Init() override;
  void CleanUp() override;

  // Notifies internals observers that new diagnostic data is available. Must be
  // called on the main thread.
  void NotifyInternalsUpdated(mojom::GeolocationDiagnosticsPtr diagnostics);

  // Notifies internals observers that a request was sent to the location
  // service.
  void NotifyNetworkLocationRequested(
      std::vector<mojom::AccessPointDataPtr> request);

  // Notifies internals observers that a response was received from the location
  // service.
  void NotifyNetworkLocationReceived(
      mojom::NetworkLocationResponsePtr response);

  // Called on the main thread when an internals observer disconnects.
  void OnInternalsObserverDisconnected(mojo::RemoteSetElementId element_id);

  // Called on the geolocation thread when new diagnostic data is available.
  void OnInternalsUpdated();

  // Called on the geolocation thread when a request is sent to the location
  // service. `request` contains the information about nearby access points
  // sent to the service.
  void OnNetworkLocationRequested(
      std::vector<mojom::AccessPointDataPtr> request);

  // Called on the geolocation thread when a response is received from the
  // location service. `response` is the received location estimate, or nullptr
  // if no location estimate was received.
  void OnNetworkLocationReceived(mojom::NetworkLocationResponsePtr response);

  // Enables geolocation diagnostics and returns the most recent diagnostic
  // data. Must be called on the geolocation thread.
  mojom::GeolocationDiagnosticsPtr EnableAndGetDiagnosticsOnGeolocationThread();

  // Disables geolocation diagnostics. Must be called on the geolocation thread.
  void DisableDiagnosticsOnGeolocationThread();

  // Called on main thread to post a task to start providers on geolocation
  // thread.
  void DoStartProvidersOnGeolocationThread();

#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
  // Called on main thread to notify clients when system permission is denied.
  void NotifyClientsSystemPermissionDenied();
#endif

  base::RepeatingCallbackList<void(const mojom::GeopositionResult&)>
      high_accuracy_callbacks_;
  base::RepeatingCallbackList<void(const mojom::GeopositionResult&)>
      low_accuracy_callbacks_;

  bool user_did_opt_into_location_services_ = false;
  mojom::GeopositionResultPtr result_;

  // True only in testing, where we want to use a custom position.
  bool ignore_location_updates_ = false;

  // Used to PostTask()s from the geolocation thread to caller thread.
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // Only to be used on the geolocation thread.
  std::unique_ptr<LocationProvider> location_provider_manager_;

  mojo::Receiver<mojom::GeolocationControl> control_receiver_{this};

  mojo::ReceiverSet<mojom::GeolocationInternals> internals_receivers_;
  mojo::RemoteSet<mojom::GeolocationInternalsObserver> internals_observers_;

  // If enabled, calling OnInternalsUpdated collects diagnostic information and
  // sends it to `internals_observers_`.
  bool diagnostics_enabled_ = false;

#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
  LocationSystemPermissionStatus system_permission_status_ =
      LocationSystemPermissionStatus::kNotDetermined;

  // On CrOS, GeolocationSystemPermissionManager may be destroyed before
  // GeolocationProviderImpl. Retaining `observers_` allows
  // GeolocationProviderImpl to safely unregister itself during its own
  // destruction.
  scoped_refptr<GeolocationSystemPermissionManager::PermissionObserverList>
      observers_;
#endif
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_GEOLOCATION_PROVIDER_IMPL_H_
