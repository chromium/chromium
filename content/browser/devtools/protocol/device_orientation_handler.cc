// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/device_orientation_handler.h"

#include "base/functional/bind.h"
#include "content/browser/devtools/protocol/emulation.h"
#include "content/browser/devtools/protocol/emulation_handler.h"
#include "content/browser/devtools/protocol/protocol.h"
#include "content/browser/generic_sensor/web_contents_sensor_provider_proxy.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "services/device/public/cpp/generic_sensor/orientation_util.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"

namespace content {
namespace protocol {

DeviceOrientationHandler::DeviceOrientationHandler()
    : DevToolsDomainHandler(DeviceOrientation::Metainfo::domainName) {}

DeviceOrientationHandler::~DeviceOrientationHandler() = default;

void DeviceOrientationHandler::Wire(UberDispatcher* dispatcher) {
  DeviceOrientation::Dispatcher::wire(dispatcher, this);
}

void DeviceOrientationHandler::SetRenderer(int process_host_id,
                                           RenderFrameHostImpl* frame_host) {
  if (!frame_host) {
    Disable();
  }
  frame_host_ = frame_host;
}

Response DeviceOrientationHandler::Disable() {
  return ClearDeviceOrientationOverride();
}

Response DeviceOrientationHandler::ClearDeviceOrientationOverride() {
  virtual_sensor_.reset();
  return Response::FallThrough();
}

void DeviceOrientationHandler::SetDeviceOrientationOverride(
    double alpha,
    double beta,
    double gamma,
    std::unique_ptr<SetDeviceOrientationOverrideCallback> callback) {
  if (!frame_host_) {
    callback->sendFailure(Response::InternalError());
    return;
  }
  if (!frame_host_->IsOutermostMainFrame()) {
    // Virtual sensors are handled at the WebContents level, so there is no need
    // to try to run the code below if we are not the outermost frame -- it will
    // just result in CreateVirtualSensorForDevTools() returning nullptr.
    callback->fallThrough();
    return;
  }

  device::SensorReading quaternion_readings;
  if (!device::ComputeQuaternionFromEulerAngles(alpha, beta, gamma,
                                                &quaternion_readings)) {
    callback->sendFailure(Response::InvalidParams(
        "Failed to convert Euler angles to quaternions. Invalid alpha, beta, "
        "or gamma value."));
    return;
  }
  quaternion_readings.raw.timestamp =
      (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();

  if (!virtual_sensor_) {
    virtual_sensor_ =
        WebContentsSensorProviderProxy::GetOrCreate(GetWebContents())
            ->CreateVirtualSensorForDevTools(
                device::mojom::SensorType::RELATIVE_ORIENTATION_QUATERNION,
                device::mojom::VirtualSensorMetadata::New());
    if (!virtual_sensor_) {
      // Already overridden elsewhere (e.g. EmulationHandler).
      callback->sendFailure(Response::InvalidParams(
          "The 'relative-orientation' sensor type is already overridden, "
          "possibly by the Emulation domain."));
      return;
    }
  }

  virtual_sensor_->UpdateVirtualSensor(quaternion_readings,
                                       base::NullCallback());
  callback->fallThrough();
}

WebContentsImpl* DeviceOrientationHandler::GetWebContents() {
  CHECK(frame_host_);  // Only call if |frame_host_| is set.
  return static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(frame_host_));
}

}  // namespace protocol
}  // namespace content
