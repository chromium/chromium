// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_VR_DEVICE_BASE_H_
#define DEVICE_VR_VR_DEVICE_BASE_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "build/build_config.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/public/mojom/xr_device.mojom.h"
#include "device/vr/public/mojom/xr_session.mojom.h"
#include "device/vr/vr_device.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

struct CHROME_LUID;

namespace device {

// Represents one of the platform's VR devices. Owned by the respective
// VRDeviceProvider.
class COMPONENT_EXPORT(DEVICE_VR_BASE) VRDeviceBase : public mojom::XRRuntime {
 public:
  explicit VRDeviceBase(mojom::XRDeviceId id);

  VRDeviceBase(const VRDeviceBase&) = delete;
  VRDeviceBase& operator=(const VRDeviceBase&) = delete;

  ~VRDeviceBase() override;

  // XRRuntime implementation
  void ListenToDeviceChanges(
      mojo::PendingAssociatedRemote<mojom::XRRuntimeEventListener> listener)
      final;

  device::mojom::XRDeviceId GetId() const;
  device::mojom::XRDeviceDataPtr GetDeviceData() const;

  bool HasExclusiveSession();

  // Devices may be paused/resumed when focus changes by GVR delegate.
  virtual void PauseTracking();
  virtual void ResumeTracking();

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
  void OnVisibilityStateChanged(mojom::XRVisibilityState visibility_state);
  void SetArBlendModeSupported(bool is_ar_blend_mode_supported);
  void SetSupportedFeatures(
      const std::vector<mojom::XRSessionFeature>& features);
  void SetDeviceData(device::mojom::XRDeviceData&& device_data);
#if BUILDFLAG(IS_WIN)
  void SetLuid(const CHROME_LUID& luid);
#endif

 private:
  mojo::AssociatedRemote<mojom::XRRuntimeEventListener> listener_;

  bool presenting_ = false;

  device::mojom::XRDeviceId id_;

  device::mojom::XRDeviceData device_data_;

  mojo::Receiver<mojom::XRRuntime> runtime_receiver_{this};
};

}  // namespace device

#endif  // DEVICE_VR_VR_DEVICE_BASE_H_
