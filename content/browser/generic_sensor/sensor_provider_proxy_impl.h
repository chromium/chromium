// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GENERIC_SENSOR_SENSOR_PROVIDER_PROXY_IMPL_H_
#define CONTENT_BROWSER_GENERIC_SENSOR_SENSOR_PROVIDER_PROXY_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

namespace content {

class PermissionControllerImpl;
class RenderFrameHost;

// This proxy acts as a gatekeeper to the real sensor provider so that this
// proxy can intercept sensor requests and allow or deny them based on
// the permission statuses retrieved from a permission controller.
class SensorProviderProxyImpl final : public device::mojom::SensorProvider {
 public:
  SensorProviderProxyImpl(PermissionControllerImpl* permission_controller,
                          RenderFrameHost* render_frame_host);
  ~SensorProviderProxyImpl() override;

  void Bind(mojo::PendingReceiver<device::mojom::SensorProvider> receiver);

 private:
  // SensorProvider implementation.
  void GetSensor(device::mojom::SensorType type,
                 GetSensorCallback callback) override;

  bool CheckFeaturePolicies(device::mojom::SensorType type) const;
  void OnPermissionRequestCompleted(device::mojom::SensorType type,
                                    GetSensorCallback callback,
                                    blink::mojom::PermissionStatus);
  void OnConnectionError();

  mojo::ReceiverSet<device::mojom::SensorProvider> receiver_set_;
  PermissionControllerImpl* permission_controller_;
  RenderFrameHost* render_frame_host_;
  mojo::Remote<device::mojom::SensorProvider> sensor_provider_;

  base::WeakPtrFactory<SensorProviderProxyImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SensorProviderProxyImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GENERIC_SENSOR_SENSOR_PROVIDER_PROXY_IMPL_H_
