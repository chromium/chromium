// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/browser/web_test_device_posture_provider.h"

#include "content/browser/device_posture/device_posture_provider_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/device_posture/device_posture_provider_automation.mojom.h"

namespace content {

WebTestDevicePostureProvider::WebTestDevicePostureProvider(
    base::WeakPtr<RenderFrameHostImpl> render_frame_host)
    : render_frame_host_(std::move(render_frame_host)) {}

WebTestDevicePostureProvider::~WebTestDevicePostureProvider() = default;

void WebTestDevicePostureProvider::SetPostureOverride(
    blink::mojom::DevicePostureType posture) {
  if (!render_frame_host_) {
    return;
  }
  static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(render_frame_host_.get()))
      ->GetDevicePostureProvider()
      ->OverrideDevicePostureForEmulation(posture);
}

void WebTestDevicePostureProvider::ClearPostureOverride() {
  if (!render_frame_host_) {
    return;
  }
  static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(render_frame_host_.get()))
      ->GetDevicePostureProvider()
      ->DisableDevicePostureOverrideForEmulation();
}

}  // namespace content
