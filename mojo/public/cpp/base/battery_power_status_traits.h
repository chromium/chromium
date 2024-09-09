// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_BATTERY_POWER_STATUS_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_BATTERY_POWER_STATUS_TRAITS_H_

#include "base/component_export.h"
#include "base/power_monitor/power_observer.h"
#include "mojo/public/mojom/base/battery_power_status.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MOJO_BASE_TRAITS)
    EnumTraits<mojo_base::mojom::BatteryPowerStatus,
               base::PowerStateObserver::BatteryPowerStatus> {
  static mojo_base::mojom::BatteryPowerStatus ToMojom(
      base::PowerStateObserver::BatteryPowerStatus battery_power_status);
  static bool FromMojom(mojo_base::mojom::BatteryPowerStatus input,
                        base::PowerStateObserver::BatteryPowerStatus* out);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_BATTERY_POWER_STATUS_TRAITS_H_
