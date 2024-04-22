// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ORIENTATION_ORIENTATION_DEVICE_H_
#define DEVICE_VR_ORIENTATION_ORIENTATION_DEVICE_H_

#include <memory>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/threading/simple_thread.h"
#include "build/build_config.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_device_base.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"
#include "ui/gfx/geometry/quaternion.h"

namespace device {

class SensorReadingSharedBufferReader;
class VROrientationSession;

// Use RELATIVE_ORIENTATION_QUATERNION rather than
// ABSOLUTE_ORIENTATION_QUATERNION because compass readings can be inacurate
// when used indoors, unless we're on Windows which doesn't support
// RELATIVE_ORIENTATION_QUATERNION.
// TODO(crbug.com/41323676) If RELATIVE_ORIENTATION_QUATERNION is ever
// implemented on Windows, use that instead.
static constexpr mojom::SensorType kOrientationSensorType =
#if BUILDFLAG(IS_WIN)
    mojom::SensorType::ABSOLUTE_ORIENTATION_QUATERNION;
#else
    mojom::SensorType::RELATIVE_ORIENTATION_QUATERNION;
#endif

// This class connects the orientation sensor events to the Web VR apis.
class COMPONENT_EXPORT(VR_ORIENTATION) VROrientationDevice
    : public VRDeviceBase,
      public mojom::SensorClient {
 public:
  VROrientationDevice(mojom::SensorProvider* sensor_provider,
                      base::OnceClosure ready_callback);
  ~VROrientationDevice() override;

  // VRDevice
  void RequestSession(
      mojom::XRRuntimeSessionOptionsPtr options,
      mojom::XRRuntime::RequestSessionCallback callback) override;
  void ShutdownSession(mojom::XRRuntime::ShutdownSessionCallback) override;

  // Indicates whether the device was able to connect to orientation events.
  bool IsAvailable() const { return available_; }

  void EndMagicWindowSession(VROrientationSession* session);
  virtual void GetInlineFrameData(
      mojom::XRFrameDataProvider::GetFrameDataCallback callback);

 private:
  // SensorClient Functions.
  void RaiseError() override;
  void SensorReadingChanged() override {}

  // Sensor event reaction functions.
  void SensorReady(device::mojom::SensorCreationResult result,
                   device::mojom::SensorInitParamsPtr params);
  void HandleSensorError();
  void OnSensorAddConfiguration(bool success);

  gfx::Quaternion SensorSpaceToWorldSpace(gfx::Quaternion q);
  gfx::Quaternion WorldSpaceToUserOrientedSpace(gfx::Quaternion q);

  bool available_ = false;
  base::OnceClosure ready_callback_;

  // The initial state of the world used to define forwards.
  std::optional<gfx::Quaternion> base_pose_;
  gfx::Quaternion latest_pose_;

  mojo::Remote<mojom::Sensor> sensor_;
  std::unique_ptr<SensorReadingSharedBufferReader> shared_buffer_reader_;
  mojo::Receiver<mojom::SensorClient> receiver_{this};

  std::vector<std::unique_ptr<VROrientationSession>> magic_window_sessions_;
};

}  // namespace device

#endif  // DEVICE_VR_ORIENTATION_ORIENTATION_DEVICE_H_
