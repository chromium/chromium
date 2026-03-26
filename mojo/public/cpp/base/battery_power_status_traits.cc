// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/battery_power_status_traits.h"

#include "base/notreached.h"
#include "base/power_monitor/power_observer.h"

namespace mojo {

// static
mojo_base::mojom::BatteryPowerStatus
EnumTraits<mojo_base::mojom::BatteryPowerStatus,
           base::PowerStateObserver::BatteryPowerStatus>::
    ToMojom(base::PowerStateObserver::BatteryPowerStatus battery_power_status) {
  switch (battery_power_status) {
    case base::PowerStateObserver::BatteryPowerStatus::kBatteryPower:
      return mojo_base::mojom::BatteryPowerStatus::kBatteryPower;
    case base::PowerStateObserver::BatteryPowerStatus::kExternalPower:
      return mojo_base::mojom::BatteryPowerStatus::kExternalPower;
    case base::PowerStateObserver::BatteryPowerStatus::kUnknown:
      return mojo_base::mojom::BatteryPowerStatus::kUnknown;
  }
  NOTREACHED();
}

// static
base::PowerStateObserver::BatteryPowerStatus
EnumTraits<mojo_base::mojom::BatteryPowerStatus,
           base::PowerStateObserver::BatteryPowerStatus>::
    FromMojom(mojo_base::mojom::BatteryPowerStatus input) {
  switch (input) {
    case mojo_base::mojom::BatteryPowerStatus::kBatteryPower:
      return base::PowerStateObserver::BatteryPowerStatus::kBatteryPower;
    case mojo_base::mojom::BatteryPowerStatus::kExternalPower:
      return base::PowerStateObserver::BatteryPowerStatus::kExternalPower;
    case mojo_base::mojom::BatteryPowerStatus::kUnknown:
      return base::PowerStateObserver::BatteryPowerStatus::kUnknown;
  }
  NOTREACHED();
}

}  // namespace mojo
