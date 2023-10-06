// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GENERIC_SENSOR_WEB_CONTENTS_SENSOR_PROVIDER_PROXY_H_
#define CONTENT_BROWSER_GENERIC_SENSOR_WEB_CONTENTS_SENSOR_PROVIDER_PROXY_H_

#include "base/observer_list.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"

namespace content {

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

  // Allows tests to override how this class binds its backing SensorProvider
  // endpoint.
  using SensorProviderBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<device::mojom::SensorProvider>)>;
  static void OverrideSensorProviderBinderForTesting(
      SensorProviderBinder binder);

  void AddObserver(Observer*);
  void RemoveObserver(Observer*);

 private:
  explicit WebContentsSensorProviderProxy(content::WebContents* web_contents);

  void OnConnectionError();

  void EnsureDeviceServiceConnection();

  mojo::Remote<device::mojom::SensorProvider> sensor_provider_;

  base::ObserverList<Observer, /*check_empty=*/true> observers_;

  friend WebContentsUserData;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_GENERIC_SENSOR_WEB_CONTENTS_SENSOR_PROVIDER_PROXY_H_
