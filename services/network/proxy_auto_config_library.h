// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PROXY_AUTO_CONFIG_LIBRARY_H_
#define SERVICES_NETWORK_PROXY_AUTO_CONFIG_LIBRARY_H_

#include <set>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "net/base/ip_address.h"
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
class COMPONENT_EXPORT(NETWORK_SERVICE) MyIpAddressImpl
    : public base::RefCountedThreadSafe<MyIpAddressImpl> {
 public:
  enum class Mode {
    kMyIpAddress,
    kMyIpAddressEx,
  };

  explicit MyIpAddressImpl(Mode mode);

  MyIpAddressImpl(const MyIpAddressImpl&) = delete;
  MyIpAddressImpl& operator=(const MyIpAddressImpl&) = delete;

  // Adds |my_ip_address_client| to the list of clients waiting for a list of
  // IP candidates. If no search for candidates is currently ongoing, a search
  // according to |mode_| is started on a worker thread.
  void AddRequest(
      mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient>
          my_ip_address_client);

  // Used for mocking the socket dependency.
  void SetSocketFactoryForTest(net::ClientSocketFactory* socket_factory);

  // Used for mocking the DNS dependency.
  void SetDNSResultForTest(const net::AddressList& addrs);

 private:
  friend base::RefCountedThreadSafe<network::MyIpAddressImpl>;
  ~MyIpAddressImpl();

  void AddRequestOnWorkerThread(
      mojo::PendingRemote<proxy_resolver::mojom::HostResolverRequestClient>
          my_ip_address_client);

  void SendResultsAndReset();

  // Adds `address` to the results if appropriate. Can add the IP to either
  // `candidate_ips_` or `link_local_ips_`.
  void Add(const net::IPAddress& address);

  // Marks the current search as done if candidate IPs have been found.
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

  void TestRoute(const net::IPAddress& destination_ip);
  void TestPublicInternetRoutes();
  void TestPrivateIPRoutes();
  void TestResolvingHostname();

  static net::IPAddressList GetSingleResultFavoringIPv4(
      const net::IPAddressList& ips);

  // The whole class except the constructor and Run() runs on the
  // worker_task_runner_.
  scoped_refptr<base::SequencedTaskRunner> worker_task_runner_;

  // The operation being carried out.
  const Mode mode_;

  // A set of clients waiting for the search to conclude.
  mojo::RemoteSet<proxy_resolver::mojom::HostResolverRequestClient>
      my_ip_address_clients_;

  // Tracks all IPs seen so far so that duplicates don't end up in the returned
  // results.
  std::set<net::IPAddress> seen_ips_;

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
  std::unique_ptr<net::AddressList> override_dns_result_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PROXY_AUTO_CONFIG_LIBRARY_H_
