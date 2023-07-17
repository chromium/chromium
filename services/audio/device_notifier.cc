// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/device_notifier.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"

namespace audio {

DeviceNotifier::DeviceNotifier()
    : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  base::SystemMonitor::Get()->AddDevicesChangedObserver(this);
}

DeviceNotifier::~DeviceNotifier() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  base::SystemMonitor::Get()->RemoveDevicesChangedObserver(this);
}

void DeviceNotifier::Bind(
    mojo::PendingReceiver<mojom::DeviceNotifier> receiver) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  receivers_.Add(this, std::move(receiver));
}

void DeviceNotifier::RegisterListener(
    mojo::PendingRemote<mojom::DeviceListener> listener) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  const auto& id = listeners_.Add(std::move(listener));
  TRACE_EVENT1("audio", "audio::DeviceNotifier::RegisterListener", "id",
               id.value());
}

void DeviceNotifier::OnDevicesChanged(
    base::SystemMonitor::DeviceType device_type) {
  if (device_type != base::SystemMonitor::DEVTYPE_AUDIO)
    return;

  TRACE_EVENT0("audio", "audio::DeviceNotifier::OnDevicesChanged");
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&DeviceNotifier::UpdateListeners,
                                        weak_factory_.GetWeakPtr()));
}

void DeviceNotifier::UpdateListeners() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("audio", "audio::DeviceNotifier::UpdateListeners");

  for (const auto& listener : listeners_)
    listener->DevicesChanged();
}

}  // namespace audio
