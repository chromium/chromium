// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ORIENTATION_DEVICE_H
#define DEVICE_VR_ORIENTATION_DEVICE_H

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
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
// TODO(crbug.com/730440) If RELATIVE_ORIENTATION_QUATERNION is ever
// implemented on Windows, use that instead.
static constexpr mojom::SensorType kOrientationSensorType =
#if defined(OS_WIN)
    mojom::SensorType::ABSOLUTE_ORIENTATION_QUATERNION;
#else
    mojom::SensorType::RELATIVE_ORIENTATION_QUATERNION;
#endif

// This class connects the orientation sensor events to the Web VR apis.
class DEVICE_VR_EXPORT VROrientationDevice : public VRDeviceBase,
                                             public mojom::SensorClient {
 public:
  VROrientationDevice(mojom::SensorProvider* sensor_provider,
                      base::OnceClosure ready_callback);
  ~VROrientationDevice() override;

  // VRDevice
  void RequestSession(
      mojom::XRRuntimeSessionOptionsPtr options,
      mojom::XRRuntime::RequestSessionCallback callback) override;

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
  base::Optional<gfx::Quaternion> base_pose_;
  gfx::Quaternion latest_pose_;

  mojo::Remote<mojom::Sensor> sensor_;
  std::unique_ptr<SensorReadingSharedBufferReader> shared_buffer_reader_;
  mojo::Receiver<mojom::SensorClient> receiver_{this};

  std::vector<std::unique_ptr<VROrientationSession>> magic_window_sessions_;
};

}  // namespace device

#endif  // DEVICE_VR_ORIENTATION_DEVICE_H
