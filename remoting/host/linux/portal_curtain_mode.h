// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_PORTAL_CURTAIN_MODE_H_
#define REMOTING_HOST_LINUX_PORTAL_CURTAIN_MODE_H_

#include "base/memory/weak_ptr.h"
#include "remoting/host/curtain_mode.h"

namespace remoting {

class ClientSessionControl;

class PortalCurtainMode : public CurtainMode {
 public:
  explicit PortalCurtainMode(
      base::WeakPtr<ClientSessionControl> client_session_control) {}
  PortalCurtainMode& operator=(const PortalCurtainMode&) = delete;
  ~PortalCurtainMode() override = default;

  // CurtainMode interface.
  bool Activate() override;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_PORTAL_CURTAIN_MODE_H_
