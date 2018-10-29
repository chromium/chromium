// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// libudev is used for monitoring device changes.

#include "media/device_monitors/device_monitor_udev.h"

#include <stddef.h>

#include <string>

#include "base/macros.h"
#include "base/system/system_monitor.h"
#include "device/udev_linux/udev.h"
#include "device/udev_linux/udev_linux.h"

namespace {

struct SubsystemMap {
  base::SystemMonitor::DeviceType device_type;
  const char* subsystem;
  const char* devtype;
};

const char kAudioSubsystem[] = "sound";
const char kVideoSubsystem[] = "video4linux";

// Add more subsystems here for monitoring.
const SubsystemMap kSubsystemMap[] = {
    {base::SystemMonitor::DEVTYPE_AUDIO, kAudioSubsystem, NULL},
    {base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE, kVideoSubsystem, NULL},
};

}  // namespace

namespace media {

DeviceMonitorLinux::DeviceMonitorLinux(
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
    : io_task_runner_(io_task_runner) {
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DeviceMonitorLinux::Initialize, base::Unretained(this)));
}

DeviceMonitorLinux::~DeviceMonitorLinux() = default;

void DeviceMonitorLinux::Initialize() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  // We want to be notified of IO message loop destruction to delete |udev_|.
  base::MessageLoopCurrent::Get()->AddDestructionObserver(this);

  std::vector<device::UdevLinux::UdevMonitorFilter> filters;
  for (const SubsystemMap& entry : kSubsystemMap) {
    filters.push_back(
        device::UdevLinux::UdevMonitorFilter(entry.subsystem, entry.devtype));
  }
  udev_.reset(new device::UdevLinux(
      filters, base::Bind(&DeviceMonitorLinux::OnDevicesChanged,
                          base::Unretained(this))));
}

void DeviceMonitorLinux::WillDestroyCurrentMessageLoop() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  udev_.reset();
}

void DeviceMonitorLinux::OnDevicesChanged(udev_device* device) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  DCHECK(device);

  base::SystemMonitor::DeviceType device_type =
      base::SystemMonitor::DEVTYPE_UNKNOWN;
  const std::string subsystem(device::udev_device_get_subsystem(device));
  for (const SubsystemMap& entry : kSubsystemMap) {
    if (subsystem == entry.subsystem) {
      device_type = entry.device_type;
      break;
    }
  }
  DCHECK_NE(device_type, base::SystemMonitor::DEVTYPE_UNKNOWN);

  base::SystemMonitor::Get()->ProcessDevicesChanged(device_type);
}

}  // namespace media
