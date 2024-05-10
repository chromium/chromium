// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_VIBRATION_VIBRATION_MANAGER_IMPL_H_
#define SERVICES_DEVICE_VIBRATION_VIBRATION_MANAGER_IMPL_H_

#include <stdint.h>

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/vibration_manager.mojom.h"

namespace device {

class VibrationManagerImpl : public mojom::VibrationManager {
 public:
  static void Create(
      mojo::PendingReceiver<mojom::VibrationManager> receiver,
      mojo::PendingRemote<mojom::VibrationManagerListener> listener);

  // Sets the listener that listens for any Vibrate calls from this class. The
  // listener is currently set from the RenderFrameHostImpl.
  explicit VibrationManagerImpl(
      mojo::PendingRemote<mojom::VibrationManagerListener> listener);
  VibrationManagerImpl(const VibrationManagerImpl&) = delete;
  VibrationManagerImpl& operator=(const VibrationManagerImpl&) = delete;
  ~VibrationManagerImpl() override;

  // mojom::VibrationManager
  void Vibrate(int64_t milliseconds, VibrateCallback callback) override;
  void Cancel(CancelCallback callback) override;

  static int64_t milli_seconds_for_testing_;
  static bool cancelled_for_testing_;

 protected:
  virtual void PlatformVibrate(int64_t milliseconds);
  virtual void PlatformCancel();

  mojo::Remote<mojom::VibrationManagerListener> listener_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_VIBRATION_VIBRATION_MANAGER_IMPL_H_
