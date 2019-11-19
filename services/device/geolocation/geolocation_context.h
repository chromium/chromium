// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_GEOLOCATION_CONTEXT_H_
#define SERVICES_DEVICE_GEOLOCATION_GEOLOCATION_CONTEXT_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/public/mojom/geolocation.mojom.h"
#include "services/device/public/mojom/geolocation_context.mojom.h"
#include "services/device/public/mojom/geoposition.mojom.h"

namespace device {

class GeolocationImpl;

// Provides information to a set of GeolocationImpl instances that are
// associated with a given context. Notably, allows pausing and resuming
// geolocation on these instances.
class GeolocationContext : public mojom::GeolocationContext {
 public:
  GeolocationContext();
  ~GeolocationContext() override;

  // Creates GeolocationContext that is strongly bound to |receiver|.
  static void Create(mojo::PendingReceiver<mojom::GeolocationContext> receiver);

  // mojom::GeolocationContext implementation:
  void BindGeolocation(
      mojo::PendingReceiver<mojom::Geolocation> receiver) override;
  void SetOverride(mojom::GeopositionPtr geoposition) override;
  void ClearOverride() override;

  // Called when a GeolocationImpl has a connection error. After this call, it
  // is no longer safe to access |impl|.
  void OnConnectionError(GeolocationImpl* impl);

 private:
  std::vector<std::unique_ptr<GeolocationImpl>> impls_;

  mojom::GeopositionPtr geoposition_override_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationContext);
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_GEOLOCATION_CONTEXT_H_
