// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/generic_sensor/frame_sensor_provider_proxy.h"

#include <vector>

#include "base/notreached.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_request_description.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/device/public/mojom/sensor_provider.mojom-shared.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

using device::mojom::SensorType;

namespace features {
BASE_FEATURE(kAllowSensorsToEnterBfcache,
             "AllowSensorsToEnterBfcache",
             base::FEATURE_DISABLED_BY_DEFAULT);
}

namespace content {

namespace {

constexpr std::vector<blink::mojom::PermissionsPolicyFeature>
SensorTypeToPermissionsPolicyFeatures(SensorType type) {
  switch (type) {
    case SensorType::AMBIENT_LIGHT:
      return {blink::mojom::PermissionsPolicyFeature::kAmbientLightSensor};
    case SensorType::ACCELEROMETER:
    case SensorType::LINEAR_ACCELERATION:
    case SensorType::GRAVITY:
      return {blink::mojom::PermissionsPolicyFeature::kAccelerometer};
    case SensorType::GYROSCOPE:
      return {blink::mojom::PermissionsPolicyFeature::kGyroscope};
    case SensorType::MAGNETOMETER:
      return {blink::mojom::PermissionsPolicyFeature::kMagnetometer};
    case SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES:
    case SensorType::ABSOLUTE_ORIENTATION_QUATERNION:
      return {blink::mojom::PermissionsPolicyFeature::kAccelerometer,
              blink::mojom::PermissionsPolicyFeature::kGyroscope,
              blink::mojom::PermissionsPolicyFeature::kMagnetometer};
    case SensorType::RELATIVE_ORIENTATION_EULER_ANGLES:
    case SensorType::RELATIVE_ORIENTATION_QUATERNION:
      return {blink::mojom::PermissionsPolicyFeature::kAccelerometer,
              blink::mojom::PermissionsPolicyFeature::kGyroscope};
  }
}

}  // namespace

FrameSensorProviderProxy::FrameSensorProviderProxy(
    RenderFrameHost* render_frame_host)
    : DocumentUserData<FrameSensorProviderProxy>(render_frame_host) {}

FrameSensorProviderProxy::~FrameSensorProviderProxy() = default;

void FrameSensorProviderProxy::Bind(
    mojo::PendingReceiver<blink::mojom::WebSensorProvider> receiver) {
  receiver_set_.Add(this, std::move(receiver));
}

void FrameSensorProviderProxy::OnMojoConnectionError() {
  receiver_set_.Clear();
}

void FrameSensorProviderProxy::GetSensor(device::mojom::SensorType type,
                                         GetSensorCallback callback) {
  const bool passes_permissions_policy_check = base::ranges::all_of(
      SensorTypeToPermissionsPolicyFeatures(type),
      [this](blink::mojom::PermissionsPolicyFeature feature) {
        return render_frame_host().IsFeatureEnabled(feature);
      });

  if (!passes_permissions_policy_check) {
    std::move(callback).Run(
        device::mojom::SensorCreationResult::ERROR_NOT_ALLOWED, nullptr);
    return;
  }

  render_frame_host()
      .GetBrowserContext()
      ->GetPermissionController()
      ->RequestPermissionFromCurrentDocument(
          &render_frame_host(),
          PermissionRequestDescription(blink::PermissionType::SENSORS),
          base::BindOnce(
              &FrameSensorProviderProxy::OnPermissionRequestCompleted,
              weak_factory_.GetWeakPtr(), type, std::move(callback)));
}

void FrameSensorProviderProxy::OnPermissionRequestCompleted(
    SensorType type,
    GetSensorCallback callback,
    blink::mojom::PermissionStatus status) {
  if (status != blink::mojom::PermissionStatus::GRANTED) {
    std::move(callback).Run(
        device::mojom::SensorCreationResult::ERROR_NOT_ALLOWED, nullptr);
    return;
  }

  // Unblock the orientation sensors as these are tested to play well with
  // back-forward cache. This is conservative.
  // TODO(crbug.com/40660549): Test and unblock all of the sensors to work with
  // back-forward cache.
  switch (type) {
    case SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES:
    case SensorType::ABSOLUTE_ORIENTATION_QUATERNION:
    case SensorType::RELATIVE_ORIENTATION_EULER_ANGLES:
    case SensorType::RELATIVE_ORIENTATION_QUATERNION:
      break;
    default:
      if (!base::FeatureList::IsEnabled(
              features::kAllowSensorsToEnterBfcache)) {
        static_cast<RenderFrameHostImpl*>(&render_frame_host())
            ->OnBackForwardCacheDisablingStickyFeatureUsed(
                blink::scheduler::WebSchedulerTrackedFeature::
                    kRequestedBackForwardCacheBlockedSensors);
      }
  }

  auto* web_contents_sensor_provider =
      WebContentsSensorProviderProxy::GetOrCreate(
          WebContents::FromRenderFrameHost(&render_frame_host()));
  if (!scoped_observation_.IsObserving()) {
    scoped_observation_.Observe(web_contents_sensor_provider);
  }
  web_contents_sensor_provider->GetSensor(type, std::move(callback));
}

DOCUMENT_USER_DATA_KEY_IMPL(FrameSensorProviderProxy);

}  // namespace content
