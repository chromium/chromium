// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_GEOLOCATION_GEO_NOTIFIER_BLINK_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_GEOLOCATION_GEO_NOTIFIER_BLINK_H_

#include "base/types/expected.h"
#include "third_party/blink/renderer/core/geolocation/geo_notifier.h"

namespace blink {

class Geolocation;

// GeoNotifierBlink is used for retrieving geolocation position from Blink. It
// is different from GeoNotifierV8 in that GeoNotifierBlink takes a
// base::RepeatingCallback instead of V8 callbacks.
class GeoNotifierBlink final : public GarbageCollected<GeoNotifierBlink>,
                               public GeoNotifier {
 public:
  GeoNotifierBlink(
      Geolocation*,
      const PositionOptions*,
      base::RepeatingCallback<
          void(base::expected<Geoposition*, GeolocationPositionError*>)>);

  ~GeoNotifierBlink() override = default;
  void Trace(Visitor*) const override;

 private:
  // GeoNotifier:
  void RunCallback(Geoposition*, GeolocationPositionError*) override;

  base::RepeatingCallback<void(
      base::expected<Geoposition*, GeolocationPositionError*>)>
      geoposition_callback_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_GEOLOCATION_GEO_NOTIFIER_BLINK_H_
