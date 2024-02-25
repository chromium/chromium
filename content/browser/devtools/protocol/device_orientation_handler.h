// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVICE_ORIENTATION_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVICE_ORIENTATION_HANDLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "content/browser/devtools/protocol/device_orientation.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"

namespace content {

class RenderFrameHostImpl;
class ScopedVirtualSensorForDevTools;
class WebContentsImpl;

namespace protocol {

// Handling of this CDP domain is also partially implemented in Blink. See
// DeviceOrientationInspectorAgent.
class DeviceOrientationHandler : public DevToolsDomainHandler,
                                 public DeviceOrientation::Backend {
 public:
  DeviceOrientationHandler();

  DeviceOrientationHandler(const DeviceOrientationHandler&) = delete;
  DeviceOrientationHandler& operator=(const DeviceOrientationHandler&) = delete;

  ~DeviceOrientationHandler() override;

 private:
  WebContentsImpl* GetWebContents();

  // DevToolsDomainHandler overrides.
  void Wire(UberDispatcher* dispatcher) override;
  void SetRenderer(int process_host_id,
                   RenderFrameHostImpl* frame_host) override;
  Response Disable() override;

  // DeviceOrientation::Backend overrides.
  Response ClearDeviceOrientationOverride() override;
  void SetDeviceOrientationOverride(
      double alpha,
      double beta,
      double gamma,
      std::unique_ptr<SetDeviceOrientationOverrideCallback> callback) override;

  // This is safe to store because it is updated by SetRenderer(). It is
  // guaranteed that SetRenderer() is called before a RFH is deleted.
  // See bug 323904196.
  raw_ptr<RenderFrameHostImpl> frame_host_ = nullptr;

  std::unique_ptr<ScopedVirtualSensorForDevTools> virtual_sensor_;
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVICE_ORIENTATION_HANDLER_H_
