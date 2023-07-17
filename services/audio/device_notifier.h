// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_DEVICE_NOTIFIER_H_
#define SERVICES_AUDIO_DEVICE_NOTIFIER_H_

#include "base/system/system_monitor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/audio/public/mojom/device_notifications.mojom.h"

namespace base {
class SequencedTaskRunner;
}

namespace audio {

// This class publishes notifications about changes in the audio devices
// to registered listeners.
class DeviceNotifier final : public base::SystemMonitor::DevicesChangedObserver,
                             public mojom::DeviceNotifier {
 public:
  DeviceNotifier();

  DeviceNotifier(const DeviceNotifier&) = delete;
  DeviceNotifier& operator=(const DeviceNotifier&) = delete;

  ~DeviceNotifier() final;

  void Bind(mojo::PendingReceiver<mojom::DeviceNotifier> receiver);

  // mojom::DeviceNotifier implementation.
  void RegisterListener(
      mojo::PendingRemote<mojom::DeviceListener> listener) final;

  // base::SystemMonitor::DevicesChangedObserver implementation;
  void OnDevicesChanged(base::SystemMonitor::DeviceType device_type) final;

 private:
  void UpdateListeners();

  mojo::RemoteSet<mojom::DeviceListener> listeners_;
  mojo::ReceiverSet<mojom::DeviceNotifier> receivers_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<DeviceNotifier> weak_factory_{this};
};

}  // namespace audio

#endif  // SERVICES_AUDIO_DEVICE_NOTIFIER_H_
