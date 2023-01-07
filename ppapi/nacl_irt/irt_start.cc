// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/at_exit.h"
#include "mojo/core/embedder/embedder.h"
#include "native_client/src/public/chrome_main.h"
#include "native_client/src/public/irt_core.h"
#include "ppapi/nacl_irt/irt_interfaces.h"
#include "ppapi/nacl_irt/plugin_startup.h"

namespace {
IPC::ChannelHandle MakeIPCHandle(int fd) {
  return IPC::ChannelHandle(base::FileDescriptor(fd, false /* auto_close */));
}
}  // namespace

void nacl_irt_start(uint32_t* info) {
  nacl_irt_init(info);

  // Though it isn't referenced here, we must instantiate an AtExitManager.
  base::AtExitManager exit_manager;

  // In SFI mode, the FDs of IPC channels are NACL_CHROME_DESC_BASE and its
  // successor, which is set in nacl_listener.cc.
  ppapi::SetIPCChannelHandles(MakeIPCHandle(NACL_CHROME_DESC_BASE),
                              MakeIPCHandle(NACL_CHROME_DESC_BASE + 1),
                              MakeIPCHandle(NACL_CHROME_DESC_BASE + 2));
  // The Mojo EDK must be initialized before using IPC.
  mojo::core::Init();
  ppapi::StartUpPlugin();

  nacl_irt_enter_user_code(info, chrome_irt_query);
}
