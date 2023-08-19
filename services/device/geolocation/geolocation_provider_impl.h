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
#include "services/device/public/cpp/geolocation/location_provider.h"
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

class GeolocationManager;

// Callback that returns the embedder's custom location provider. This callback
// is provided to the Device Service by its embedder.
using CustomLocationProviderCallback =
    base::RepeatingCallback<std::unique_ptr<LocationProvider>()>;

class GeolocationProviderImpl : public GeolocationProvider,
                                public mojom::GeolocationControl,
                                public mojom::GeolocationInternals,
                                public base::Thread {
 public:
  // GeolocationProvider implementation:
  base::CallbackListSubscription AddLocationUpdateCallback(
      const LocationUpdateCallback& callback,
      bool enable_high_accuracy) override;
  bool HighAccuracyLocationInUse() override;
  void OverrideLocationForTesting(mojom::GeopositionResultPtr result) override;

  // Callback from the LocationArbitrator. Public for testing.
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
  // |geolocation_manager| : An object that holds the macOS CLLocationManager
  // object in order to avoid multiple initializations. Should be a nullptr
  // on all other platforms.
  // |use_gms_core_location_provider| : For android only, a flag indicates
  // whether using the GMS core location provider.
  static void SetGeolocationConfiguration(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& api_key,
      const CustomLocationProviderCallback& custom_location_provider_getter,
      GeolocationManager* geolocation_manager,
      bool use_gms_core_location_provider);

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
  void SetArbitratorForTesting(std::unique_ptr<LocationProvider> arbitrator);

  // mojom::GeolocationInternals implementation:
  void AddInternalsObserver(
      mojo::PendingRemote<mojom::GeolocationInternalsObserver> observer,
      AddInternalsObserverCallback callback) override;

  // Calls OnInternalsUpdated on the geolocation thread to simulate updated
  // diagnostics in tests.
  void SimulateInternalsUpdatedForTesting();

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
  // arbitrator). If `enable_diagnostics` is true, also enables geolocation
  // diagnostics.
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

  // Called on the main thread when an internals observer disconnects.
  void OnInternalsObserverDisconnected(mojo::RemoteSetElementId element_id);

  // Called on the geolocation thread when new diagnostic data is available.
  void OnInternalsUpdated();

  // Enables geolocation diagnostics and returns the most recent diagnostic
  // data. Must be called on the geolocation thread.
  mojom::GeolocationDiagnosticsPtr EnableAndGetDiagnosticsOnGeolocationThread();

  // Disables geolocation diagnostics. Must be called on the geolocation thread.
  void DisableDiagnosticsOnGeolocationThread();

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
  std::unique_ptr<LocationProvider> arbitrator_;

  mojo::Receiver<mojom::GeolocationControl> control_receiver_{this};

  mojo::ReceiverSet<mojom::GeolocationInternals> internals_receivers_;
  mojo::RemoteSet<mojom::GeolocationInternalsObserver> internals_observers_;

  // If enabled, calling OnInternalsUpdated collects diagnostic information and
  // sends it to `internals_observers_`.
  bool diagnostics_enabled_ = false;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_GEOLOCATION_PROVIDER_IMPL_H_
