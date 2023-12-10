// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_DEVICE_REQUEST_PROMPT_INFO_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_DEVICE_REQUEST_PROMPT_INFO_H_

#include <string>
#include <vector>

namespace content {

struct DevtoolsDeviceRequestPromptDevice {
  std::string id;
  std::string name;
};

class DevtoolsDeviceRequestPromptInfo {
 public:
  virtual ~DevtoolsDeviceRequestPromptInfo() = default;
  virtual std::vector<DevtoolsDeviceRequestPromptDevice> GetDevices() = 0;
  virtual bool SelectDevice(const std::string& device_id) = 0;
  virtual void Cancel() = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_DEVICE_REQUEST_PROMPT_INFO_H_
