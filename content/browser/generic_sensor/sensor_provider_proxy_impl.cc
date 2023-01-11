// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/generic_sensor/sensor_provider_proxy_impl.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"

using device::mojom::SensorType;

using device::mojom::SensorCreationResult;

namespace content {

namespace {

SensorProviderProxyImpl::SensorProviderBinder& GetBinderOverride() {
  static base::NoDestructor<SensorProviderProxyImpl::SensorProviderBinder>
      binder;
  return *binder;
}

}  // namespace

SensorProviderProxyImpl::SensorProviderProxyImpl(
    RenderFrameHost* render_frame_host)
    : DocumentUserData<SensorProviderProxyImpl>(render_frame_host) {
  DCHECK(render_frame_host);
}

SensorProviderProxyImpl::~SensorProviderProxyImpl() = default;

void SensorProviderProxyImpl::Bind(
    mojo::PendingReceiver<device::mojom::SensorProvider> receiver) {
  receiver_set_.Add(this, std::move(receiver));
}

// static
void SensorProviderProxyImpl::OverrideSensorProviderBinderForTesting(
    SensorProviderBinder binder) {
  GetBinderOverride() = std::move(binder);
}

void SensorProviderProxyImpl::GetSensor(SensorType type,
                                        GetSensorCallback callback) {
  if (!CheckFeaturePolicies(type)) {
    std::move(callback).Run(SensorCreationResult::ERROR_NOT_ALLOWED, nullptr);
    return;
  }

  if (!sensor_provider_) {
    auto receiver = sensor_provider_.BindNewPipeAndPassReceiver();
    sensor_provider_.set_disconnect_handler(base::BindOnce(
        &SensorProviderProxyImpl::OnConnectionError, base::Unretained(this)));

    const auto& binder = GetBinderOverride();
    if (binder)
      binder.Run(std::move(receiver));
    else
      GetDeviceService().BindSensorProvider(std::move(receiver));
  }

  render_frame_host()
      .GetBrowserContext()
      ->GetPermissionController()
      ->RequestPermissionFromCurrentDocument(
          blink::PermissionType::SENSORS, &render_frame_host(), false,
          base::BindOnce(&SensorProviderProxyImpl::OnPermissionRequestCompleted,
                         weak_factory_.GetWeakPtr(), type,
                         std::move(callback)));
}

void SensorProviderProxyImpl::OnPermissionRequestCompleted(
    SensorType type,
    GetSensorCallback callback,
    blink::mojom::PermissionStatus status) {
  if (status != blink::mojom::PermissionStatus::GRANTED || !sensor_provider_) {
    std::move(callback).Run(SensorCreationResult::ERROR_NOT_ALLOWED, nullptr);
    return;
  }

  // Unblock the orientation sensors as these are tested to play well with
  // back-forward cache. This is conservative.
  // TODO(crbug.com/1027985): Test and unblock all of the sensors to work with
  // back-forward cache.
  switch (type) {
    case SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES:
    case SensorType::ABSOLUTE_ORIENTATION_QUATERNION:
    case SensorType::RELATIVE_ORIENTATION_EULER_ANGLES:
    case SensorType::RELATIVE_ORIENTATION_QUATERNION:
      break;
    default:
      static_cast<RenderFrameHostImpl*>(&render_frame_host())
          ->OnBackForwardCacheDisablingStickyFeatureUsed(
              blink::scheduler::WebSchedulerTrackedFeature::
                  kRequestedBackForwardCacheBlockedSensors);
  }
  sensor_provider_->GetSensor(type, std::move(callback));
}

namespace {

std::vector<blink::mojom::PermissionsPolicyFeature>
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
    default:
      NOTREACHED() << "Unknown sensor type " << type;
      return {};
  }
}

}  // namespace

bool SensorProviderProxyImpl::CheckFeaturePolicies(SensorType type) const {
  const std::vector<blink::mojom::PermissionsPolicyFeature>& features =
      SensorTypeToPermissionsPolicyFeatures(type);
  return base::ranges::all_of(
      features, [this](blink::mojom::PermissionsPolicyFeature feature) {
        return render_frame_host().IsFeatureEnabled(feature);
      });
}

void SensorProviderProxyImpl::OnConnectionError() {
  // Close all the bindings to notify them of this failure as the
  // GetSensorCallbacks will never be called.
  receiver_set_.Clear();
  sensor_provider_.reset();
}

DOCUMENT_USER_DATA_KEY_IMPL(SensorProviderProxyImpl);

}  // namespace content
