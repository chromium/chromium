// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/generic_sensor/web_contents_sensor_provider_proxy.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
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
