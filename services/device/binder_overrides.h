// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_BINDER_OVERRIDES_H_
#define SERVICES_DEVICE_BINDER_OVERRIDES_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/public/mojom/geolocation_context.mojom.h"
#include "services/device/public/mojom/pressure_manager.mojom.h"
#include "services/device/public/mojom/time_zone_monitor.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "services/device/public/mojom/nfc_provider.mojom.h"
#endif

namespace device {
namespace internal {

using GeolocationContextBinder = base::RepeatingCallback<void(
    mojo::PendingReceiver<device::mojom::GeolocationContext>)>;
COMPONENT_EXPORT(DEVICE_SERVICE_BINDER_OVERRIDES)
GeolocationContextBinder& GetGeolocationContextBinderOverride();

using PressureManagerBinder = base::RepeatingCallback<void(
    mojo::PendingReceiver<device::mojom::PressureManager>)>;
COMPONENT_EXPORT(DEVICE_SERVICE_BINDER_OVERRIDES)
PressureManagerBinder& GetPressureManagerBinderOverride();

using TimeZoneMonitorBinder = base::RepeatingCallback<void(
    mojo::PendingReceiver<device::mojom::TimeZoneMonitor>)>;
COMPONENT_EXPORT(DEVICE_SERVICE_BINDER_OVERRIDES)
TimeZoneMonitorBinder& GetTimeZoneMonitorBinderOverride();

#if BUILDFLAG(IS_ANDROID)
using NFCProviderBinder = base::RepeatingCallback<void(
    mojo::PendingReceiver<device::mojom::NFCProvider>)>;
COMPONENT_EXPORT(DEVICE_SERVICE_BINDER_OVERRIDES)
NFCProviderBinder& GetNFCProviderBinderOverride();
#endif

}  // namespace internal
}  // namespace device

#endif  // SERVICES_DEVICE_BINDER_OVERRIDES_H_
