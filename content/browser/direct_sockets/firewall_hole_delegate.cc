// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/direct_sockets/firewall_hole_delegate.h"

#include <utility>

#include "base/check.h"
#include "base/containers/map_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "chromeos/components/firewall_hole/firewall_hole.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"

namespace content {

namespace {

// Runs the supplied `callback` with `net_error` and default params for other
// args.
template <typename... Args>
void FulfillWithError(base::OnceCallback<void(int32_t, Args...)> callback,
                      int32_t net_error) {
  std::move(callback).Run(net_error, std::remove_cvref_t<Args>()...);
}

}  // namespace

FirewallHoleTracker::FirewallHoleTracker(
    std::unique_ptr<chromeos::FirewallHole> firewall_hole,
    size_t usage_count)
    : firewall_hole(std::move(firewall_hole)), usage_count(usage_count) {}

FirewallHoleTracker::FirewallHoleTracker(FirewallHoleTracker&&) = default;
FirewallHoleTracker& FirewallHoleTracker::operator=(FirewallHoleTracker&&) =
    default;

FirewallHoleTracker::~FirewallHoleTracker() = default;

// static
FirewallHoleDelegate* FirewallHoleDelegate::GetInstance() {
  static base::NoDestructor<FirewallHoleDelegate> instance;
  return instance.get();
}

bool FirewallHoleDelegate::ShouldOpenFirewallHole(
    const net::IPAddress& address) {
  if (always_open_firewall_hole_for_testing) {
    return true;
  }
  return !address.IsLoopback();
}

// static
void FirewallHoleDelegate::SetAlwaysOpenFirewallHoleForTesting(bool value) {
  GetInstance()->always_open_firewall_hole_for_testing = value;
}

FirewallHoleDelegate::FirewallHoleDelegate() {
  udp_receivers_.set_disconnect_handler(base::BindRepeating(
      &FirewallHoleDelegate::OnUdpConnectionTrackerDisconnected,
      base::Unretained(this)));
}

FirewallHoleDelegate::~FirewallHoleDelegate() = default;

// static
void FirewallHoleDelegate::OpenTCPFirewallHole(
    mojo::PendingReceiver<network::mojom::SocketConnectionTracker>
        connection_tracker,
    OpenSocketCallback callback,
    int32_t result,
    const std::optional<net::IPEndPoint>& local_addr) {
  GetInstance()->OpenTCPFirewallHoleImpl(std::move(connection_tracker),
                                         std::move(callback), std::move(result),
                                         local_addr);
}

// static
void FirewallHoleDelegate::OpenUDPFirewallHole(
    mojo::PendingReceiver<network::mojom::SocketConnectionTracker>
        connection_tracker,
    OpenSocketCallback callback,
    int32_t result,
    const std::optional<net::IPEndPoint>& local_addr) {
  GetInstance()->OpenUDPFirewallHoleImpl(std::move(connection_tracker),
                                         std::move(callback), std::move(result),
                                         local_addr);
}

void FirewallHoleDelegate::OpenTCPFirewallHoleImpl(
    mojo::PendingReceiver<network::mojom::SocketConnectionTracker>
        connection_tracker,
    OpenSocketCallback callback,
    int32_t result,
    const std::optional<net::IPEndPoint>& local_addr) {
  if (result != net::OK) {
    FulfillWithError(std::move(callback), result);
    return;
  }
  if (!ShouldOpenFirewallHole(local_addr->address())) {
    std::move(callback).Run(net::OK, *local_addr);
    return;
  }
  auto [callback_a, callback_b] = base::SplitOnceCallback(std::move(callback));
  chromeos::FirewallHole::Open(
      chromeos::FirewallHole::PortType::kTcp, local_addr->port(),
      "" /*all interfaces*/,
      base::BindOnce(
          &FirewallHoleDelegate::OnTCPFirewallHoleOpened,
          base::Unretained(this), std::move(connection_tracker),
          /*on_success=*/
          base::BindOnce(std::move(callback_a), net::OK, *local_addr),
          /*on_failure=*/
          base::BindOnce(std::move(callback_b), net::ERR_NETWORK_ACCESS_DENIED,
                         std::nullopt)));
}

void FirewallHoleDelegate::OpenUDPFirewallHoleImpl(
    mojo::PendingReceiver<network::mojom::SocketConnectionTracker>
        connection_tracker,
    OpenSocketCallback callback,
    int32_t result,
    const std::optional<net::IPEndPoint>& local_addr) {
  if (result != net::OK) {
    FulfillWithError(std::move(callback), result);
    return;
  }
  if (!ShouldOpenFirewallHole(local_addr->address())) {
    std::move(callback).Run(net::OK, *local_addr);
    return;
  }

  if (auto* hole_tracker =
          base::FindOrNull(udp_firewall_holes_, local_addr->port())) {
    hole_tracker->usage_count += 1;
    udp_receivers_.Add(this, std::move(connection_tracker), local_addr->port());
    std::move(callback).Run(net::OK, *local_addr);
    return;
  }

  auto& pending_callbacks =
      pending_udp_open_socket_requests_[local_addr->port()];
  pending_callbacks.push_back(std::move(callback));

  // Only initiate a new FirewallHole::Open request if this is the first
  // request for this port.
  if (pending_callbacks.size() == 1) {
    chromeos::FirewallHole::Open(
        chromeos::FirewallHole::PortType::kUdp, local_addr->port(),
        "" /*all interfaces*/,
        base::BindOnce(&FirewallHoleDelegate::OnUDPFirewallHoleOpened,
                       base::Unretained(this), std::move(connection_tracker),
                       *local_addr));
  }
}

void FirewallHoleDelegate::OnUDPFirewallHoleOpened(
    mojo::PendingReceiver<network::mojom::SocketConnectionTracker>
        connection_tracker,
    const net::IPEndPoint& local_addr,
    std::unique_ptr<chromeos::FirewallHole> firewall_hole) {
  auto find_pending_callbacks =
      pending_udp_open_socket_requests_.find(local_addr.port());
  CHECK(find_pending_callbacks != pending_udp_open_socket_requests_.end());
  CHECK(!find_pending_callbacks->second.empty());
  auto pending_callbacks = std::move(find_pending_callbacks->second);
  pending_udp_open_socket_requests_.erase(find_pending_callbacks);

  if (!firewall_hole) {
    for (auto& callback : pending_callbacks) {
      FulfillWithError(std::move(callback), net::ERR_NETWORK_ACCESS_DENIED);
    }
    return;
  }

  FirewallHoleTracker tracker{
      std::move(firewall_hole),
      // The number of pending callbacks corresponds to the number of UDP
      // socket instances created for this port.
      pending_callbacks.size()};

  udp_receivers_.Add(this, std::move(connection_tracker), local_addr.port());
  udp_firewall_holes_.emplace(local_addr.port(), std::move(tracker));

  for (auto& callback : pending_callbacks) {
    std::move(callback).Run(net::OK, local_addr);
  }
}

void FirewallHoleDelegate::OnTCPFirewallHoleOpened(
    mojo::PendingReceiver<network::mojom::SocketConnectionTracker>
        connection_tracker,
    base::OnceClosure on_success,
    base::OnceClosure on_failure,
    std::unique_ptr<chromeos::FirewallHole> firewall_hole) {
  if (!firewall_hole) {
    std::move(on_failure).Run();
    return;
  }
  tcp_receivers_.Add(this, std::move(connection_tracker),
                     std::move(firewall_hole));
  std::move(on_success).Run();
}

void FirewallHoleDelegate::OnUdpConnectionTrackerDisconnected() {
  LocalPort local_port = udp_receivers_.current_context();
  auto find_hole_tracker = udp_firewall_holes_.find(local_port);
  CHECK(find_hole_tracker != udp_firewall_holes_.end());
  find_hole_tracker->second.usage_count -= 1;

  if (find_hole_tracker->second.usage_count == 0) {
    udp_firewall_holes_.erase(find_hole_tracker);
  }
}

}  // namespace content
