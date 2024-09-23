// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_GEOLOCATION_CONTEXT_H_
#define SERVICES_DEVICE_GEOLOCATION_GEOLOCATION_CONTEXT_H_

#include <memory>
#include <vector>

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/public/mojom/geolocation.mojom.h"
#include "services/device/public/mojom/geolocation_client_id.mojom.h"
#include "services/device/public/mojom/geolocation_context.mojom.h"
#include "services/device/public/mojom/geoposition.mojom.h"
#include "url/origin.h"

namespace device {

class GeolocationImpl;

// Provides information to a set of GeolocationImpl instances that are
// associated with a given context. Notably, allows pausing and resuming
// geolocation on these instances.
class GeolocationContext : public mojom::GeolocationContext {
 public:
  GeolocationContext();

  GeolocationContext(const GeolocationContext&) = delete;
  GeolocationContext& operator=(const GeolocationContext&) = delete;

  ~GeolocationContext() override;

  // Creates GeolocationContext that is strongly bound to |receiver|.
  static void Create(mojo::PendingReceiver<mojom::GeolocationContext> receiver);

  // mojom::GeolocationContext implementation:
  void BindGeolocation(mojo::PendingReceiver<mojom::Geolocation> receiver,
                       const GURL& requesting_url,
                       mojom::GeolocationClientId client_id) override;
  void OnPermissionRevoked(const url::Origin& origin) override;

  void SetOverride(mojom::GeopositionResultPtr geoposition_result) override;
  void ClearOverride() override;

  // Called when a GeolocationImpl has a connection error. After this call, it
  // is no longer safe to access |impl|.
  void OnConnectionError(GeolocationImpl* impl);

 private:
  std::vector<std::unique_ptr<GeolocationImpl>> impls_;

  mojom::GeopositionResultPtr geoposition_override_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_GEOLOCATION_CONTEXT_H_
