// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_DEVICE_NOTIFIER_H_
#define SERVICES_AUDIO_DEVICE_NOTIFIER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/system/system_monitor.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/audio/public/mojom/device_notifications.mojom.h"
#include "services/audio/traced_service_ref.h"

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
  ~DeviceNotifier() final;

  void Bind(mojom::DeviceNotifierRequest request, TracedServiceRef context_ref);

  // mojom::DeviceNotifier implementation.
  void RegisterListener(mojom::DeviceListenerPtr listener) final;

  // base::SystemMonitor::DevicesChangedObserver implementation;
  void OnDevicesChanged(base::SystemMonitor::DeviceType device_type) final;

 private:
  void UpdateListeners();
  void RemoveListener(int listener_id);

  int next_listener_id_ = 0;
  base::flat_map<int, mojom::DeviceListenerPtr> listeners_;
  mojo::BindingSet<mojom::DeviceNotifier, TracedServiceRef> bindings_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<DeviceNotifier> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DeviceNotifier);
};

}  // namespace audio

#endif  // SERVICES_AUDIO_DEVICE_NOTIFIER_H_
