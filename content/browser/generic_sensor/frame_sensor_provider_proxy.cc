// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/generic_sensor/frame_sensor_provider_proxy.h"

#include <algorithm>
#include <vector>

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "components/content_settings/core/common/features.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/permission_request_description.h"
#include "content/public/browser/sensor_delegate.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/content_client.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/mojom/sensor_provider.mojom-shared.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

using device::mojom::SensorType;

namespace content {

namespace {

constexpr std::vector<network::mojom::PermissionsPolicyFeature>
SensorTypeToPermissionsPolicyFeatures(SensorType type) {
  switch (type) {
    case SensorType::AMBIENT_LIGHT:
      return {network::mojom::PermissionsPolicyFeature::kAmbientLightSensor};
    case SensorType::ACCELEROMETER:
    case SensorType::LINEAR_ACCELERATION:
    case SensorType::GRAVITY:
      return {network::mojom::PermissionsPolicyFeature::kAccelerometer};
    case SensorType::GYROSCOPE:
      return {network::mojom::PermissionsPolicyFeature::kGyroscope};
    case SensorType::MAGNETOMETER:
      return {network::mojom::PermissionsPolicyFeature::kMagnetometer};
    case SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES:
    case SensorType::ABSOLUTE_ORIENTATION_QUATERNION:
      return {network::mojom::PermissionsPolicyFeature::kAccelerometer,
              network::mojom::PermissionsPolicyFeature::kGyroscope,
              network::mojom::PermissionsPolicyFeature::kMagnetometer};
    case SensorType::RELATIVE_ORIENTATION_EULER_ANGLES:
    case SensorType::RELATIVE_ORIENTATION_QUATERNION:
      return {network::mojom::PermissionsPolicyFeature::kAccelerometer,
              network::mojom::PermissionsPolicyFeature::kGyroscope};
  }
}

// Lightweight static cache to remember sensors missing on this device
std::vector<device::mojom::SensorType>& GetUnavailableSensorsCache() {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);
  static base::NoDestructor<std::vector<device::mojom::SensorType>> cache;
  return *cache;
}

}  // namespace

FrameSensorProviderProxy::FrameSensorProviderProxy(
    RenderFrameHost* render_frame_host)
    : DocumentUserData<FrameSensorProviderProxy>(render_frame_host),
      WebContentsObserver(WebContents::FromRenderFrameHost(render_frame_host)) {
  client_controllers_.set_disconnect_handler(
      base::BindRepeating(&FrameSensorProviderProxy::OnSensorDisconnect,
                          weak_factory_.GetWeakPtr()));
}

FrameSensorProviderProxy::~FrameSensorProviderProxy() {
  if (permission_subscription_id_) {
    auto* permission_controller =
        render_frame_host().GetBrowserContext()->GetPermissionController();
    permission_controller->UnsubscribeFromPermissionResultChange(
        permission_subscription_id_);
  }

  // Notify the delegate for each active connection that is being cleared.
  auto* delegate = GetContentClient()->browser()->GetSensorDelegate();
  if (delegate) {
    for (size_t i = 0; i < client_controllers_.size(); ++i) {
      delegate->OnSensorStopped(&render_frame_host());
    }
  }
}

void FrameSensorProviderProxy::Bind(
    mojo::PendingReceiver<blink::mojom::WebSensorProvider> receiver) {
  receiver_set_.Add(this, std::move(receiver));
}

void FrameSensorProviderProxy::OnMojoConnectionError() {
  receiver_set_.Clear();
  client_controllers_.Clear();
}

