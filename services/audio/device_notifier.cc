// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/device_notifier.h"

#include <utility>

#include "base/bind.h"
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
  TRACE_EVENT1("audio", "audio::DeviceNotifier::RegisterListener", "id",
               next_listener_id_);

  int listener_id = next_listener_id_++;
  auto& new_listener = listeners_[listener_id];
  new_listener.Bind(std::move(listener));
  new_listener.set_disconnect_handler(
      base::BindOnce(&DeviceNotifier::RemoveListener,
                     weak_factory_.GetWeakPtr(), listener_id));
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
    listener.second->DevicesChanged();
}

void DeviceNotifier::RemoveListener(int listener_id) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT1("audio", "audio::DeviceNotifier::RemoveListener", "id",
               listener_id);

  listeners_.erase(listener_id);
}

}  // namespace audio
