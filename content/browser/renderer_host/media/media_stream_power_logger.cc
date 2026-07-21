// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_power_logger.h"

#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_source.h"
#include "base/strings/stringprintf.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"

namespace content {

namespace {
void SendLogMessage(const std::string& message) {
  MediaStreamManager::SendMessageToNativeLog("MSPL::" + message);
}
}  // namespace

MediaStreamPowerLogger::MediaStreamPowerLogger()
    : id_(base::UnguessableToken::Create()) {
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
  SendLogMessage(
      base::StringPrintf("OnSuspend([id=%s])", id_.ToString().c_str()));
}

void MediaStreamPowerLogger::OnResume() {
  SendLogMessage(
      base::StringPrintf("OnResume([id=%s])", id_.ToString().c_str()));
}

void MediaStreamPowerLogger::OnThermalStateChange(
    base::PowerThermalObserver::DeviceThermalState new_state) {
  const char* state_name =
      base::PowerMonitorSource::DeviceThermalStateToString(new_state);
  SendLogMessage(
      base::StringPrintf("OnThermalStateChange({id=%s}, {new_state=%s})",
                         id_.ToString().c_str(), state_name));
}

void MediaStreamPowerLogger::OnSpeedLimitChange(int new_limit) {
  SendLogMessage(
      base::StringPrintf("OnSpeedLimitChange({id=%s}, {new_limit=%d})",
                         id_.ToString().c_str(), new_limit));
}

}  // namespace content
