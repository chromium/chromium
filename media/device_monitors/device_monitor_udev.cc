// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// libudev is used for monitoring device changes.

#include "media/device_monitors/device_monitor_udev.h"

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/sequence_checker.h"
#include "base/system/system_monitor.h"
#include "base/task/thread_pool.h"
#include "device/udev_linux/udev.h"
#include "device/udev_linux/udev_watcher.h"

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
    {base::SystemMonitor::DEVTYPE_AUDIO, kAudioSubsystem, ""},
    {base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE, kVideoSubsystem, ""},
};

}  // namespace

namespace media {

// Wraps a device::UdevWatcher with an API that makes it easier to use from
// DeviceMonitorLinux. Since it is essentially a wrapper around blocking udev
// calls, Initialize() must be called from a task runner that can block.
class DeviceMonitorLinux::BlockingTaskRunnerHelper
    : public device::UdevWatcher::Observer {
 public:
  BlockingTaskRunnerHelper();

  BlockingTaskRunnerHelper(const BlockingTaskRunnerHelper&) = delete;
  BlockingTaskRunnerHelper& operator=(const BlockingTaskRunnerHelper&) = delete;

  ~BlockingTaskRunnerHelper() override = default;

  void Initialize();

 private:
  void OnDevicesChanged(device::ScopedUdevDevicePtr device);

  // device::UdevWatcher::Observer overrides
  void OnDeviceAdded(device::ScopedUdevDevicePtr device) override;
  void OnDeviceRemoved(device::ScopedUdevDevicePtr device) override;
  void OnDeviceChanged(device::ScopedUdevDevicePtr device) override;

  std::unique_ptr<device::UdevWatcher> udev_watcher_;

  SEQUENCE_CHECKER(sequence_checker_);
};

DeviceMonitorLinux::BlockingTaskRunnerHelper::BlockingTaskRunnerHelper() {
  // Detaches from the sequence on which this object was created. It will be
  // bound to its owning sequence when Initialize() is called.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void DeviceMonitorLinux::BlockingTaskRunnerHelper::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<device::UdevWatcher::Filter> filters;
  for (const auto& [device_type, subsys, devtype] : kSubsystemMap) {
    filters.emplace_back(subsys, devtype);
  }
  udev_watcher_ = device::UdevWatcher::StartWatching(this, filters);
}

void DeviceMonitorLinux::BlockingTaskRunnerHelper::OnDeviceAdded(
    device::ScopedUdevDevicePtr device) {
  OnDevicesChanged(std::move(device));
}

void DeviceMonitorLinux::BlockingTaskRunnerHelper::OnDeviceRemoved(
    device::ScopedUdevDevicePtr device) {
  OnDevicesChanged(std::move(device));
}

void DeviceMonitorLinux::BlockingTaskRunnerHelper::OnDeviceChanged(
    device::ScopedUdevDevicePtr device) {
  OnDevicesChanged(std::move(device));
}

void DeviceMonitorLinux::BlockingTaskRunnerHelper::OnDevicesChanged(
    device::ScopedUdevDevicePtr device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::SystemMonitor::DeviceType type = base::SystemMonitor::DEVTYPE_UNKNOWN;
  const std::string subsystem(device::udev_device_get_subsystem(device.get()));
  for (const auto& [device_type, subsys, devtype] : kSubsystemMap) {
    if (subsystem == subsys) {
      type = device_type;
      break;
    }
  }
  DCHECK_NE(type, base::SystemMonitor::DEVTYPE_UNKNOWN);

  // base::SystemMonitor takes care of notifying each observer in their own task
  // runner via base::ObserverListThreadSafe.
  base::SystemMonitor::Get()->ProcessDevicesChanged(type);
}

DeviceMonitorLinux::DeviceMonitorLinux()
    : blocking_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      blocking_task_helper_(new BlockingTaskRunnerHelper,
                            base::OnTaskRunnerDeleter(blocking_task_runner_)) {
  // Unretained() is safe because the deletion of |blocking_task_helper_|
  // is scheduled on |blocking_task_runner_| when DeviceMonitorLinux is
  // deleted.
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DeviceMonitorLinux::BlockingTaskRunnerHelper::Initialize,
                     base::Unretained(blocking_task_helper_.get())));
}

DeviceMonitorLinux::~DeviceMonitorLinux() = default;

}  // namespace media
