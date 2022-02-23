// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/curtain_mode.h"

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/base/logging.h"
#include "remoting/host/client_session_control.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/xinput.h"
#include "ui/gfx/x/xproto_types.h"

namespace remoting {

class CurtainModeLinux : public CurtainMode {
 public:
  CurtainModeLinux();

  CurtainModeLinux(const CurtainModeLinux&) = delete;
  CurtainModeLinux& operator=(const CurtainModeLinux&) = delete;

  // Overriden from CurtainMode.
  bool Activate() override;

 private:
  // Returns true if the host is running under a virtual session.
  bool IsVirtualSession();
};

CurtainModeLinux::CurtainModeLinux() = default;

bool CurtainModeLinux::Activate() {
  // We can't curtain the session in run-time in Linux.
  // Either the session is running in a virtual session (i.e. always curtained),
  // or it is attached to the physical console (i.e. impossible to curtain).
  if (!IsVirtualSession()) {
    LOG(ERROR) << "Curtain-mode is not supported when running on non-virtual "
                  "X server";
    return false;
  }

  return true;
}

bool CurtainModeLinux::IsVirtualSession() {
  // Try to identify a virtual session. Since there's no way to tell from the
  // vendor string, we check for known virtual input devices.
  // TODO(rmsousa): Find a similar way to determine that the *output* is secure.
  x11::Connection* connection = x11::Connection::Get();
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

// static
std::unique_ptr<CurtainMode> CurtainMode::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control) {
  return base::WrapUnique(new CurtainModeLinux());
}

}  // namespace remoting
