// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_GEOPOSITION_H_
#define SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_GEOPOSITION_H_

#include "base/component_export.h"
#include "services/device/public/mojom/geoposition.mojom.h"

namespace device {

bool COMPONENT_EXPORT(GEOLOCATION)
    ValidateGeoposition(const mojom::Geoposition& position);

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_GEOLOCATION_GEOPOSITION_H_
