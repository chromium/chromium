// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/p2p/ipc_network_manager.h"

#include <string>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "components/webrtc/net_address_utils.h"
#include "net/base/ip_address.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_interfaces.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/webrtc/rtc_base/socket_address.h"

namespace blink {

namespace {

rtc::AdapterType ConvertConnectionTypeToAdapterType(
    net::NetworkChangeNotifier::ConnectionType type) {
  switch (type) {
    case net::NetworkChangeNotifier::CONNECTION_UNKNOWN:
      return rtc::ADAPTER_TYPE_UNKNOWN;
    case net::NetworkChangeNotifier::CONNECTION_ETHERNET:
      return rtc::ADAPTER_TYPE_ETHERNET;
    case net::NetworkChangeNotifier::CONNECTION_WIFI:
      return rtc::ADAPTER_TYPE_WIFI;
    case net::NetworkChangeNotifier::CONNECTION_2G:
    case net::NetworkChangeNotifier::CONNECTION_3G:
    case net::NetworkChangeNotifier::CONNECTION_4G:
    case net::NetworkChangeNotifier::CONNECTION_5G:
      return rtc::ADAPTER_TYPE_CELLULAR;
    default:
      return rtc::ADAPTER_TYPE_UNKNOWN;
  }
}

}  // namespace

IpcNetworkManager::IpcNetworkManager(
    blink::NetworkListManager* network_list_manager,
    std::unique_ptr<webrtc::MdnsResponderInterface> mdns_responder)
    : network_list_manager_(network_list_manager),
      mdns_responder_(std::move(mdns_responder)) {
  DETACH_FROM_THREAD(thread_checker_);
  network_list_manager->AddNetworkListObserver(this);
}

IpcNetworkManager::~IpcNetworkManager() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!network_list_manager_);
}

void IpcNetworkManager::ContextDestroyed() {
  DCHECK(network_list_manager_);
  network_list_manager_->RemoveNetworkListObserver(this);
  network_list_manager_ = nullptr;
}

base::WeakPtr<IpcNetworkManager>
IpcNetworkManager::AsWeakPtrForSignalingThread() {
  return weak_factory_.GetWeakPtr();
}

void IpcNetworkManager::StartUpdating() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (network_list_received_) {
    // Post a task to avoid reentrancy.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, WTF::BindOnce(&IpcNetworkManager::SendNetworksChangedSignal,
                                 weak_factory_.GetWeakPtr()));
  } else {
    VLOG(1) << "IpcNetworkManager::StartUpdating called; still waiting for "
               "network list from browser process.";
  }
  ++start_count_;
}

void IpcNetworkManager::StopUpdating() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_GT(start_count_, 0);
  --start_count_;
}

