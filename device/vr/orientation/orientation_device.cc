// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>

#include <numbers>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "device/vr/orientation/orientation_device.h"
#include "device/vr/orientation/orientation_session.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading_shared_buffer.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading_shared_buffer_reader.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace device {

using gfx::Quaternion;
using gfx::Vector3dF;

namespace {
static constexpr int kDefaultPumpFrequencyHz = 60;

display::Display::Rotation GetRotation() {
  display::Screen* screen = display::Screen::GetScreen();
  if (!screen) {
    // If we can't get rotation we'll assume it's 0.
    return display::Display::ROTATE_0;
  }

  return screen->GetPrimaryDisplay().rotation();
}

const std::vector<mojom::XRSessionFeature>& GetSupportedFeatures() {
  static base::NoDestructor<std::vector<mojom::XRSessionFeature>>
      kSupportedFeatures{{
    mojom::XRSessionFeature::REF_SPACE_VIEWER,
    mojom::XRSessionFeature::REF_SPACE_LOCAL,
    mojom::XRSessionFeature::REF_SPACE_LOCAL_FLOOR,
  }};

  return *kSupportedFeatures;
}

}  // namespace

VROrientationDevice::VROrientationDevice(mojom::SensorProvider* sensor_provider,
                                         base::OnceClosure ready_callback)
    : VRDeviceBase(mojom::XRDeviceId::ORIENTATION_DEVICE_ID),
      ready_callback_(std::move(ready_callback)) {
  DVLOG(2) << __func__;
  sensor_provider->GetSensor(kOrientationSensorType,
                             base::BindOnce(&VROrientationDevice::SensorReady,
                                            base::Unretained(this)));

  SetSupportedFeatures(GetSupportedFeatures());
}

VROrientationDevice::~VROrientationDevice() {
  DVLOG(2) << __func__;
}

void VROrientationDevice::SensorReady(
    device::mojom::SensorCreationResult,
    device::mojom::SensorInitParamsPtr params) {
  if (!params) {
    // This means that there are no orientation sensors on this device.
    HandleSensorError();
    std::move(ready_callback_).Run();
    return;
  }

  DVLOG(2) << __func__;
  constexpr size_t kReadBufferSize = sizeof(device::SensorReadingSharedBuffer);

  DCHECK_EQ(0u, params->buffer_offset % kReadBufferSize);

  device::PlatformSensorConfiguration default_config =
      params->default_configuration;

  sensor_.Bind(std::move(params->sensor));

  receiver_.Bind(std::move(params->client_receiver));

  shared_buffer_reader_ = device::SensorReadingSharedBufferReader::Create(
      std::move(params->memory), params->buffer_offset);
  if (!shared_buffer_reader_) {
    // If we cannot read data, we cannot supply a device.
    HandleSensorError();
    std::move(ready_callback_).Run();
    return;
  }

  default_config.set_frequency(kDefaultPumpFrequencyHz);
  sensor_.set_disconnect_handler(base::BindOnce(
      &VROrientationDevice::HandleSensorError, base::Unretained(this)));
  sensor_->ConfigureReadingChangeNotifications(false /* disabled */);
  sensor_->AddConfiguration(
      default_config,
      base::BindOnce(&VROrientationDevice::OnSensorAddConfiguration,
                     base::Unretained(this)));
}

// Mojo callback for Sensor::AddConfiguration().
void VROrientationDevice::OnSensorAddConfiguration(bool success) {
  if (!success) {
    // Sensor config is not supported so we can't provide sensor events.
    HandleSensorError();
  } else {
    // We're good to go.
    available_ = true;
  }

  std::move(ready_callback_).Run();
}

void VROrientationDevice::RaiseError() {
  HandleSensorError();
}

void VROrientationDevice::HandleSensorError() {
  sensor_.reset();
  shared_buffer_reader_.reset();
  receiver_.reset();
}

