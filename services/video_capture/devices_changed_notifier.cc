// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/devices_changed_notifier.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"

namespace video_capture {

DevicesChangedNotifier::DevicesChangedNotifier()
    : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  base::SystemMonitor::Get()->AddDevicesChangedObserver(this);
}

DevicesChangedNotifier::~DevicesChangedNotifier() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  base::SystemMonitor::Get()->RemoveDevicesChangedObserver(this);
}

void DevicesChangedNotifier::RegisterObserver(
    mojo::PendingRemote<mojom::DevicesChangedObserver> observer) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  observers_.Add(std::move(observer));
}

void DevicesChangedNotifier::OnDevicesChanged(
    base::SystemMonitor::DeviceType device_type) {
  if (device_type != base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE) {
    return;
  }

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DevicesChangedNotifier::UpdateObservers,
                                weak_factory_.GetWeakPtr()));
}

void DevicesChangedNotifier::UpdateObservers() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  for (const auto& observer : observers_) {
    observer->OnDevicesChanged();
  }
}

}  // namespace video_capture