void IpcNetworkManager::OnNetworkListChanged(
    const net::NetworkInterfaceList& list,
    const net::IPAddress& default_ipv4_local_address,
    const net::IPAddress& default_ipv6_local_address) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Update flag if network list received for the first time.
  if (!network_list_received_) {
    VLOG(1) << "IpcNetworkManager received network list from browser process "
               "for the first time.";
    network_list_received_ = true;
  }

  // Default addresses should be set only when they are in the filtered list of
  // network addresses.
  bool use_default_ipv4_address = false;
  bool use_default_ipv6_address = false;

  // rtc::Network uses these prefix_length to compare network
  // interfaces discovered.
  std::vector<std::unique_ptr<rtc::Network>> networks;
  for (auto it = list.begin(); it != list.end(); it++) {
    rtc::IPAddress ip_address = webrtc::NetIPAddressToRtcIPAddress(it->address);
    DCHECK(!ip_address.IsNil());

    rtc::IPAddress prefix = rtc::TruncateIP(ip_address, it->prefix_length);
    rtc::AdapterType adapter_type =
        ConvertConnectionTypeToAdapterType(it->type);
    // If the adapter type is unknown, try to guess it using WebRTC's string
    // matching rules.
    if (adapter_type == rtc::ADAPTER_TYPE_UNKNOWN) {
      adapter_type = rtc::GetAdapterTypeFromName(it->name.c_str());
    }
    rtc::AdapterType underlying_adapter_type = rtc::ADAPTER_TYPE_UNKNOWN;
    if (it->mac_address.has_value() && IsVpnMacAddress(*it->mac_address)) {
      adapter_type = rtc::ADAPTER_TYPE_VPN;
      // With MAC-based detection we do not know the
      // underlying adapter type.
      underlying_adapter_type = rtc::ADAPTER_TYPE_UNKNOWN;
    }
    auto network = CreateNetwork(it->name, it->name, prefix, it->prefix_length,
                                 adapter_type);
    if (adapter_type == rtc::ADAPTER_TYPE_VPN) {
      network->set_underlying_type_for_vpn(underlying_adapter_type);
    }
    network->set_default_local_address_provider(this);
    network->set_mdns_responder_provider(this);

    rtc::InterfaceAddress iface_addr;
    if (it->address.IsIPv4()) {
      use_default_ipv4_address |= (default_ipv4_local_address == it->address);
      iface_addr = rtc::InterfaceAddress(ip_address);
    } else {
      DCHECK(it->address.IsIPv6());
      iface_addr = rtc::InterfaceAddress(ip_address, it->ip_address_attributes);

      // Only allow non-link-local, non-loopback, non-deprecated IPv6 addresses
      // which don't contain MAC.
      if (rtc::IPIsMacBased(iface_addr) ||
          (it->ip_address_attributes & net::IP_ADDRESS_ATTRIBUTE_DEPRECATED) ||
          rtc::IPIsLinkLocal(iface_addr) || rtc::IPIsLoopback(iface_addr)) {
        continue;
      }

      // On Fuchsia skip private IPv6 addresses as they break some application.
      // TODO(b/350111561): Remove once the applications are updated to handle
      // ULA addresses properly.
#if BUILDFLAG(IS_FUCHSIA)
      if (rtc::IPIsPrivate(iface_addr)) {
        continue;
      }
#endif  // BUILDFLAG(IS_FUCHSIA)

      use_default_ipv6_address |= (default_ipv6_local_address == it->address);
    }
    network->AddIP(iface_addr);
    networks.push_back(std::move(network));
  }

  // Update the default local addresses.
  rtc::IPAddress ipv4_default;
  rtc::IPAddress ipv6_default;
  if (use_default_ipv4_address) {
    ipv4_default =
        webrtc::NetIPAddressToRtcIPAddress(default_ipv4_local_address);
  }
  if (use_default_ipv6_address) {
    ipv6_default =
        webrtc::NetIPAddressToRtcIPAddress(default_ipv6_local_address);
  }
  set_default_local_addresses(ipv4_default, ipv6_default);

  if (Platform::Current()->AllowsLoopbackInPeerConnection()) {
    std::string name_v4("loopback_ipv4");
    rtc::IPAddress ip_address_v4(INADDR_LOOPBACK);
    auto network_v4 = CreateNetwork(name_v4, name_v4, ip_address_v4, 32,
                                    rtc::ADAPTER_TYPE_UNKNOWN);
    network_v4->set_default_local_address_provider(this);
    network_v4->set_mdns_responder_provider(this);
    network_v4->AddIP(ip_address_v4);
    networks.push_back(std::move(network_v4));

    rtc::IPAddress ipv6_default_address;
    // Only add IPv6 loopback if we can get default local address for IPv6. If
    // we can't, it means that we don't have IPv6 enabled on this machine and
    // bind() to the IPv6 loopback address will fail.
    if (GetDefaultLocalAddress(AF_INET6, &ipv6_default_address)) {
      DCHECK(!ipv6_default_address.IsNil());
      std::string name_v6("loopback_ipv6");
      rtc::IPAddress ip_address_v6(in6addr_loopback);
      auto network_v6 = CreateNetwork(name_v6, name_v6, ip_address_v6, 64,
                                      rtc::ADAPTER_TYPE_UNKNOWN);
      network_v6->set_default_local_address_provider(this);
      network_v6->set_mdns_responder_provider(this);
      network_v6->AddIP(ip_address_v6);
      networks.push_back(std::move(network_v6));
    }
  }

  bool changed = false;
  NetworkManager::Stats stats;
  MergeNetworkList(std::move(networks), &changed, &stats);
  if (changed)
    SignalNetworksChanged();

  // Send interface counts to UMA.
  UMA_HISTOGRAM_COUNTS_100("WebRTC.PeerConnection.IPv4Interfaces",
                           stats.ipv4_network_count);
  UMA_HISTOGRAM_COUNTS_100("WebRTC.PeerConnection.IPv6Interfaces",
                           stats.ipv6_network_count);
}

webrtc::MdnsResponderInterface* IpcNetworkManager::GetMdnsResponder() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return mdns_responder_.get();
}

void IpcNetworkManager::SendNetworksChangedSignal() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SignalNetworksChanged();
}

}  // namespace blink
