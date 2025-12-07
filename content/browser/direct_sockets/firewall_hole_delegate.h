// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DIRECT_SOCKETS_FIREWALL_HOLE_DELEGATE_H_
#define CONTENT_BROWSER_DIRECT_SOCKETS_FIREWALL_HOLE_DELEGATE_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/base/ip_address.h"
#include "services/network/public/mojom/socket_connection_tracker.mojom.h"

namespace net {
class IPEndPoint;
}  // namespace net

namespace chromeos {
class FirewallHole;
}  // namespace chromeos

namespace content {

// For multicast capabilities, the same port of a UDP socket may be reused with
// allowAddressSharing = true. A firewall hole is reused for connections on
// the same port.
struct CONTENT_EXPORT FirewallHoleTracker {
  FirewallHoleTracker(const FirewallHoleTracker&) = delete;
  FirewallHoleTracker& operator=(const FirewallHoleTracker&) = delete;

  FirewallHoleTracker(std::unique_ptr<chromeos::FirewallHole> firewall_hole,
                      size_t usage_count);
  ~FirewallHoleTracker();

  FirewallHoleTracker(FirewallHoleTracker&&);
  FirewallHoleTracker& operator=(FirewallHoleTracker&&);

  std::unique_ptr<chromeos::FirewallHole> firewall_hole;
  size_t usage_count;
};

// This class inherits from SocketConnectionTracker so that all stored firewall
// hole handles reference |this| in the internal ReceiverSet. It must live for
// the whole browser lifecycle because firewall holes can be requested by
// different web apps.
class CONTENT_EXPORT FirewallHoleDelegate
    : public network::mojom::SocketConnectionTracker {
 public:
  using LocalPort = uint16_t;
  using OpenSocketCallback =
      base::OnceCallback<void(int32_t, const std::optional<net::IPEndPoint>&)>;

  static void SetAlwaysOpenFirewallHoleForTesting(bool value);

  FirewallHoleDelegate();
  ~FirewallHoleDelegate() override;

  FirewallHoleDelegate(const FirewallHoleDelegate&) = delete;
  FirewallHoleDelegate& operator=(const FirewallHoleDelegate&) = delete;

  static void OpenTCPFirewallHole(
      mojo::PendingReceiver<network::mojom::SocketConnectionTracker>
          connection_tracker,
      OpenSocketCallback callback,
      int32_t result,
      const std::optional<net::IPEndPoint>& local_addr);

  static void OpenUDPFirewallHole(
      mojo::PendingReceiver<network::mojom::SocketConnectionTracker>
          connection_tracker,
      OpenSocketCallback callback,
      int32_t result,
      const std::optional<net::IPEndPoint>& local_addr);

 private:
  static FirewallHoleDelegate* GetInstance();

  bool ShouldOpenFirewallHole(const net::IPAddress& address);

  void OpenTCPFirewallHoleImpl(
      mojo::PendingReceiver<network::mojom::SocketConnectionTracker>
          connection_tracker,
      OpenSocketCallback callback,
      int32_t result,
      const std::optional<net::IPEndPoint>& local_addr);

  void OpenUDPFirewallHoleImpl(
      mojo::PendingReceiver<network::mojom::SocketConnectionTracker>
          connection_tracker,
      OpenSocketCallback callback,
      int32_t result,
      const std::optional<net::IPEndPoint>& local_addr);

  void OnUDPFirewallHoleOpened(
      mojo::PendingReceiver<network::mojom::SocketConnectionTracker>
          connection_tracker,
      const net::IPEndPoint& local_addr,
      std::unique_ptr<chromeos::FirewallHole> firewall_hole);

  void OnTCPFirewallHoleOpened(
      mojo::PendingReceiver<network::mojom::SocketConnectionTracker>
          connection_tracker,
      base::OnceClosure on_success,
      base::OnceClosure on_failure,
      std::unique_ptr<chromeos::FirewallHole> firewall_hole);

  void OnUdpConnectionTrackerDisconnected();

  mojo::ReceiverSet<network::mojom::SocketConnectionTracker,
                    std::unique_ptr<chromeos::FirewallHole>>
      tcp_receivers_;

  // This map tracks UDP socket callbacks so that
  // UDP sockets created in parallel can share the same firewall hole.
  base::flat_map<LocalPort, std::vector<OpenSocketCallback>>
      pending_udp_open_socket_requests_;

  mojo::ReceiverSet<network::mojom::SocketConnectionTracker, LocalPort>
      udp_receivers_;

  base::flat_map<LocalPort, FirewallHoleTracker> udp_firewall_holes_;

  bool always_open_firewall_hole_for_testing = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DIRECT_SOCKETS_FIREWALL_HOLE_DELEGATE_H_
