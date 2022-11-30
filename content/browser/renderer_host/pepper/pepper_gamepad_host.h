// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_GAMEPAD_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_GAMEPAD_HOST_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "device/gamepad/gamepad_consumer.h"
#include "ppapi/host/resource_host.h"

namespace device {
class GamepadService;
}

namespace ppapi {
namespace host {
struct ReplyMessageContext;
}
}

namespace content {

class BrowserPpapiHost;

class CONTENT_EXPORT PepperGamepadHost :
    public ppapi::host::ResourceHost,
    public device::GamepadConsumer {
 public:
  PepperGamepadHost(BrowserPpapiHost* host,
                    PP_Instance instance,
                    PP_Resource resource);

  // Allows tests to specify a gamepad service to use rather than the global
  // singleton. The caller owns the gamepad_service pointer.
  PepperGamepadHost(device::GamepadService* gamepad_service,
                    BrowserPpapiHost* host,
                    PP_Instance instance,
                    PP_Resource resource);

  PepperGamepadHost(const PepperGamepadHost&) = delete;
  PepperGamepadHost& operator=(const PepperGamepadHost&) = delete;

  ~PepperGamepadHost() override;

  int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) override;

  // device::GamepadConsumer implementation.
  void OnGamepadConnected(uint32_t index,
                          const device::Gamepad& gamepad) override {}
  void OnGamepadDisconnected(uint32_t index,
                             const device::Gamepad& gamepad) override {}

 private:
  int32_t OnRequestMemory(ppapi::host::HostMessageContext* context);

  void GotUserGesture(const ppapi::host::ReplyMessageContext& in_context);

  raw_ptr<device::GamepadService> gamepad_service_;

  bool is_started_;

  base::WeakPtrFactory<PepperGamepadHost> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_GAMEPAD_HOST_H_