void FrameSensorProviderProxy::GetSensor(device::mojom::SensorType type,
                                         bool user_gesture,
                                         GetSensorCallback callback) {
  const bool passes_permissions_policy_check = std::ranges::all_of(
      SensorTypeToPermissionsPolicyFeatures(type),
      [this](network::mojom::PermissionsPolicyFeature feature) {
        return render_frame_host().IsFeatureEnabled(feature);
      });

  if (!passes_permissions_policy_check) {
    std::move(callback).Run(
        device::mojom::SensorCreationResult::ERROR_NOT_ALLOWED, nullptr);
    return;
  }

  auto* permission_controller =
      render_frame_host().GetBrowserContext()->GetPermissionController();

  bool has_valid_gesture =
      user_gesture && render_frame_host().HasTransientUserActivation();

  auto permission_status =
      permission_controller->GetPermissionStatusForCurrentDocument(
          content::PermissionDescriptorUtil::
              CreatePermissionDescriptorForPermissionType(
                  blink::PermissionType::SENSORS),
          &render_frame_host());

  if (permission_status == blink::mojom::PermissionStatus::DENIED ||
      (permission_status == blink::mojom::PermissionStatus::ASK &&
       !has_valid_gesture)) {
    std::move(callback).Run(
        device::mojom::SensorCreationResult::ERROR_NOT_ALLOWED, nullptr);

    // Check our global static cache to see if we already know the hardware is
    // missing.
    if (std::ranges::contains(GetUnavailableSensorsCache(), type)) {
      return;
    }

    auto* web_contents_sensor_provider =
        WebContentsSensorProviderProxy::GetOrCreate(
            WebContents::FromRenderFrameHost(&render_frame_host()));

    web_contents_sensor_provider->GetSensor(
        type, mojo::NullReceiver(), /*initially_suspended=*/false,
        base::BindOnce(
            &FrameSensorProviderProxy::OnHardwareCheckForBlockedSensor,
            weak_factory_.GetWeakPtr(), type));
    return;
  }

  auto* web_contents_sensor_provider =
      WebContentsSensorProviderProxy::GetOrCreate(
          WebContents::FromRenderFrameHost(&render_frame_host()));
  if (!scoped_observation_.IsObserving()) {
    scoped_observation_.Observe(web_contents_sensor_provider);
  }

  mojo::PendingReceiver<device::mojom::SensorClientController>
      controller_receiver;
  mojo::PendingRemote<device::mojom::SensorClientController> controller;
  bool initially_suspended = false;

  if (ShouldTrackSensorConnection()) {
    if (!permission_subscription_id_) {
      permission_subscription_id_ =
          permission_controller->SubscribeToPermissionResultChange(
              content::PermissionDescriptorUtil::
                  CreatePermissionDescriptorForPermissionType(
                      blink::PermissionType::SENSORS),
              nullptr, &render_frame_host(),
              render_frame_host().GetLastCommittedOrigin().GetURL(),
              /*should_include_device_status=*/false,
              base::BindRepeating(
                  &FrameSensorProviderProxy::OnPermissionChanged,
                  weak_factory_.GetWeakPtr()));
    }

    controller_receiver = controller.InitWithNewPipeAndPassReceiver();
    if (base::FeatureList::IsEnabled(features::kSensorPrivacyMitigations)) {
      initially_suspended = ShouldSuspendSensors();
    }
  }

  web_contents_sensor_provider->GetSensor(
      type, std::move(controller_receiver), initially_suspended,
      base::BindOnce(&FrameSensorProviderProxy::OnHardwareCheckCompleted,
                     weak_factory_.GetWeakPtr(), type, permission_status,
                     has_valid_gesture, std::move(controller),
                     std::move(callback)));
}

void FrameSensorProviderProxy::OnHardwareCheckCompleted(
    device::mojom::SensorType type,
    blink::mojom::PermissionStatus permission_status,
    bool user_gesture,
    mojo::PendingRemote<device::mojom::SensorClientController> controller,
    GetSensorCallback callback,
    device::mojom::SensorCreationResult result,
    device::mojom::SensorInitParamsPtr params) {
  if (result != device::mojom::SensorCreationResult::SUCCESS) {
    if (result == device::mojom::SensorCreationResult::ERROR_NOT_AVAILABLE) {
      if (!std::ranges::contains(GetUnavailableSensorsCache(), type)) {
        GetUnavailableSensorsCache().push_back(type);
      }
    }
    std::move(callback).Run(result, nullptr);
    return;
  }

  if (permission_status == blink::mojom::PermissionStatus::GRANTED) {
    FinalizeSensorConnection(std::move(controller));
    std::move(callback).Run(result, std::move(params));
    return;
  }

  CHECK_EQ(permission_status, blink::mojom::PermissionStatus::ASK);
  CHECK(user_gesture);

  auto* permission_controller =
      render_frame_host().GetBrowserContext()->GetPermissionController();
  auto permission_descriptor = content::PermissionDescriptorUtil::
      CreatePermissionDescriptorForPermissionType(
          blink::PermissionType::SENSORS);

  permission_controller->RequestPermissionFromCurrentDocument(
      &render_frame_host(),
      PermissionRequestDescription(std::move(permission_descriptor),
                                   user_gesture),
      base::BindOnce(&FrameSensorProviderProxy::OnPermissionRequestCompleted,
                     weak_factory_.GetWeakPtr(), std::move(params),
                     std::move(callback), std::move(controller)));
}

