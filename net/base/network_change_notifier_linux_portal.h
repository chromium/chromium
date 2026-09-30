// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_CHANGE_NOTIFIER_LINUX_PORTAL_H_
#define NET_BASE_NETWORK_CHANGE_NOTIFIER_LINUX_PORTAL_H_

#include <memory>

#include "base/functional/callback.h"
#include "build/config/linux/dbus/buildflags.h"
#include "net/base/cronet_buildflags.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_change_notifier_linux.h"

namespace dbus {
class Bus;
}  // namespace dbus

namespace net {

class NetworkChangeNotifierLinux::PortalMonitor {
 public:
  using OnlineStateCallback = base::RepeatingCallback<void(PortalOnlineState)>;
  using ConnectionCostCallback =
      base::RepeatingCallback<void(NetworkChangeNotifier::ConnectionCost)>;

  static std::unique_ptr<PortalMonitor> Create(
      dbus::Bus* bus,
      OnlineStateCallback online_state_callback,
      ConnectionCostCallback connection_cost_callback);

  PortalMonitor() = default;
  PortalMonitor(const PortalMonitor&) = delete;
  PortalMonitor& operator=(const PortalMonitor&) = delete;
  virtual ~PortalMonitor() = default;
};

#if !BUILDFLAG(USE_DBUS) || BUILDFLAG(CRONET_BUILD)
inline std::unique_ptr<NetworkChangeNotifierLinux::PortalMonitor>
NetworkChangeNotifierLinux::PortalMonitor::Create(
    dbus::Bus* bus,
    OnlineStateCallback online_state_callback,
    ConnectionCostCallback connection_cost_callback) {
  return nullptr;
}
#endif

}  // namespace net

#endif  // NET_BASE_NETWORK_CHANGE_NOTIFIER_LINUX_PORTAL_H_
