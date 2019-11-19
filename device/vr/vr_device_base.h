// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_VR_DEVICE_BASE_H_
#define DEVICE_VR_VR_DEVICE_BASE_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_export.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/display/display.h"

namespace device {

// Represents one of the platform's VR devices. Owned by the respective
// VRDeviceProvider.
// TODO(mthiesse, crbug.com/769373): Remove DEVICE_VR_EXPORT.
class DEVICE_VR_EXPORT VRDeviceBase : public mojom::XRRuntime {
 public:
  explicit VRDeviceBase(mojom::XRDeviceId id);
  ~VRDeviceBase() override;

  // XRRuntime implementation
  void ListenToDeviceChanges(
      mojo::PendingAssociatedRemote<mojom::XRRuntimeEventListener> listener,
      mojom::XRRuntime::ListenToDeviceChangesCallback callback) final;
  void SetInlinePosesEnabled(bool enable) override;
  void ShutdownSession(mojom::XRRuntime::ShutdownSessionCallback) override;

  virtual void RequestHitTest(
      mojom::XRRayPtr ray,
      mojom::XREnvironmentIntegrationProvider::RequestHitTestCallback callback);
  device::mojom::XRDeviceId GetId() const;

  bool HasExclusiveSession();

  // Devices may be paused/resumed when focus changes by GVR delegate.
  virtual void PauseTracking();
  virtual void ResumeTracking();

  mojom::VRDisplayInfoPtr GetVRDisplayInfo();

  // Used by providers to bind devices.
  mojo::PendingRemote<mojom::XRRuntime> BindXRRuntime();

  // TODO(mthiesse): The browser should handle browser-side exiting of
  // presentation before device/ is even aware presentation is being exited.
  // Then the browser should call StopSession() on Device, which does device/
  // exiting of presentation before notifying displays. This is currently messy
  // because browser-side notions of presentation are mostly Android-specific.
  virtual void OnExitPresent();

 protected:
  // Devices tell VRDeviceBase when they start presenting.  It will be paired
  // with an OnExitPresent when the device stops presenting.
  void OnStartPresenting();
  bool IsPresenting() { return presenting_; }  // Exposed for test.
  void SetVRDisplayInfo(mojom::VRDisplayInfoPtr display_info);
  void OnVisibilityStateChanged(mojom::XRVisibilityState visibility_state);

  mojom::VRDisplayInfoPtr display_info_;

  bool inline_poses_enabled_ = true;

 private:

  mojo::AssociatedRemote<mojom::XRRuntimeEventListener> listener_;

  bool presenting_ = false;

  device::mojom::XRDeviceId id_;

  mojo::Receiver<mojom::XRRuntime> runtime_receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(VRDeviceBase);
};

}  // namespace device

#endif  // DEVICE_VR_VR_DEVICE_BASE_H_
