// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVICE_ACCESS_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVICE_ACCESS_HANDLER_H_

#include "content/browser/devtools/devtools_device_request_prompt_info.h"
#include "content/browser/devtools/protocol/device_access.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"

namespace content {

namespace protocol {

class DeviceAccessHandler : public DevToolsDomainHandler,
                            public DeviceAccess::Backend {
 public:
  DeviceAccessHandler();
  ~DeviceAccessHandler() override;

  static std::vector<DeviceAccessHandler*> ForAgentHost(
      DevToolsAgentHostImpl* host);
  void Wire(UberDispatcher* dispatcher) override;

  DeviceAccessHandler(const DeviceAccessHandler&) = delete;
  DeviceAccessHandler& operator=(const DeviceAccessHandler&) = delete;

  void UpdateDeviceRequestPrompt(DevtoolsDeviceRequestPromptInfo* prompt_info);
  void CleanUpDeviceRequestPrompt(DevtoolsDeviceRequestPromptInfo* prompt_info);

 private:
  DispatchResponse Enable() override;
  DispatchResponse Disable() override;
  DispatchResponse SelectPrompt(const String& in_id,
                                const String& in_deviceId) override;
  DispatchResponse CancelPrompt(const String& in_id) override;

  const std::string& FindOrAddRequestId(
      DevtoolsDeviceRequestPromptInfo* prompt_info);
  DevtoolsDeviceRequestPromptInfo* FindRequest(const String& requestId);

  std::optional<DeviceAccess::Frontend> frontend_;

  bool enabled_ = false;

  // Map to PromptInfo instances until CleanUp is called, removing them
  // from the map.
  base::flat_map<std::string /*id*/, raw_ptr<DevtoolsDeviceRequestPromptInfo>>
      request_prompt_infos_;
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DEVICE_ACCESS_HANDLER_H_
