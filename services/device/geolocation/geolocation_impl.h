// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_GEOLOCATION_IMPL_H_
#define SERVICES_DEVICE_GEOLOCATION_GEOLOCATION_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/device/geolocation/geolocation_provider_impl.h"
#include "services/device/public/mojom/geolocation.mojom.h"
#include "services/device/public/mojom/geolocation_client_id.mojom.h"
#include "url/gurl.h"

namespace device {

class GeolocationProvider;
class GeolocationContext;

// Implements the Geolocation Mojo interface.
class GeolocationImpl : public mojom::Geolocation {
 public:
  // |context| must outlive this object.
  GeolocationImpl(mojo::PendingReceiver<mojom::Geolocation> receiver,
                  const GURL& requesting_url,
                  mojom::GeolocationClientId client_id,
                  GeolocationContext* context);

  GeolocationImpl(const GeolocationImpl&) = delete;
  GeolocationImpl& operator=(const GeolocationImpl&) = delete;

  ~GeolocationImpl() override;

  // Starts listening for updates.
  void StartListeningForUpdates();

  // Pauses and resumes sending updates to the client of this instance.
  void PauseUpdates();
  void ResumeUpdates();

  // Enables and disables geolocation override.
  void SetOverride(const mojom::GeopositionResult& result);
  void ClearOverride();

  // Invokes any pending position callback with a permission denied error.
  // Called by GeolocationContext when permission is lost.
  void OnPermissionRevoked();

  const GURL& url() { return url_; }

 private:
  // mojom::Geolocation:
  void SetHighAccuracy(bool high_accuracy) override;
  void QueryNextPosition(QueryNextPositionCallback callback) override;

  void OnConnectionError();

  void OnLocationUpdate(const mojom::GeopositionResult& result);
  void ReportCurrentPosition();

  // The binding between this object and the other end of the pipe.
  mojo::Receiver<mojom::Geolocation> receiver_;

  // The requesting URL.
  const GURL url_;

  const mojom::GeolocationClientId client_id_;

  // Owns this object.
  raw_ptr<GeolocationContext> context_;

  // Token that unsubscribes from GeolocationProvider updates when destroyed.
  base::CallbackListSubscription geolocation_subscription_;

  // The callback passed to QueryNextPosition.
  QueryNextPositionCallback position_callback_;

  // Set if SetOverride() has been called and ClearOverride() has not
  // subsequently been called, `nullptr` otherwise.
  mojom::GeopositionResultPtr position_override_;

  mojom::GeopositionResultPtr current_result_;

  // Whether this instance is currently observing location updates with high
  // accuracy.
  bool high_accuracy_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_GEOLOCATION_IMPL_H_
