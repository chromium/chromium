// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_NETWORK_MONITOR_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_NETWORK_MONITOR_HOST_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/base/network_interfaces.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"
#include "services/network/public/cpp/network_connection_tracker.h"

namespace content {

class BrowserPpapiHostImpl;

// The host for PPB_NetworkMonitor. This class lives on the IO thread.
class PepperNetworkMonitorHost
    : public ppapi::host::ResourceHost,
      public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  PepperNetworkMonitorHost(BrowserPpapiHostImpl* host,
                           PP_Instance instance,
                           PP_Resource resource);

  PepperNetworkMonitorHost(const PepperNetworkMonitorHost&) = delete;
  PepperNetworkMonitorHost& operator=(const PepperNetworkMonitorHost&) = delete;

  ~PepperNetworkMonitorHost() override;

  // network::NetworkConnectionTracker::NetworkConnectionObserver interface.
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

 private:
  void OnPermissionCheckResult(bool can_use_network_monitor);
  void SetNetworkConnectionTracker(
      network::NetworkConnectionTracker* network_connection_tracker);

  void GetAndSendNetworkList();
  void SendNetworkList(const net::NetworkInterfaceList& list);

  ppapi::host::ReplyMessageContext reply_context_;

  raw_ptr<network::NetworkConnectionTracker> network_connection_tracker_;

  base::WeakPtrFactory<PepperNetworkMonitorHost> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_NETWORK_MONITOR_HOST_H_
