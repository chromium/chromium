// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class is used to detect device change and notify base::SystemMonitor
// on Linux.

#ifndef MEDIA_DEVICE_MONITORS_DEVICE_MONITOR_UDEV_H_
#define MEDIA_DEVICE_MONITORS_DEVICE_MONITOR_UDEV_H_

#include <memory>

#include "base/task/sequenced_task_runner.h"
#include "media/base/media_export.h"

namespace media {

class MEDIA_EXPORT DeviceMonitorLinux {
 public:
  DeviceMonitorLinux();

  DeviceMonitorLinux(const DeviceMonitorLinux&) = delete;
  DeviceMonitorLinux& operator=(const DeviceMonitorLinux&) = delete;

  ~DeviceMonitorLinux();

  // TODO(mcasas): Consider adding a StartMonitoring() method like
  // DeviceMonitorMac to reduce startup impact time.

 private:
  class BlockingTaskRunnerHelper;

  // Task for running udev code that can potentially block.
  const scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  // Holds the BlockingTaskRunnerHelper which runs tasks on
  // |blocking_task_runner_|.
  std::unique_ptr<BlockingTaskRunnerHelper, base::OnTaskRunnerDeleter>
      blocking_task_helper_;
};

}  // namespace media

#endif  // MEDIA_DEVICE_MONITORS_DEVICE_MONITOR_UDEV_H_
