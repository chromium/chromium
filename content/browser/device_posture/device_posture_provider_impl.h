// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_POSTURE_DEVICE_POSTURE_PROVIDER_IMPL_H_
#define CONTENT_BROWSER_DEVICE_POSTURE_DEVICE_POSTURE_PROVIDER_IMPL_H_

#include "content/browser/device_posture/device_posture_platform_provider.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/device_posture/device_posture_provider.mojom.h"

namespace content {

class DevicePostureProviderImpl final
    : public blink::mojom::DevicePostureProvider,
      public WebContentsUserData<DevicePostureProviderImpl>,
      public DevicePosturePlatformProvider::Observer {
 public:
  static DevicePostureProviderImpl* GetOrCreate(WebContents*);
  explicit DevicePostureProviderImpl(content::WebContents* web_contents);

  ~DevicePostureProviderImpl() override;
  DevicePostureProviderImpl(const DevicePostureProviderImpl&) = delete;
  DevicePostureProviderImpl& operator=(const DevicePostureProviderImpl&) =
      delete;
  void Bind(
      mojo::PendingReceiver<blink::mojom::DevicePostureProvider> receiver);
  DevicePosturePlatformProvider* platform_provider() const;

  // DevicePostureProvider implementation.
  CONTENT_EXPORT void OverrideDevicePostureForEmulation(
      blink::mojom::DevicePostureType posture) override;
  CONTENT_EXPORT void DisableDevicePostureOverrideForEmulation() override;

 private:
  // DevicePostureClient implementation.
  void OnDevicePostureChanged(
      const blink::mojom::DevicePostureType& posture) override;

  // DevicePostureProvider implementation.
  void AddListenerAndGetCurrentPosture(
      mojo::PendingRemote<blink::mojom::DevicePostureClient> client,
      AddListenerAndGetCurrentPostureCallback callback) override;

  void OnRemoteDisconnect(mojo::RemoteSetElementId);

  std::unique_ptr<DevicePosturePlatformProvider> platform_provider_;
  bool is_posture_emulated_ = false;
  mojo::ReceiverSet<blink::mojom::DevicePostureProvider> receivers_;
  mojo::RemoteSet<blink::mojom::DevicePostureClient> posture_clients_;

  friend WebContentsUserData;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_POSTURE_DEVICE_POSTURE_PROVIDER_IMPL_H_
