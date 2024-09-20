// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/x11_util.h"

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/types/cxx23_to_underlying.h"
#include "remoting/base/logging.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/randr.h"
#include "ui/gfx/x/xinput.h"
#include "ui/gfx/x/xproto_types.h"
#include "ui/gfx/x/xtest.h"

namespace remoting {

bool IgnoreXServerGrabs(x11::Connection* connection, bool ignore) {
  if (!connection->xtest().present()) {
    return false;
  }

  connection->xtest().GrabControl({ignore});
  return true;
}

bool IsVirtualSession(x11::Connection* connection) {
  // Try to identify a virtual session. Since there's no way to tell from the
  // vendor string, we check for known virtual input devices.
  // For Xorg+video-dummy, there is no input device that is specific to CRD, so
  // we just check if all input devices are virtual and if the display outputs
  // are all DUMMY*.
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
  bool found_other_devices = false;
  for (size_t i = 0; i < devices->devices.size(); i++) {
    const auto& device_info = devices->devices[i];
    const std::string& name = devices->names[i].name;
    if (device_info.device_use == x11::Input::DeviceUse::IsXExtensionPointer) {
      if (name == "Xvfb mouse") {
        found_xvfb_mouse = true;
      } else if (name == "Chrome Remote Desktop Input") {
        // Ignored. This device only exists if xserver-xorg-input-void is
        // installed, which is no longer available since Debian 11, so we can't
        // use it to reliably check if the user is on Xorg+video-dummy.
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
  return ((found_xvfb_mouse && found_xvfb_keyboard) ||
          IsUsingVideoDummyDriver(connection)) &&
         !found_other_devices;
}

bool IsUsingVideoDummyDriver(x11::Connection* connection) {
  if (!connection->randr().present()) {
    // If RANDR is not available, assume it is not using a video dummy driver.
    LOG(ERROR) << "RANDR extension not available";
    return false;
  }

  auto root = connection->default_root();
  auto randr = connection->randr();
  auto resources = randr.GetScreenResourcesCurrent({root}).Sync();
  if (!resources) {
    LOG(ERROR) << "Cannot get screen resources";
    return false;
  }
  if (resources->outputs.empty()) {
    LOG(ERROR) << "RANDR returns no outputs";
    return false;
  }
  bool has_only_dummy_outputs = true;
  for (x11::RandR::Output output : resources->outputs) {
    auto output_info =
        randr
            .GetOutputInfo({.output = output,
                            .config_timestamp = resources->config_timestamp})
            .Sync();
    if (!output_info) {
      LOG(WARNING) << "Cannot get info for output "
                   << base::to_underlying(output);
      continue;
    }
    auto* output_name = reinterpret_cast<char*>(output_info->name.data());
    if (!base::StartsWith(output_name, "DUMMY")) {
      HOST_LOG << "Non-dummy output found: " << output_name;
      has_only_dummy_outputs = false;
      break;
    }
  }
  return has_only_dummy_outputs;
}

x11::Atom GetX11Atom(x11::Connection* connection, const std::string& name) {
  auto reply = connection->InternAtom({false, name}).Sync();
  if (!reply) {
    LOG(ERROR) << "Failed to intern atom " << name;
    return x11::Atom::None;
  }
  return reply->atom;
}

void SetOutputPhysicalSizeInMM(x11::Connection* connection,
                               x11::RandR::Output output,
                               int width,
                               int height) {
  static const x11::Atom width_mm_atom = GetX11Atom(connection, "WIDTH_MM");
  static const x11::Atom height_mm_atom = GetX11Atom(connection, "HEIGHT_MM");
  if (width_mm_atom == x11::Atom::None || height_mm_atom == x11::Atom::None) {
    return;
  }

  auto width_32 = static_cast<uint32_t>(width);
  auto height_32 = static_cast<uint32_t>(height);

  x11::RandR::ChangeOutputPropertyRequest request = {
      .output = output,
      .property = width_mm_atom,
      .type = x11::Atom::INTEGER,
      .format = 32,
      .mode = x11::PropMode::Replace,
      .num_units = 1,
      .data = base::MakeRefCounted<base::RefCountedStaticMemory>(
          base::byte_span_from_ref(width_32)),
  };
  connection->randr().ChangeOutputProperty(request).Sync();

  request.property = height_mm_atom;
  request.data = base::MakeRefCounted<base::RefCountedStaticMemory>(
      base::byte_span_from_ref(height_32));
  connection->randr().ChangeOutputProperty(request).Sync();
}

}  // namespace remoting
