// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IPC_SCREEN_CONTROLS_H_
#define REMOTING_HOST_IPC_SCREEN_CONTROLS_H_

#include "base/memory/ref_counted.h"
#include "remoting/host/base/screen_controls.h"

namespace remoting {

class DesktopSessionProxy;
class ScreenResolution;

class IpcScreenControls : public ScreenControls {
 public:
  explicit IpcScreenControls(
      scoped_refptr<DesktopSessionProxy> desktop_session_proxy);

  IpcScreenControls(const IpcScreenControls&) = delete;
  IpcScreenControls& operator=(const IpcScreenControls&) = delete;

  ~IpcScreenControls() override;

  // SessionController interface.
  void SetScreenResolution(const ScreenResolution& resolution) override;

 private:
  // Wraps the IPC channel to the desktop session agent.
  scoped_refptr<DesktopSessionProxy> desktop_session_proxy_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_IPC_SCREEN_CONTROLS_H_
