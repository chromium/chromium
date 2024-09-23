// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GENERIC_SENSOR_FRAME_SENSOR_PROVIDER_PROXY_H_
#define CONTENT_BROWSER_GENERIC_SENSOR_FRAME_SENSOR_PROVIDER_PROXY_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "content/browser/generic_sensor/web_contents_sensor_provider_proxy.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_user_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/device/public/mojom/sensor.mojom-shared.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-shared.h"
#include "third_party/blink/public/mojom/sensor/web_sensor_provider.mojom.h"

namespace features {
CONTENT_EXPORT BASE_DECLARE_FEATURE(kAllowSensorsToEnterBfcache);
}  // namespace features

namespace content {

// Per-RenderFrameHost SensorProvider implementation. It does permission checks
// before forwarding GetSensor() calls in WebContentsSensorProviderProxy.
class FrameSensorProviderProxy final
    : public blink::mojom::WebSensorProvider,
      public WebContentsSensorProviderProxy::Observer,
      public DocumentUserData<FrameSensorProviderProxy> {
 public:
  FrameSensorProviderProxy(const FrameSensorProviderProxy&) = delete;
  FrameSensorProviderProxy& operator=(const FrameSensorProviderProxy&) = delete;

  ~FrameSensorProviderProxy() override;

  void Bind(mojo::PendingReceiver<blink::mojom::WebSensorProvider> receiver);

  // WebContentsSensorProviderProxy::Observer overrides.
  void OnMojoConnectionError() override;

 private:
  explicit FrameSensorProviderProxy(RenderFrameHost* render_frame_host);

  // blink::mojom::WebSensorProvider overrides.
  void GetSensor(device::mojom::SensorType type,
                 GetSensorCallback callback) override;

  void OnPermissionRequestCompleted(device::mojom::SensorType type,
                                    GetSensorCallback callback,
                                    blink::mojom::PermissionStatus);

  mojo::ReceiverSet<blink::mojom::WebSensorProvider> receiver_set_;

  base::ScopedObservation<WebContentsSensorProviderProxy,
                          WebContentsSensorProviderProxy::Observer>
      scoped_observation_{this};

  base::WeakPtrFactory<FrameSensorProviderProxy> weak_factory_{this};

  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_GENERIC_SENSOR_FRAME_SENSOR_PROVIDER_PROXY_H_