void VROrientationDevice::RequestSession(
    mojom::XRRuntimeSessionOptionsPtr options,
    mojom::XRRuntime::RequestSessionCallback callback) {
  DVLOG(2) << __func__;
  DCHECK_EQ(options->mode, mojom::XRSessionMode::kInline);

  // TODO(http://crbug.com/695937): Perform a check to see if sensors are
  // available when RequestSession is called for non-immersive sessions.

  mojo::PendingRemote<mojom::XRFrameDataProvider> data_provider;
  mojo::PendingRemote<mojom::XRSessionController> controller;
  magic_window_sessions_.push_back(std::make_unique<VROrientationSession>(
      this, data_provider.InitWithNewPipeAndPassReceiver(),
      controller.InitWithNewPipeAndPassReceiver()));

  auto session_result = mojom::XRRuntimeSessionResult::New();
  session_result->controller = std::move(controller);

  session_result->session = mojom::XRSession::New();
  auto* session = session_result->session.get();

  session->data_provider = std::move(data_provider);
  session->device_config = device::mojom::XRSessionDeviceConfig::New();
  session->enviroment_blend_mode =
      device::mojom::XREnvironmentBlendMode::kOpaque;
  session->interaction_mode = device::mojom::XRInteractionMode::kScreenSpace;

  // Currently, the initial filtering of supported devices happens on the
  // browser side (BrowserXRRuntimeImpl::SupportsFeature()), so if we have
  // reached this point, it is safe to assume that all requested features are
  // enabled.
  // TODO(crbug.com/41476975): revisit the approach when the bug is fixed.
  session->enabled_features.insert(session->enabled_features.end(),
                                   options->required_features.begin(),
                                   options->required_features.end());
  session->enabled_features.insert(session->enabled_features.end(),
                                   options->optional_features.begin(),
                                   options->optional_features.end());

  std::move(callback).Run(std::move(session_result));

  // The sensor may have been suspended, so resume it now.
  sensor_->Resume();
}

void VROrientationDevice::ShutdownSession(
    mojom::XRRuntime::ShutdownSessionCallback callback) {
  // We don't actually have enough information here to figure out which session
  // is being requested to be terminated. However, since sessions don't get
  // exclusive control of the device and we can drive many sessions at once,
  // there's not really anything for us to do here except to reply to the
  // callback.
  // The session will end up getting shutdown via other mechanisms (some of
  // its mojom pipes getting torn down during destruction in the other
  // processes as a result of continuing the flow here).
  std::move(callback).Run();
}

void VROrientationDevice::EndMagicWindowSession(VROrientationSession* session) {
  DVLOG(2) << __func__;
  std::erase_if(magic_window_sessions_,
                [session](const std::unique_ptr<VROrientationSession>& item) {
                  return item.get() == session;
                });

  // If there are no more magic window sessions, suspend the sensor until we get
  // a new one.
  if (magic_window_sessions_.empty()) {
    sensor_->Suspend();
  }
}

void VROrientationDevice::GetInlineFrameData(
    mojom::XRFrameDataProvider::GetFrameDataCallback callback) {
  // Orientation sessions should never be exclusive or presenting.
  DCHECK(!HasExclusiveSession());

  mojom::VRPosePtr pose = mojom::VRPose::New();

  SensorReading latest_reading;
  // If the reading fails just return the last pose that we got.
  if (shared_buffer_reader_->GetReading(&latest_reading)) {
    latest_pose_.set_x(latest_reading.orientation_quat.x);
    latest_pose_.set_y(latest_reading.orientation_quat.y);
    latest_pose_.set_z(latest_reading.orientation_quat.z);
    latest_pose_.set_w(latest_reading.orientation_quat.w);

    latest_pose_ =
        WorldSpaceToUserOrientedSpace(SensorSpaceToWorldSpace(latest_pose_));
  }

  pose->orientation = latest_pose_;

  mojom::XRFrameDataPtr frame_data = mojom::XRFrameData::New();
  frame_data->mojo_from_viewer = std::move(pose);

  std::move(callback).Run(std::move(frame_data));
}

Quaternion VROrientationDevice::SensorSpaceToWorldSpace(Quaternion q) {
  display::Display::Rotation rotation = GetRotation();

  if (rotation == display::Display::ROTATE_90) {
    // Rotate the sensor reading to account for the screen rotation.
    q = q * Quaternion(Vector3dF(0, 0, 1), -std::numbers::pi / 2);
  } else if (rotation == display::Display::ROTATE_270) {
    // Rotate the sensor reading to account for the screen rotation the other
    // way.
    q = q * Quaternion(Vector3dF(0, 0, 1), std::numbers::pi / 2);
  }

  // Tilt the view up to have the y axis as the vertical axis instead of z
  q = Quaternion(Vector3dF(1, 0, 0), -std::numbers::pi / 2) * q;

  return q;
}

Quaternion VROrientationDevice::WorldSpaceToUserOrientedSpace(Quaternion q) {
  if (!base_pose_) {
    // Check that q is valid by checking if the length is not 0 (it should
    // technically always be 1, but this accounts for rounding errors).
    if (!(q.Length() > .1)) {
      // q is invalid. Do not use for base pose.
      return q;
    }

    // A base pose to read the initial forward direction off of.
    base_pose_ = q;

    // Extract the yaw from base pose to use as the base forward direction.
    base_pose_->set_x(0);
    base_pose_->set_z(0);
    base_pose_ = base_pose_->Normalized();
  }

  // Adjust the base forward on the orientation to where the original forward
  // was.
  q = base_pose_->inverse() * q;

  return q;
}

}  // namespace device
