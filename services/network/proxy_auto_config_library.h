// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PROXY_AUTO_CONFIG_LIBRARY_H_
#define SERVICES_NETWORK_PROXY_AUTO_CONFIG_LIBRARY_H_

#include <list>
#include <set>

#include "base/component_export.h"
#include "base/containers/queue.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "net/base/completion_once_callback.h"
#include "net/base/ip_address.h"
#include "net/dns/host_resolver_proc.h"
#include "net/dns/host_resolver_system_task.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"

namespace net {
class ClientSocketFactory;
class AddressList;
}  // namespace net

namespace network {

// myIpAddress() is a broken API available to PAC scripts. Do not use these
// outside of PAC as they are broken APIs.
// It has the problematic definition of:
// "Returns the IP address of the host machine."
//
// This has ambiguity on what should happen for multi-homed hosts which may have
// multiple IP addresses to choose from. To be unambiguous we would need to
// know which hosts is going to be connected to, in order to use the outgoing
// IP for that request.
//
// However at this point that is not known, as the proxy still hasn't been
// decided.
//
// The strategy used here is to prioritize the IP address that would be used
// for connecting to the public internet by testing which interface is used for
// connecting to 8.8.8.8 and 2001:4860:4860::8888 (public IPs).
//
// If that fails, we will try resolving the machine's hostname, and also probing
// for routes in the private IP space.
//
// Link-local IP addresses are not generally returned, however may be if no
// other IP was found by the probes.
//
// This class supports being deleted at any time.
class COMPONENT_EXPORT(NETWORK_SERVICE) MyIpAddressImpl {
 public:
  enum class Mode {
    kMyIpAddress,
    kMyIpAddressEx,
  };

  explicit MyIpAddressImpl(Mode mode);
  ~MyIpAddressImpl();

  MyIpAddressImpl(const MyIpAddressImpl&) = delete;
  MyIpAddressImpl& operator=(const MyIpAddressImpl&) = delete;

  // Adds `my_ip_address_client` to the list of clients waiting for a list of
  // IP candidates. If no search for candidates is currently ongoing, a search
  // according to `mode_` is started which will run asynchronously on the
  // current thread.
  void AddRequest(
      mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient>
          my_ip_address_client);

  // Used for mocking the socket dependency.
  void SetSocketFactoryForTest(net::ClientSocketFactory* socket_factory);

  // Used for mocking the DNS dependency.
  void SetHostResolverProcForTest(
      scoped_refptr<net::HostResolverProc> host_resolver_proc);

 private:
  // State-machine states.
  enum class State {
    kNone,
    kConnectSocketsPublicInternetRoutes,
    kTestResolvingHostname,
    kConnectSocketsPrivateIpRoutes,
    kSendResultsAndReset,
  };

  // Sockets that are created and connected for the purpose of probing their
  // source addresses.
  struct SocketConnectionResult;

  // Try to make progress gathering IP candidates.
  void DoLoop();

  int DoConnectSocketsPublicInternetRoutes();
  int DoTestResolvingHostname();
  int DoConnectSocketsPrivateIPRoutes();
  int DoSendResultsAndReset();

  // Resets back to the initial state. Clears all candidate IPs, clears the
  // RemoteSet, and invalidates any weak pointers to cancel any pending
  // callbacks.
  void Reset();

  // Callback for a disconnected Remote in `my_ip_address_clients_`. If no
  // clients are left, calls Reset() to cancel the current candidate search.
  void ClientDisconnected(mojo::RemoteSetElementId id);

  // DoConnectSocketsPublicInternetRoutes() and
  // DoConnectSocketsPrivateIPRoutes() both test what source IP addresses would
  // be used for sending a UDP packet to a set of destination IPs. This function
  // connects sockets stored in `socket_results_` to the given
  // `destination_ips`. The sockets are stored in the same order as
  // `destination_ips` so sockets_results_[0] is connected to
  // destination_ips[0], etc.
  // Then returns the result of DoProcessConnectedSockets().
  int DoConnectSockets(std::vector<net::IPAddress> destination_ips);

  // Loops through the connected sockets in `socket_results_` (in order) and
  // gets the local address associated with the socket. The local address is
  // Add()ed and if the search is `done_`, returns immediately with net::OK.
  // If the loop encounters a socket that has not yet connected, this function
  // will return with net::ERR_IO_PENDING. This function can then be called
  // again later and will not reconsider already-processed sockets.
  // If all sockets in `socket_results_` are processed, calls
  // MarkAsDoneIfHaveCandidates().
  int DoProcessConnectedSockets();

  // Called whenever a socket is connect asynchronously. `socket_result` should
  // always be in `socket_results_` unless the WeakPtr associated with this
  // callback is invalidated.
  // Stores `result` in `socket_result` and calls DoProcessConnectedSockets().
  // If the result is not ERR_IO_PENDING, re-enters DoLoop().
  void OnConnectedSocket(SocketConnectionResult& socket_result, int result);

  // A callback for SystemHostResolverCallAsync(). Add()s all the IPs in
  // `addr_list` if `net_error` is net::OK, and then returns to the state
  // machine by calling DoLoop().
  void ReceiveDnsResults(
      std::unique_ptr<net::HostResolverSystemTask> system_dns_resolution_task,
      const net::AddressList& addr_list,
      int os_error,
      int net_error);

  // Adds `address` to the results if appropriate. Can add the IP to either
  // `candidate_ips_` or `link_local_ips_`.
  void Add(const net::IPAddress& address);

  // If candidate IPs have been found, marks the current search as done.
  // If `done_` is true then also sets `next_state_` to kSendResultsAndReset.
  //
  // This is used to stop exploring for IPs if any of the high-level tests find
  // a match (i.e. either the public internet route test, or hostname test, or
  // private route test found something).
  //
  // In the case of myIpAddressEx() this means it will be conservative in which
  // IPs it returns and not enumerate the full set. See http://crbug.com/905366
  // before expanding that policy.
  void MarkAsDoneIfHaveCandidates();

  net::IPAddressList GetResultForMyIpAddress() const;
  net::IPAddressList GetResultForMyIpAddressEx() const;

  static net::IPAddressList GetSingleResultFavoringIPv4(
      const net::IPAddressList& ips);

  // The operation being carried out.
  const Mode mode_;

  // A set of clients waiting for the search to conclude.
  mojo::RemoteSet<proxy_resolver::mojom::HostResolverRequestClient>
      my_ip_address_clients_;

  // Tracks current state of the state machine.
  State next_state_ = State::kNone;

  // Tracks all IPs seen so far so that duplicates don't end up in the returned
  // results.
  std::set<net::IPAddress> seen_ips_;

  // Sockets used to probe public and private routes for the machine's IP
  // address. These are considered in order.
  std::list<SocketConnectionResult> socket_results_;

  // The preferred ordered candidate IPs so far.
  net::IPAddressList candidate_ips_;

  // The link-local IP addresses seen so far (not part of `candidate_ips_`).
  net::IPAddressList link_local_ips_;

  // Whether the search for results has completed.
  //
  // Once "done", calling Add() will not change the final result. This is used
  // to short-circuit early.
  bool done_ = false;

  raw_ptr<net::ClientSocketFactory> override_socket_factory_ = nullptr;
  scoped_refptr<net::HostResolverProc> host_resolver_proc_ = nullptr;

  base::WeakPtrFactory<MyIpAddressImpl> weak_ptr_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_PROXY_AUTO_CONFIG_LIBRARY_H_
