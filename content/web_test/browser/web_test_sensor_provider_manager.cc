// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/browser/web_test_sensor_provider_manager.h"

#include "base/notreached.h"
#include "content/browser/generic_sensor/web_contents_sensor_provider_proxy.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents.h"

namespace content {

WebTestSensorProviderManager::WebTestSensorProviderManager(
    WebContents* web_contents)
    : web_contents_(static_cast<WebContentsImpl*>(web_contents)->GetWeakPtr()) {
}

WebTestSensorProviderManager::~WebTestSensorProviderManager() = default;

void WebTestSensorProviderManager::Bind(
    mojo::PendingReceiver<blink::test::mojom::WebSensorProviderAutomation>
        receiver) {
  receivers_.Add(this, std::move(receiver));
}

void WebTestSensorProviderManager::CreateVirtualSensor(
    device::mojom::SensorType type,
    device::mojom::VirtualSensorMetadataPtr metadata,
    device::mojom::SensorProvider::CreateVirtualSensorCallback callback) {
  if (!web_contents_) {
    return;
  }

  if (sensor_overrides_.contains(type)) {
    std::move(callback).Run(
        device::mojom::CreateVirtualSensorResult::kSensorTypeAlreadyOverridden);
    return;
  }

  auto virtual_sensor =
      WebContentsSensorProviderProxy::GetOrCreate(web_contents_.get())
          ->CreateVirtualSensorForDevTools(type, std::move(metadata));
  CHECK(virtual_sensor);
  sensor_overrides_[type] = std::move(virtual_sensor);

  std::move(callback).Run(device::mojom::CreateVirtualSensorResult::kSuccess);
}

void WebTestSensorProviderManager::UpdateVirtualSensor(
    device::mojom::SensorType type,
    const device::SensorReading& reading,
    device::mojom::SensorProvider::UpdateVirtualSensorCallback callback) {
  if (!web_contents_) {
    return;
  }

  auto it = sensor_overrides_.find(type);
  if (it == sensor_overrides_.end()) {
    std::move(callback).Run(
        device::mojom::UpdateVirtualSensorResult::kSensorTypeNotOverridden);
    return;
  }

  it->second->UpdateVirtualSensor(std::move(reading), std::move(callback));
}

void WebTestSensorProviderManager::RemoveVirtualSensor(
    device::mojom::SensorType type,
    device::mojom::SensorProvider::RemoveVirtualSensorCallback callback) {
  if (!web_contents_) {
    return;
  }

  sensor_overrides_.erase(type);
  std::move(callback).Run();
}

void WebTestSensorProviderManager::GetVirtualSensorInformation(
    device::mojom::SensorType type,
    device::mojom::SensorProvider::GetVirtualSensorInformationCallback
        callback) {
  if (!web_contents_) {
    return;
  }

  auto it = sensor_overrides_.find(type);
  if (it == sensor_overrides_.end()) {
    std::move(callback).Run(
        device::mojom::GetVirtualSensorInformationResult::NewError(
            device::mojom::GetVirtualSensorInformationError::
                kSensorTypeNotOverridden));
    return;
  }

  it->second->GetVirtualSensorInformation(std::move(callback));
}

}  // namespace content
