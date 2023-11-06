// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_BROWSER_WEB_TEST_SENSOR_PROVIDER_MANAGER_H_
#define CONTENT_WEB_TEST_BROWSER_WEB_TEST_SENSOR_PROVIDER_MANAGER_H_

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/device/public/mojom/sensor_provider.mojom-shared.h"
#include "third_party/blink/public/mojom/sensor/web_sensor_provider_automation.mojom.h"

namespace content {

class ScopedVirtualSensorForDevTools;
class WebContents;

// Implementation of the VirtualSensorProvider Mojo interface for use by
// Blink's InternalsSensor. It implements the required method calls exposed by
// testdriver.js by forwarding the calls to WebContentsSensorProviderProxy.
class WebTestSensorProviderManager
    : public blink::test::mojom::WebSensorProviderAutomation {
 public:
  explicit WebTestSensorProviderManager(WebContents* web_contents);

  WebTestSensorProviderManager(const WebTestSensorProviderManager&) = delete;
  WebTestSensorProviderManager& operator=(const WebTestSensorProviderManager&) =
      delete;

  ~WebTestSensorProviderManager() override;

  void Bind(
      mojo::PendingReceiver<blink::test::mojom::WebSensorProviderAutomation>
          receiver);

  // blink::mojom::SensorProviderAutomation overrides.
  void CreateVirtualSensor(device::mojom::SensorType type,
                           device::mojom::VirtualSensorMetadataPtr metadata,
                           CreateVirtualSensorCallback callback) override;
  void UpdateVirtualSensor(device::mojom::SensorType type,
                           const device::SensorReading& reading,
                           UpdateVirtualSensorCallback callback) override;
  void RemoveVirtualSensor(device::mojom::SensorType type,
                           RemoveVirtualSensorCallback callback) override;
  void GetVirtualSensorInformation(
      device::mojom::SensorType type,
      GetVirtualSensorInformationCallback callback) override;

 private:
  base::flat_map<device::mojom::SensorType,
                 std::unique_ptr<ScopedVirtualSensorForDevTools>>
      sensor_overrides_;

  mojo::ReceiverSet<blink::test::mojom::WebSensorProviderAutomation> receivers_;

  base::WeakPtr<WebContents> web_contents_;
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_BROWSER_WEB_TEST_SENSOR_PROVIDER_MANAGER_H_
