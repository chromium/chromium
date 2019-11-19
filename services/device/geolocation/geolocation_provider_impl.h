// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_GEOLOCATION_PROVIDER_IMPL_H_
#define SERVICES_DEVICE_GEOLOCATION_GEOLOCATION_PROVIDER_IMPL_H_

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/device/geolocation/geolocation_provider.h"
#include "services/device/public/cpp/geolocation/location_provider.h"
#include "services/device/public/mojom/geolocation_control.mojom.h"
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
    base::Callback<std::unique_ptr<LocationProvider>()>;

class GeolocationProviderImpl : public GeolocationProvider,
                                public mojom::GeolocationControl,
                                public base::Thread {
 public:
  // GeolocationProvider implementation:
  std::unique_ptr<GeolocationProvider::Subscription> AddLocationUpdateCallback(
      const LocationUpdateCallback& callback,
      bool enable_high_accuracy) override;
  bool HighAccuracyLocationInUse() override;
  void OverrideLocationForTesting(const mojom::Geoposition& position) override;

  // Callback from the LocationArbitrator. Public for testing.
  void OnLocationUpdate(const LocationProvider* provider,
                        const mojom::Geoposition& position);

  // Gets a pointer to the singleton instance of the location relayer, which
  // is in turn bound to the browser's global context objects. This must only be
  // called on the UI thread so that the GeolocationProviderImpl is always
  // instantiated on the same thread. Ownership is NOT returned.
  static GeolocationProviderImpl* GetInstance();

  // Optional: Provide global configuration to Geolocation. Should be called
  // before using Init() on the singleton GetInstance().
  // |url_loader_factory| : a factory to use for network geolocation requests.
  // |api_key| : a Google API key for network geolocation requests.
  // |custom_location_provider_getter| : a callback which returns a custom
  // location provider from embedder.
  // |use_gms_core_location_provider| : For android only, a flag indicates
  // whether using the GMS core location provider.
  static void SetGeolocationConfiguration(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& api_key,
      const CustomLocationProviderCallback& custom_location_provider_getter,
      bool use_gms_core_location_provider = false);

  void BindGeolocationControlReceiver(
      mojo::PendingReceiver<mojom::GeolocationControl> receiver);

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
  // arbitrator).
  void StartProviders(bool enable_high_accuracy);

  // Updates the providers on the geolocation thread, which must be running.
  void InformProvidersPermissionGranted();

  // Notifies all registered clients that a position update is available.
  void NotifyClients(const mojom::Geoposition& position);

  // Thread
  void Init() override;
  void CleanUp() override;

  base::CallbackList<void(const mojom::Geoposition&)> high_accuracy_callbacks_;
  base::CallbackList<void(const mojom::Geoposition&)> low_accuracy_callbacks_;

  bool user_did_opt_into_location_services_;
  mojom::Geoposition position_;

  // True only in testing, where we want to use a custom position.
  bool ignore_location_updates_;

  // Used to PostTask()s from the geolocation thread to caller thread.
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // Only to be used on the geolocation thread.
  std::unique_ptr<LocationProvider> arbitrator_;

  mojo::Receiver<mojom::GeolocationControl> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(GeolocationProviderImpl);
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_GEOLOCATION_PROVIDER_IMPL_H_
