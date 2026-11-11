// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/daemon_controller.h"

#include <unistd.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/types/expected.h"
#include "remoting/base/username.h"
#include "remoting/host/linux/host_type_environment.h"
#include "remoting/host/linux/host_types.h"
#include "remoting/host/setup/daemon_controller_delegate_linux_multi_process.h"
#include "remoting/host/setup/daemon_controller_delegate_linux_single_process.h"

namespace remoting {

namespace {

const HostType* g_host_type = nullptr;

std::string& GetLocalUserName() {
  static base::NoDestructor<std::string> local_user_name;
  return *local_user_name;
}

// Creates the delegate for `host_type`. The other parameters are only used for
// a multi-process `host_type`, see DaemonControllerDelegateLinuxMultiProcess.
// If `env` is null, a new one is created for a multi-process `host_type`.
std::unique_ptr<DaemonController::Delegate> CreateDelegate(
    const HostType& host_type,
    std::string_view local_user_name,
    std::unique_ptr<HostTypeEnvironment> env) {
  if (!host_type.is_multi_process()) {
    return std::make_unique<DaemonControllerDelegateLinuxSingleProcess>();
  }
  return std::make_unique<DaemonControllerDelegateLinuxMultiProcess>(
      host_type, local_user_name,
      env ? std::move(env) : HostTypeEnvironment::Create());
}

}  // namespace

// static
void DaemonController::SetHostTypeForSetup(const HostType& type,
                                           std::string_view local_user_name) {
  g_host_type = &type;
  GetLocalUserName() = local_user_name;
}

// static
scoped_refptr<DaemonController> DaemonController::Create() {
  if (g_host_type) {
    return base::MakeRefCounted<DaemonController>(
        CreateDelegate(*g_host_type, GetLocalUserName(), /*env=*/nullptr));
  }

  // No host type was set for setup (e.g. by the native messaging host), so the
  // configured host type is used. The current user is only used to find a
  // single-process host without a host type record. As root (e.g. in the
  // native messaging host's pkexec child), it is not used, since root has no
  // single-process host.
  std::string local_user_name;
  if (getuid() != 0) {
    local_user_name = GetUsername();
  }
  std::unique_ptr<HostTypeEnvironment> env = HostTypeEnvironment::Create();
  const HostType* configured_host_type;
  base::expected<const HostType*, std::string> configured =
      HostType::GetConfigured(*env, local_user_name);
  if (configured.has_value()) {
    configured_host_type = *configured;
  } else {
    // GetConfigured() only falls back to GetFallbackConfigured() if there is
    // no host type record. An invalid or insecure record is an error, so that
    // the setup tools fail and say how to fix it, but the native messaging
    // host should keep working, so it falls back here.
    LOG(ERROR) << configured.error();
    configured_host_type =
        HostType::GetFallbackConfigured(*env, local_user_name);
  }
  const HostType& host_type = configured_host_type
                                  ? *configured_host_type
                                  : HostType::GetDefaultHostType();
  // A multi-process delegate only uses the local user for the start hook,
  // which it only runs as root, where the current user isn't used (see
  // above). So no local user is passed.
  return base::MakeRefCounted<DaemonController>(
      CreateDelegate(host_type, /*local_user_name=*/"", std::move(env)));
}

}  // namespace remoting
