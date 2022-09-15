// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/x11_util.h"

#include "base/bind.h"
#include "remoting/base/logging.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/xinput.h"
#include "ui/gfx/x/xproto_types.h"
#include "ui/gfx/x/xtest.h"

namespace remoting {

ScopedXGrabServer::ScopedXGrabServer(x11::Connection* connection)
    : connection_(connection) {
  connection_->GrabServer();
}

ScopedXGrabServer::~ScopedXGrabServer() {
  connection_->UngrabServer();
  connection_->Flush();
}

bool IgnoreXServerGrabs(x11::Connection* connection, bool ignore) {
  if (!connection->xtest()
           .GetVersion({x11::Test::major_version, x11::Test::minor_version})
           .Sync()) {
    return false;
  }

  connection->xtest().GrabControl({ignore});
  return true;
}

bool IsVirtualSession(x11::Connection* connection) {
  // Try to identify a virtual session. Since there's no way to tell from the
  // vendor string, we check for known virtual input devices.
  // TODO(lambroslambrou): Find a similar way to determine that the *output* is
  // secure.
  if (!connection->xinput().present()) {
    // If XInput is not available, assume it is not a virtual session.
    LOG(ERROR) << "X Input extension not available";
    return false;
  }

  auto devices = connection->xinput().ListInputDevices().Sync();
  if (!devices) {
    LOG(ERROR) << "ListInputDevices failed";
    return false;
  }

  bool found_xvfb_mouse = false;
  bool found_xvfb_keyboard = false;
  bool found_crd_void_input = false;
  bool found_other_devices = false;
  for (size_t i = 0; i < devices->devices.size(); i++) {
    const auto& device_info = devices->devices[i];
    const std::string& name = devices->names[i].name;
    if (device_info.device_use == x11::Input::DeviceUse::IsXExtensionPointer) {
      if (name == "Xvfb mouse") {
        found_xvfb_mouse = true;
      } else if (name == "Chrome Remote Desktop Input") {
        found_crd_void_input = true;
      } else if (name != "Virtual core XTEST pointer") {
        found_other_devices = true;
        HOST_LOG << "Non-virtual mouse found: " << name;
      }
    } else if (device_info.device_use ==
               x11::Input::DeviceUse::IsXExtensionKeyboard) {
      if (name == "Xvfb keyboard") {
        found_xvfb_keyboard = true;
      } else if (name != "Virtual core XTEST keyboard") {
        found_other_devices = true;
        HOST_LOG << "Non-virtual keyboard found: " << name;
      }
    } else if (device_info.device_use == x11::Input::DeviceUse::IsXPointer) {
      if (name != "Virtual core pointer") {
        found_other_devices = true;
        HOST_LOG << "Non-virtual mouse found: " << name;
      }
    } else if (device_info.device_use == x11::Input::DeviceUse::IsXKeyboard) {
      if (name != "Virtual core keyboard") {
        found_other_devices = true;
        HOST_LOG << "Non-virtual keyboard found: " << name;
      }
    } else {
      found_other_devices = true;
      HOST_LOG << "Non-virtual device found: " << name;
    }
  }
  return ((found_xvfb_mouse && found_xvfb_keyboard) || found_crd_void_input) &&
         !found_other_devices;
}

}  // namespace remoting
