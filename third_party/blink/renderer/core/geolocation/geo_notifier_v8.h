// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_GEOLOCATION_GEO_NOTIFIER_V8_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_GEOLOCATION_GEO_NOTIFIER_V8_H_

#include "third_party/blink/renderer/core/geolocation/geo_notifier.h"

namespace blink {

class Geolocation;

// GeoNotifierV8 is used for retrieving geolocation position from V8.
class GeoNotifierV8 final : public GarbageCollected<GeoNotifierV8>,
                            public GeoNotifier {
 public:
  GeoNotifierV8(Geolocation*,
                const PositionOptions*,
                V8PositionCallback*,
                V8PositionErrorCallback*);

  void Trace(Visitor*) const override;

 private:
  // GeoNotifier:
  void RunCallback(Geoposition*, GeolocationPositionError*) override;
  Member<V8PositionCallback> success_callback_;
  Member<V8PositionErrorCallback> error_callback_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_GEOLOCATION_GEO_NOTIFIER_V8_H_
