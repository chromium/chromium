// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/generic_sensor/sensor_provider_proxy_impl.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/system_connector.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom.h"

using device::mojom::SensorType;

using device::mojom::SensorCreationResult;

namespace content {

SensorProviderProxyImpl::SensorProviderProxyImpl(
    PermissionControllerImpl* permission_controller,
    RenderFrameHost* render_frame_host)
    : permission_controller_(permission_controller),
      render_frame_host_(render_frame_host) {
  DCHECK(permission_controller);
  DCHECK(render_frame_host);
}

SensorProviderProxyImpl::~SensorProviderProxyImpl() = default;

void SensorProviderProxyImpl::Bind(
    mojo::PendingReceiver<device::mojom::SensorProvider> receiver) {
  receiver_set_.Add(this, std::move(receiver));
}

void SensorProviderProxyImpl::GetSensor(SensorType type,
                                        GetSensorCallback callback) {
  if (!CheckFeaturePolicies(type)) {
    std::move(callback).Run(SensorCreationResult::ERROR_NOT_ALLOWED, nullptr);
    return;
  }

  if (!sensor_provider_) {
    auto* connector = GetSystemConnector();
    if (!connector) {
      std::move(callback).Run(SensorCreationResult::ERROR_NOT_AVAILABLE,
                              nullptr);
      return;
    }

    connector->Connect(device::mojom::kServiceName,
                       sensor_provider_.BindNewPipeAndPassReceiver());
    sensor_provider_.set_disconnect_handler(base::BindOnce(
        &SensorProviderProxyImpl::OnConnectionError, base::Unretained(this)));
  }

  // TODO(shalamov): base::BindOnce should be used (https://crbug.com/714018),
  // however, PermissionController::RequestPermission enforces use of repeating
  // callback.
  permission_controller_->RequestPermission(
      PermissionType::SENSORS, render_frame_host_,
      render_frame_host_->GetLastCommittedURL().GetOrigin(), false,
      base::BindRepeating(
          &SensorProviderProxyImpl::OnPermissionRequestCompleted,
          weak_factory_.GetWeakPtr(), type, base::Passed(std::move(callback))));
}

void SensorProviderProxyImpl::OnPermissionRequestCompleted(
    device::mojom::SensorType type,
    GetSensorCallback callback,
    blink::mojom::PermissionStatus status) {
  if (status != blink::mojom::PermissionStatus::GRANTED || !sensor_provider_) {
    std::move(callback).Run(SensorCreationResult::ERROR_NOT_ALLOWED, nullptr);
    return;
  }
  sensor_provider_->GetSensor(type, std::move(callback));
}

namespace {

std::vector<blink::mojom::FeaturePolicyFeature>
SensorTypeToFeaturePolicyFeatures(SensorType type) {
  switch (type) {
    case SensorType::AMBIENT_LIGHT:
      return {blink::mojom::FeaturePolicyFeature::kAmbientLightSensor};
    case SensorType::ACCELEROMETER:
    case SensorType::LINEAR_ACCELERATION:
      return {blink::mojom::FeaturePolicyFeature::kAccelerometer};
    case SensorType::GYROSCOPE:
      return {blink::mojom::FeaturePolicyFeature::kGyroscope};
    case SensorType::MAGNETOMETER:
      return {blink::mojom::FeaturePolicyFeature::kMagnetometer};
    case SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES:
    case SensorType::ABSOLUTE_ORIENTATION_QUATERNION:
      return {blink::mojom::FeaturePolicyFeature::kAccelerometer,
              blink::mojom::FeaturePolicyFeature::kGyroscope,
              blink::mojom::FeaturePolicyFeature::kMagnetometer};
    case SensorType::RELATIVE_ORIENTATION_EULER_ANGLES:
    case SensorType::RELATIVE_ORIENTATION_QUATERNION:
      return {blink::mojom::FeaturePolicyFeature::kAccelerometer,
              blink::mojom::FeaturePolicyFeature::kGyroscope};
    default:
      NOTREACHED() << "Unknown sensor type " << type;
      return {};
  }
}

}  // namespace

bool SensorProviderProxyImpl::CheckFeaturePolicies(SensorType type) const {
  const std::vector<blink::mojom::FeaturePolicyFeature>& features =
      SensorTypeToFeaturePolicyFeatures(type);
  return std::all_of(features.begin(), features.end(),
                     [this](blink::mojom::FeaturePolicyFeature feature) {
                       return render_frame_host_->IsFeatureEnabled(feature);
                     });
}

void SensorProviderProxyImpl::OnConnectionError() {
  // Close all the bindings to notify them of this failure as the
  // GetSensorCallbacks will never be called.
  receiver_set_.Clear();
  sensor_provider_.reset();
}

}  // namespace content