void FrameSensorProviderProxy::OnHardwareCheckForBlockedSensor(
    device::mojom::SensorType type,
    device::mojom::SensorCreationResult result,
    device::mojom::SensorInitParamsPtr params) {
  if (result == device::mojom::SensorCreationResult::ERROR_NOT_AVAILABLE) {
    if (!std::ranges::contains(GetUnavailableSensorsCache(), type)) {
      GetUnavailableSensorsCache().push_back(type);
    }
    // No need to update PSCS since requested_sensor_has_hardware_ defaults to
    // false
  } else if (result == device::mojom::SensorCreationResult::SUCCESS ||
             result == device::mojom::SensorCreationResult::ERROR_NOT_ALLOWED) {
    // Hardware is present (or blocked at OS level), so we should record that
    // the sensor was requested and blocked.
    SensorDelegate* delegate =
        GetContentClient()->browser()->GetSensorDelegate();
    if (delegate) {
      delegate->SetRequestedSensorIsAvailable(&render_frame_host(), true);
      delegate->OnSensorBlocked(&render_frame_host());
    }
  }
}

void FrameSensorProviderProxy::OnPermissionRequestCompleted(
    device::mojom::SensorInitParamsPtr params,
    GetSensorCallback callback,
    mojo::PendingRemote<device::mojom::SensorClientController> controller,
    PermissionResult permission_result) {
  if (permission_result.status != blink::mojom::PermissionStatus::GRANTED) {
    std::move(callback).Run(
        device::mojom::SensorCreationResult::ERROR_NOT_ALLOWED, nullptr);
    return;
  }

  FinalizeSensorConnection(std::move(controller));

  std::move(callback).Run(device::mojom::SensorCreationResult::SUCCESS,
                          std::move(params));
}

void FrameSensorProviderProxy::OnPermissionChanged(
    PermissionResult permission_result) {
  if (permission_result.status != blink::mojom::PermissionStatus::GRANTED) {
    SensorDelegate* delegate =
        GetContentClient()->browser()->GetSensorDelegate();
    if (delegate) {
      // Notify the delegate for each active connection that is being cleared.
      for (size_t i = 0; i < client_controllers_.size(); ++i) {
        delegate->OnSensorStopped(&render_frame_host());
      }
    }
    client_controllers_.Clear();
  }
}

void FrameSensorProviderProxy::OnVisibilityChanged(
    content::Visibility visibility) {
  UpdateSensorSessionControllers();
}

void FrameSensorProviderProxy::RenderFrameHostStateChanged(
    RenderFrameHost* render_frame_host,
    RenderFrameHost::LifecycleState old_state,
    RenderFrameHost::LifecycleState new_state) {
  if (render_frame_host == &this->render_frame_host()) {
    UpdateSensorSessionControllers();
  }
}

bool FrameSensorProviderProxy::ShouldSuspendSensors() const {
  return !web_contents() || !render_frame_host().IsActive() ||
         web_contents()->GetVisibility() == content::Visibility::HIDDEN;
}

void FrameSensorProviderProxy::UpdateSensorSessionControllers() {
  bool suspended = ShouldSuspendSensors();
  if (is_suspended_ && *is_suspended_ == suspended) {
    return;
  }
  is_suspended_ = suspended;
  for (auto& controller : client_controllers_) {
    if (suspended) {
      controller->Suspend();
    } else {
      controller->Resume();
    }
  }
}

void FrameSensorProviderProxy::OnSensorDisconnect(mojo::RemoteSetElementId id) {
  SensorDelegate* delegate = GetContentClient()->browser()->GetSensorDelegate();
  if (delegate) {
    delegate->OnSensorStopped(&render_frame_host());
  }
}

void FrameSensorProviderProxy::FinalizeSensorConnection(
    mojo::PendingRemote<device::mojom::SensorClientController> controller) {
  SensorDelegate* delegate = GetContentClient()->browser()->GetSensorDelegate();
  bool bound_controller = false;
  if (ShouldTrackSensorConnection() && controller.is_valid()) {
    mojo::RemoteSetElementId id =
        client_controllers_.Add(std::move(controller));
    if (is_suspended_.value_or(ShouldSuspendSensors())) {
      client_controllers_.Get(id)->Suspend();
    }
    bound_controller = true;
  }

  if (delegate) {
    delegate->SetRequestedSensorIsAvailable(&render_frame_host(), true);
    if (bound_controller) {
      delegate->OnSensorStarted(&render_frame_host());
    }
  }
}

bool FrameSensorProviderProxy::ShouldTrackSensorConnection() {
  return base::FeatureList::IsEnabled(
             features::kSensorsAllowAskBlockPermissionModel) ||
         base::FeatureList::IsEnabled(features::kSensorPrivacyMitigations) ||
         base::FeatureList::IsEnabled(
             content_settings::features::kLeftHandSideSensorActivityIndicators);
}

DOCUMENT_USER_DATA_KEY_IMPL(FrameSensorProviderProxy);

}  // namespace content
