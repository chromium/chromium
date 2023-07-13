// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_DEVICES_CHANGED_NOTIFIER_H_
#define SERVICES_VIDEO_CAPTURE_DEVICES_CHANGED_NOTIFIER_H_

#include "base/system/system_monitor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/video_capture/public/mojom/devices_changed_observer.mojom.h"

namespace base {
class SequencedTaskRunner;
}

namespace video_capture {

// This class publishes notifications about changes in the video_capture devices
// to registered observers.
class DevicesChangedNotifier final
    : public base::SystemMonitor::DevicesChangedObserver {
 public:
  DevicesChangedNotifier();

  DevicesChangedNotifier(const DevicesChangedNotifier&) = delete;
  DevicesChangedNotifier& operator=(const DevicesChangedNotifier&) = delete;

  ~DevicesChangedNotifier() final;

  void RegisterObserver(
      mojo::PendingRemote<mojom::DevicesChangedObserver> observer);

  // base::SystemMonitor::DevicesChangedObserver implementation;
  void OnDevicesChanged(base::SystemMonitor::DeviceType device_type) final;

 private:
  void UpdateObservers();

  mojo::RemoteSet<mojom::DevicesChangedObserver> observers_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<DevicesChangedNotifier> weak_factory_{this};
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_DEVICES_CHANGED_NOTIFIER_H_
