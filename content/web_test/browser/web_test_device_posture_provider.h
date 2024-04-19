// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_BROWSER_WEB_TEST_DEVICE_POSTURE_PROVIDER_H_
#define CONTENT_WEB_TEST_BROWSER_WEB_TEST_DEVICE_POSTURE_PROVIDER_H_

#include "base/memory/weak_ptr.h"
#include "third_party/blink/public/mojom/device_posture/device_posture_provider.mojom-shared.h"
#include "third_party/blink/public/mojom/device_posture/device_posture_provider_automation.mojom.h"

namespace content {

class RenderFrameHostImpl;

class WebTestDevicePostureProvider
    : public blink::test::mojom::DevicePostureProviderAutomation {
 public:
  explicit WebTestDevicePostureProvider(base::WeakPtr<RenderFrameHostImpl>);
  ~WebTestDevicePostureProvider() override;

  WebTestDevicePostureProvider(const WebTestDevicePostureProvider&) = delete;
  WebTestDevicePostureProvider& operator=(const WebTestDevicePostureProvider&) =
      delete;

  // blink::test::mojom::DevicePostureProviderAutomation overrides.
  void SetPostureOverride(blink::mojom::DevicePostureType posture) override;
  void ClearPostureOverride() override;

 private:
  base::WeakPtr<RenderFrameHostImpl> render_frame_host_;
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_BROWSER_WEB_TEST_DEVICE_POSTURE_PROVIDER_H_
