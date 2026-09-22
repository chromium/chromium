// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_monitor/local_input_monitor_x11_common.h"

#include "base/containers/span.h"
#include "base/logging.h"
#include "ui/gfx/x/future.h"

namespace remoting {

x11::Input::XIEventMask CommonXIEventMaskForRootWindow() {
  x11::Input::XIEventMask mask{};
  ui::SetXinputMask(base::byte_span_from_ref(mask),
                    x11::Input::RawDeviceEvent::RawKeyPress);
  ui::SetXinputMask(base::byte_span_from_ref(mask),
                    x11::Input::RawDeviceEvent::RawKeyRelease);
  ui::SetXinputMask(base::byte_span_from_ref(mask),
                    x11::Input::RawDeviceEvent::RawMotion);
  return mask;
}

base::flat_set<x11::Input::DeviceId> GetXTestDeviceIds(
    x11::Connection* connection) {
  base::flat_set<x11::Input::DeviceId> xtest_device_ids;
  auto reply =
      connection->xinput().XIQueryDevice({x11::Input::DeviceId::All}).Sync();
  if (!reply) {
    LOG(ERROR) << "XIQueryDevice failed.";
    return xtest_device_ids;
  }
  for (const auto& info : reply->infos) {
    if (info.name.ends_with("XTEST keyboard") ||
        info.name.ends_with("XTEST pointer")) {
      xtest_device_ids.insert(info.deviceid);
    }
  }
  return xtest_device_ids;
}

}  // namespace remoting
