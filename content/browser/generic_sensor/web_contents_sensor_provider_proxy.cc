// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/generic_sensor/web_contents_sensor_provider_proxy.h"

#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "content/browser/generic_sensor/frame_sensor_provider_proxy.h"
#include "content/public/browser/device_service.h"

using device::mojom::SensorCreationResult;
using device::mojom::SensorType;

namespace content {

namespace {

WebContentsSensorProviderProxy::SensorProviderBinder& GetBinderOverride() {
  static base::NoDestructor<
      WebContentsSensorProviderProxy::SensorProviderBinder>
      binder;
  return *binder;
}

}  // namespace

ScopedVirtualSensorForDevTools::ScopedVirtualSensorForDevTools(
    SensorType type,
    device::mojom::VirtualSensorMetadataPtr metadata,
    WebContentsSensorProviderProxy* web_contents_sensor_provider)
    : type_(type), web_contents_sensor_provider_(web_contents_sensor_provider) {
  web_contents_sensor_provider_->CreateVirtualSensor(
      type_, std::move(metadata),
      base::BindOnce([](device::mojom::CreateVirtualSensorResult result) {
        switch (result) {
          case device::mojom::CreateVirtualSensorResult::
              kSensorTypeAlreadyOverridden:
            NOTREACHED() << "WebContentsSensorProviderProxy::"
                            "CreateVirtualSensorForDevTools() should have "
                            "prevented this result";
          case device::mojom::CreateVirtualSensorResult::kSuccess:
            break;
        }
      }));
}

ScopedVirtualSensorForDevTools::~ScopedVirtualSensorForDevTools() {
  web_contents_sensor_provider_->RemoveVirtualSensor(type_,
                                                     base::NullCallback());
}

void ScopedVirtualSensorForDevTools::GetVirtualSensorInformation(
    device::mojom::SensorProvider::GetVirtualSensorInformationCallback
        callback) {
  web_contents_sensor_provider_->GetVirtualSensorInformation(
      type_, std::move(callback));
}

void ScopedVirtualSensorForDevTools::UpdateVirtualSensor(
    const device::SensorReading& reading,
    device::mojom::SensorProvider::UpdateVirtualSensorCallback callback) {
  web_contents_sensor_provider_->UpdateVirtualSensor(type_, reading,
                                                     std::move(callback));
}

WebContentsSensorProviderProxy::WebContentsSensorProviderProxy(
    WebContents* web_contents)
    : WebContentsUserData<WebContentsSensorProviderProxy>(*web_contents) {}

WebContentsSensorProviderProxy::~WebContentsSensorProviderProxy() = default;

// static
WebContentsSensorProviderProxy* WebContentsSensorProviderProxy::GetOrCreate(
    WebContents* web_contents) {
  WebContentsUserData<WebContentsSensorProviderProxy>::CreateForWebContents(
      web_contents);
  return WebContentsUserData<WebContentsSensorProviderProxy>::FromWebContents(
      web_contents);
}

std::unique_ptr<ScopedVirtualSensorForDevTools>
WebContentsSensorProviderProxy::CreateVirtualSensorForDevTools(
    SensorType type,
    device::mojom::VirtualSensorMetadataPtr metadata) {
  auto did_insert = virtual_sensor_types_for_devtools_.insert(type).second;
  if (!did_insert) {
    return nullptr;
  }
  return base::WrapUnique(
      new ScopedVirtualSensorForDevTools(type, std::move(metadata), this));
}

// static
void WebContentsSensorProviderProxy::OverrideSensorProviderBinderForTesting(
    SensorProviderBinder binder) {
  GetBinderOverride() = std::move(binder);
}

void WebContentsSensorProviderProxy::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void WebContentsSensorProviderProxy::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void WebContentsSensorProviderProxy::GetSensor(
    SensorType type,
    device::mojom::SensorProvider::GetSensorCallback callback) {
  EnsureDeviceServiceConnection();
  sensor_provider_->GetSensor(type, std::move(callback));
}

void WebContentsSensorProviderProxy::CreateVirtualSensor(
    SensorType type,
    device::mojom::VirtualSensorMetadataPtr metadata,
    device::mojom::SensorProvider::CreateVirtualSensorCallback callback) {
  // CHECK that this was called via CreateVirtualSensorForDevTools().
  CHECK(base::Contains(virtual_sensor_types_for_devtools_, type));
  EnsureDeviceServiceConnection();
  sensor_provider_->CreateVirtualSensor(type, std::move(metadata),
                                        std::move(callback));
}

void WebContentsSensorProviderProxy::UpdateVirtualSensor(
    SensorType type,
    const device::SensorReading& reading,
    device::mojom::SensorProvider::UpdateVirtualSensorCallback callback) {
  EnsureDeviceServiceConnection();
  sensor_provider_->UpdateVirtualSensor(type, reading, std::move(callback));
}

void WebContentsSensorProviderProxy::RemoveVirtualSensor(
    SensorType type,
    device::mojom::SensorProvider::RemoveVirtualSensorCallback callback) {
  virtual_sensor_types_for_devtools_.erase(type);
  EnsureDeviceServiceConnection();
  sensor_provider_->RemoveVirtualSensor(type, std::move(callback));
}

void WebContentsSensorProviderProxy::GetVirtualSensorInformation(
    SensorType type,
    device::mojom::SensorProvider::GetVirtualSensorInformationCallback
        callback) {
  EnsureDeviceServiceConnection();
  sensor_provider_->GetVirtualSensorInformation(type, std::move(callback));
}

void WebContentsSensorProviderProxy::OnConnectionError() {
  // FrameSensorProviderProxy instances need to be notified first so that the
  // corresponding mojo::Receivers are cleared before the mojo::Remotes below.
  for (auto& observer : observers_) {
    observer.OnMojoConnectionError();
  }
  sensor_provider_.reset();
}

void WebContentsSensorProviderProxy::EnsureDeviceServiceConnection() {
  if (sensor_provider_) {
    return;
  }

  auto receiver = sensor_provider_.BindNewPipeAndPassReceiver();
  sensor_provider_.set_disconnect_handler(
      base::BindOnce(&WebContentsSensorProviderProxy::OnConnectionError,
                     base::Unretained(this)));

  const auto& binder = GetBinderOverride();
  if (binder) {
    binder.Run(std::move(receiver));
  } else {
    GetDeviceService().BindSensorProvider(std::move(receiver));
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebContentsSensorProviderProxy);

}  // namespace content
