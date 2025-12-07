// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/geolocation/geo_notifier_v8.h"

namespace blink {

GeoNotifierV8::GeoNotifierV8(Geolocation* geolocation,
                             const PositionOptions* options,
                             V8PositionCallback* success_callback,
                             V8PositionErrorCallback* error_callback)
    : GeoNotifier(geolocation, options),
      success_callback_(success_callback),
      error_callback_(error_callback) {}

void GeoNotifierV8::RunCallback(Geoposition* position,
                                GeolocationPositionError* error) {
  if (position) {
    success_callback_->InvokeAndReportException(nullptr, position);
  } else if (error && error_callback_) {
    error_callback_->InvokeAndReportException(nullptr, error);
  }
}

void GeoNotifierV8::Trace(Visitor* visitor) const {
  visitor->Trace(success_callback_);
  visitor->Trace(error_callback_);
  GeoNotifier::Trace(visitor);
}

}  // namespace blink
