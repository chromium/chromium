// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_TIME_ZONE_MONITOR_TIME_ZONE_MONITOR_LACROS_H_
#define SERVICES_DEVICE_TIME_ZONE_MONITOR_TIME_ZONE_MONITOR_LACROS_H_

#include <string>

#include "build/chromeos_buildflags.h"
#include "chromeos/crosapi/mojom/timezone.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/device/time_zone_monitor/time_zone_monitor.h"

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
#error "This file should be used only for lacros builds."
#endif

namespace device {

// TimeZoneMonitor implementation for Lacros based on crosapi.
// Unlike other per-platform implementation, Lacros needs a header file
// to fallback into TimeZoneMonitorLinux if ash-chrome is older.
// See also time_zone_monitor_linux.cc's TimeZoneMonitor::Create().
class TimeZoneMonitorLacros : public TimeZoneMonitor,
                              public crosapi::mojom::TimeZoneObserver {
 public:
  TimeZoneMonitorLacros();
  TimeZoneMonitorLacros(const TimeZoneMonitorLacros&) = delete;
  TimeZoneMonitorLacros& operator=(const TimeZoneMonitorLacros&) = delete;
  ~TimeZoneMonitorLacros() override;

  // crosapi::mojom::TimeZoneObserver
  void OnTimeZoneChanged(const std::u16string& time_zone_id) override;

 private:
  mojo::Receiver<crosapi::mojom::TimeZoneObserver> receiver_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_TIME_ZONE_MONITOR_TIME_ZONE_MONITOR_LACROS_H_
