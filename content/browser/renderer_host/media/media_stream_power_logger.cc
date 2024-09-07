// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_power_logger.h"

#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_source.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"

namespace content {

namespace {
void SendLogMessage(const std::string& message) {
  MediaStreamManager::SendMessageToNativeLog("MSPL::" + message);
}
}  // namespace

MediaStreamPowerLogger::MediaStreamPowerLogger() {
  auto* power_monitor = base::PowerMonitor::GetInstance();
  power_monitor->AddPowerSuspendObserver(this);
  power_monitor->AddPowerThermalObserver(this);
}

MediaStreamPowerLogger::~MediaStreamPowerLogger() {
  auto* power_monitor = base::PowerMonitor::GetInstance();
  power_monitor->RemovePowerSuspendObserver(this);
  power_monitor->RemovePowerThermalObserver(this);
}

void MediaStreamPowerLogger::OnSuspend() {
  SendLogMessage(base::StringPrintf("OnSuspend([this=%p])", this));
}

void MediaStreamPowerLogger::OnResume() {
  SendLogMessage(base::StringPrintf("OnResume([this=%p])", this));
}

void MediaStreamPowerLogger::OnThermalStateChange(
    base::PowerThermalObserver::DeviceThermalState new_state) {
  const char* state_name =
      base::PowerMonitorSource::DeviceThermalStateToString(new_state);
  SendLogMessage(base::StringPrintf(
      "OnThermalStateChange({this=%p}, {new_state=%s})", this, state_name));
}

void MediaStreamPowerLogger::OnSpeedLimitChange(int new_limit) {
  SendLogMessage(base::StringPrintf(
      "OnSpeedLimitChange({this=%p}, {new_limit=%d})", this, new_limit));
}

}  // namespace content
