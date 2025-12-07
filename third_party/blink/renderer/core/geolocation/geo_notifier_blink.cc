// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/geolocation/geo_notifier_blink.h"

namespace blink {

GeoNotifierBlink::GeoNotifierBlink(
    Geolocation* geolocation,
    const PositionOptions* options,
    base::RepeatingCallback<
        void(base::expected<Geoposition*, GeolocationPositionError*>)> callback)
    : GeoNotifier(geolocation, options), geoposition_callback_(callback) {}

void GeoNotifierBlink::Trace(Visitor* visitor) const {
  GeoNotifier::Trace(visitor);
}

void GeoNotifierBlink::RunCallback(Geoposition* position,
                                   GeolocationPositionError* error) {
  if (position) {
    geoposition_callback_.Run(position);
  } else if (error) {
    geoposition_callback_.Run(base::unexpected(error));
  }
}

}  // namespace blink
