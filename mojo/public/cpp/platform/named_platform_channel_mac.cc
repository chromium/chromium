// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/platform/named_platform_channel.h"

#include <mach/port.h>
#include <servers/bootstrap.h>

#include "base/mac/foundation_util.h"
#include "base/mac/mach_logging.h"
#include "base/mac/scoped_mach_port.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "mojo/public/cpp/platform/platform_channel.h"

namespace mojo {

namespace {

std::string GetBootstrapName(const std::string& name) {
  if (name.empty()) {
    return base::StringPrintf("%s.mojo.%llu", base::mac::BaseBundleID(),
                              base::RandUint64());
  }
  return name;
}

}  // namespace

// static
PlatformChannelServerEndpoint NamedPlatformChannel::CreateServerEndpoint(
    const NamedPlatformChannel::Options& options,
    NamedPlatformChannel::ServerName* server_name) {
  const std::string bootstrap_name = GetBootstrapName(options.server_name);
  DCHECK_LT(bootstrap_name.length(),
            static_cast<size_t>(BOOTSTRAP_MAX_NAME_LEN));

  base::mac::ScopedMachReceiveRight receive_right;
  kern_return_t kr = bootstrap_check_in(
      bootstrap_port, bootstrap_name.c_str(),
      base::mac::ScopedMachReceiveRight::Receiver(receive_right).get());
  if (kr != KERN_SUCCESS) {
    BOOTSTRAP_LOG(ERROR, kr) << "bootstrap_check_in " << bootstrap_name;
    return PlatformChannelServerEndpoint();
  }

  // The mpl_qlimit specified here should stay in sync with PlatformChannel.
  mach_port_limits_t limits{};
  limits.mpl_qlimit = MACH_PORT_QLIMIT_LARGE;
  kr = mach_port_set_attributes(
      mach_task_self(), receive_right.get(), MACH_PORT_LIMITS_INFO,
      reinterpret_cast<mach_port_info_t>(&limits), MACH_PORT_LIMITS_INFO_COUNT);
  MACH_LOG_IF(ERROR, kr != KERN_SUCCESS, kr) << "mach_port_set_attributes";

  *server_name = bootstrap_name;
  return PlatformChannelServerEndpoint(
      PlatformHandle(std::move(receive_right)));
}

// static
PlatformChannelEndpoint NamedPlatformChannel::CreateClientEndpoint(
    const NamedPlatformChannel::ServerName& server_name) {
  base::mac::ScopedMachSendRight send_right;
  kern_return_t kr = bootstrap_look_up(
      bootstrap_port, server_name.c_str(),
      base::mac::ScopedMachSendRight::Receiver(send_right).get());
  if (kr != KERN_SUCCESS) {
    BOOTSTRAP_LOG(ERROR, kr) << "bootstrap_look_up " << server_name;
    return PlatformChannelEndpoint();
  }

  return PlatformChannelEndpoint(PlatformHandle(std::move(send_right)));
}

}  // namespace mojo
