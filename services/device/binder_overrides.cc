// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/binder_overrides.h"

#include "base/no_destructor.h"
#include "build/build_config.h"

namespace device {
namespace internal {

GeolocationContextBinder& GetGeolocationContextBinderOverride() {
  static base::NoDestructor<GeolocationContextBinder> binder;
  return *binder;
}

PressureManagerBinder& GetPressureManagerBinderOverride() {
  static base::NoDestructor<PressureManagerBinder> binder;
  return *binder;
}

TimeZoneMonitorBinder& GetTimeZoneMonitorBinderOverride() {
  static base::NoDestructor<TimeZoneMonitorBinder> binder;
  return *binder;
}

#if BUILDFLAG(IS_ANDROID)
NFCProviderBinder& GetNFCProviderBinderOverride() {
  static base::NoDestructor<NFCProviderBinder> binder;
  return *binder;
}
#endif

}  // namespace internal
}  // namespace device
