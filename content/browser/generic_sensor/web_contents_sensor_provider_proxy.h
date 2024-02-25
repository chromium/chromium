// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GENERIC_SENSOR_WEB_CONTENTS_SENSOR_PROVIDER_PROXY_H_
#define CONTENT_BROWSER_GENERIC_SENSOR_WEB_CONTENTS_SENSOR_PROVIDER_PROXY_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "services/device/public/mojom/sensor.mojom-shared.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"

namespace content {

class ScopedVirtualSensorForDevTools;

// This proxy acts as a gatekeeper to the real sensor provider so that this
// proxy can intercept sensor requests and allow or deny them based on
// the permission statuses retrieved from a permission controller.
//
// Its mojo::ReceiverSet references FrameSensorProviderProxy to retrieve
// RenderHostInformation used in the checks
class CONTENT_EXPORT WebContentsSensorProviderProxy final
    : public WebContentsUserData<WebContentsSensorProviderProxy> {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnMojoConnectionError() = 0;
  };

  WebContentsSensorProviderProxy(const WebContentsSensorProviderProxy&) =
      delete;
  WebContentsSensorProviderProxy& operator=(
      const WebContentsSensorProviderProxy&) = delete;

  ~WebContentsSensorProviderProxy() override;

  static WebContentsSensorProviderProxy* GetOrCreate(WebContents*);

  void GetSensor(device::mojom::SensorType type,
                 device::mojom::SensorProvider::GetSensorCallback callback);

  // Attempts to create and return a ScopedVirtualSensorForDevTools instance of
  // a given |type| if one does not exist (and therefore a |type| virtual
  // sensor has not been created), otherwise returns nullptr.
  std::unique_ptr<ScopedVirtualSensorForDevTools>
  CreateVirtualSensorForDevTools(
      device::mojom::SensorType type,
      device::mojom::VirtualSensorMetadataPtr metadata);

  // Allows tests to override how this class binds its backing SensorProvider
  // endpoint.
  using SensorProviderBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<device::mojom::SensorProvider>)>;
  static void OverrideSensorProviderBinderForTesting(
      SensorProviderBinder binder);

  void AddObserver(Observer*);
  void RemoveObserver(Observer*);

 private:
  friend class ScopedVirtualSensorForDevTools;

  explicit WebContentsSensorProviderProxy(content::WebContents* web_contents);

  // These functions as wrappers around their Mojo counterparts. They are
  // supposed to be called via ScopedVirtualSensorForDevTools.
  void CreateVirtualSensor(
      device::mojom::SensorType type,
      device::mojom::VirtualSensorMetadataPtr metadata,
      device::mojom::SensorProvider::CreateVirtualSensorCallback callback);
  void UpdateVirtualSensor(
      device::mojom::SensorType type,
      const device::SensorReading& reading,
      device::mojom::SensorProvider::UpdateVirtualSensorCallback callback);
  void RemoveVirtualSensor(
      device::mojom::SensorType type,
      device::mojom::SensorProvider::RemoveVirtualSensorCallback callback);
  void GetVirtualSensorInformation(
      device::mojom::SensorType type,
      device::mojom::SensorProvider::GetVirtualSensorInformationCallback
          callback);

  void OnConnectionError();

  void EnsureDeviceServiceConnection();

  mojo::Remote<device::mojom::SensorProvider> sensor_provider_;

  base::ObserverList<Observer, /*check_empty=*/true> observers_;

  base::flat_set<device::mojom::SensorType> virtual_sensor_types_for_devtools_;

  friend WebContentsUserData;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

// This class is the public interface to the virtual-sensor related Mojo calls.
// Instances are created by
// WebContentsSensorProviderProxy::CreateVirtualSensorForDevTools().
//
// RemoveVirtualSensor() is invoked automatically on destruction, and
// CreateVirtualSensor() is invoked automatically on creation.
class CONTENT_EXPORT ScopedVirtualSensorForDevTools {
 public:
  ~ScopedVirtualSensorForDevTools();

  void GetVirtualSensorInformation(
      device::mojom::SensorProvider::GetVirtualSensorInformationCallback
          callback);

  void UpdateVirtualSensor(
      const device::SensorReading& reading,
      device::mojom::SensorProvider::UpdateVirtualSensorCallback callback);

 private:
  friend class WebContentsSensorProviderProxy;

  ScopedVirtualSensorForDevTools(
      device::mojom::SensorType type,
      device::mojom::VirtualSensorMetadataPtr metadata,
      WebContentsSensorProviderProxy*);

  device::mojom::SensorType type_;

  // From https://crrev.com/c/4770864: WebContentsSensorProviderProxy is
  // associated to a WebContents and will not get destroyed before the WC, and
  // on the DevTools side the contract is the
  // DevToolsAgentHost::DisconnectWebContents() will be called before the WC is
  // destroyed.
  raw_ptr<WebContentsSensorProviderProxy> web_contents_sensor_provider_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_GENERIC_SENSOR_WEB_CONTENTS_SENSOR_PROVIDER_PROXY_H_
