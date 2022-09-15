// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_POWER_LOGGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_POWER_LOGGER_H_

#include "base/power_monitor/power_observer.h"

namespace content {

// Injects system power event log entries into the WebRTC text logs, for
// debugging of unexpected call ending and performance changes eg when calls end
// due to a laptop's lid being closed.
class MediaStreamPowerLogger : public base::PowerSuspendObserver,
                               public base::PowerThermalObserver {
 public:
  MediaStreamPowerLogger();
  ~MediaStreamPowerLogger() override;

  // base::PowerSuspendObserver overrides.
  void OnSuspend() override;
  void OnResume() override;

  // base::PowerThermalObserver overrides.
  void OnThermalStateChange(
      base::PowerThermalObserver::DeviceThermalState new_state) override;
  void OnSpeedLimitChange(int new_limit) override;
};
}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_POWER_LOGGER_H_
