// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_GAMEPAD_RESOURCE_H_
#define PPAPI_PROXY_GAMEPAD_RESOURCE_H_

#include "base/compiler_specific.h"
#include "base/memory/shared_memory_mapping.h"
#include "device/gamepad/public/mojom/gamepad_hardware_buffer.h"
#include "ppapi/c/ppb_gamepad.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/thunk/ppb_gamepad_api.h"

struct PP_GamepadsSampleData;

namespace ppapi {
namespace proxy {

// This class is a bit weird. It isn't a true resource from the plugin's
// perspective. But we need to make requests to the browser and get replies.
// It's more convenient to do this as a resource, so the instance just
// maintains an internal lazily instantiated copy of this resource.
class PPAPI_PROXY_EXPORT GamepadResource
      : public PluginResource,
        public thunk::PPB_Gamepad_API {
 public:
  GamepadResource(Connection connection, PP_Instance instance);

  GamepadResource(const GamepadResource&) = delete;
  GamepadResource& operator=(const GamepadResource&) = delete;

  ~GamepadResource() override;

  // Resource implementation.
  thunk::PPB_Gamepad_API* AsPPB_Gamepad_API() override;

  // PPB_Gamepad_API.
  void Sample(PP_Instance instance, PP_GamepadsSampleData* data) override;

 private:
  void OnPluginMsgSendMemory(const ResourceMessageReplyParams& params);

  base::ReadOnlySharedMemoryMapping shared_memory_mapping_;
  const device::GamepadHardwareBuffer* buffer_;

  // Last data returned so we can use this in the event of a read failure.
  PP_GamepadsSampleData last_read_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_GAMEPAD_RESOURCE_H_
