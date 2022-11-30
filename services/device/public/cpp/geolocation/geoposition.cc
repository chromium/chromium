// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/geolocation/geoposition.h"

namespace device {

bool ValidateGeoposition(const mojom::Geoposition& position) {
  return position.latitude >= -90. && position.latitude <= 90. &&
         position.longitude >= -180. && position.longitude <= 180. &&
         position.accuracy >= 0. && !position.timestamp.is_null();
}

}  // namespace device
