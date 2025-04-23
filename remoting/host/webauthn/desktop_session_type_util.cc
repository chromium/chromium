// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/webauthn/desktop_session_type_util.h"

#include "base/environment.h"
#include "build/build_config.h"
#include "remoting/base/constants.h"

namespace remoting {
namespace {

#if BUILDFLAG(IS_LINUX)
DesktopSessionType GetDesktopSessionTypeInternal(
    std::unique_ptr<base::Environment> environment) {
  // Currently on Linux, a desktop session is either remote-only or local-only.
  // The former has the environment while the latter doesn't. This may change
  // when we move to Wayland.
  // TODO(yuweih): If this no longer works, just check whether the device is a
  // GCE VM. For GCE VMs, /sys/class/dmi/id/product_serial starts with
  // "GoogleCloud-".
  return environment->HasVar(kChromeRemoteDesktopSessionEnvVar)
             ? DesktopSessionType::REMOTE_ONLY
             : DesktopSessionType::LOCAL_ONLY;
}
#endif

}  // namespace

DesktopSessionType GetDesktopSessionType() {
#if BUILDFLAG(IS_LINUX)
  static const DesktopSessionType desktop_session_type =
      GetDesktopSessionTypeInternal(base::Environment::Create());
  return desktop_session_type;
#else
  return DesktopSessionType::UNSPECIFIED;
#endif
}

}  // namespace remoting
