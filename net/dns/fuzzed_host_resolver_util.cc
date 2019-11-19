// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/fuzzed_host_resolver_util.h"

#include <stdint.h>

#include <fuzzer/FuzzedDataProvider.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/address_list.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/dns/context_host_resolver.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_hosts.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver_manager.h"
#include "net/dns/host_resolver_proc.h"
#include "net/dns/mdns_client.h"
#include "net/dns/public/util.h"
#include "net/log/net_log.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/datagram_server_socket.h"
#include "net/socket/fuzzed_socket_factory.h"

namespace net {

namespace {

// Returns a fuzzed non-zero port number.
uint16_t FuzzPort(FuzzedDataProvider* data_provider) {
  return data_provider->ConsumeIntegral<uint16_t>();
}

// Returns a fuzzed IPv4 address.  Can return invalid / reserved addresses.
IPAddress FuzzIPv4Address(FuzzedDataProvider* data_provider) {
  return IPAddress(data_provider->ConsumeIntegral<uint8_t>(),
                   data_provider->ConsumeIntegral<uint8_t>(),
                   data_provider->ConsumeIntegral<uint8_t>(),
                   data_provider->ConsumeIntegral<uint8_t>());
}

// Returns a fuzzed IPv6 address.  Can return invalid / reserved addresses.
IPAddress FuzzIPv6Address(FuzzedDataProvider* data_provider) {
  return IPAddress(data_provider->ConsumeIntegral<uint8_t>(),
                   data_provider->ConsumeIntegral<uint8_t>(),
                   data_provider->ConsumeIntegral<uint8_t>(),
                   data_provider->ConsumeIntegral<uint8_t>(),
                   data_provider->ConsumeIntegral<uint8_t>(),
                   data_provider->ConsumeIntegral<uint8_t>(),
                   data_provider->ConsumeIntegral<uint8_t>(),
                   data_provider->ConsumeIntegral<uint8_t>(),
                   data_provider->ConsumeIntegral<uint8_t>(),
                   data_provider->ConsumeIntegral<uint8_t>(),
                   data_provider->ConsumeIntegral<uint8_t>(),
                   data_provider->ConsumeIntegral<uint8_t>(),
                   data_provider->ConsumeIntegral<uint8_t>(),
                   data_provider->ConsumeIntegral<uint8_t>(),
                   data_provider->ConsumeIntegral<uint8_t>(),
                   data_provider->ConsumeIntegral<uint8_t>());
}

// Returns a fuzzed address, which can be either IPv4 or IPv6.  Can return
// invalid / reserved addresses.
IPAddress FuzzIPAddress(FuzzedDataProvider* data_provider) {
  if (data_provider->ConsumeBool())
    return FuzzIPv4Address(data_provider);
  return FuzzIPv6Address(data_provider);
}

DnsConfig GetFuzzedDnsConfig(FuzzedDataProvider* data_provider) {
  // Fuzz DNS configuration.
  DnsConfig config;

  // Fuzz name servers.
  uint32_t num_nameservers = data_provider->ConsumeIntegralInRange(0, 4);
  for (uint32_t i = 0; i < num_nameservers; ++i) {
    config.nameservers.push_back(
        IPEndPoint(FuzzIPAddress(data_provider), FuzzPort(data_provider)));
  }

  // Fuzz suffix search list.
  switch (data_provider->ConsumeIntegralInRange(0, 3)) {
    case 3:
      config.search.push_back("foo.com");
      FALLTHROUGH;
    case 2:
      config.search.push_back("bar");
      FALLTHROUGH;
    case 1:
      config.search.push_back("com");
      FALLTHROUGH;
    default:
      break;
  }

  net::DnsHosts hosts;
  // Fuzz hosts file.
  uint8_t num_hosts_entries = data_provider->ConsumeIntegral<uint8_t>();
  for (uint8_t i = 0; i < num_hosts_entries; ++i) {
    const char* kHostnames[] = {"foo", "foo.com",   "a.foo.com",
                                "bar", "localhost", "localhost6"};
    const char* hostname = data_provider->PickValueInArray(kHostnames);
    net::IPAddress address = FuzzIPAddress(data_provider);
    config.hosts[net::DnsHostsKey(hostname, net::GetAddressFamily(address))] =
        address;
  }

  config.unhandled_options = data_provider->ConsumeBool();
  config.append_to_multi_label_name = data_provider->ConsumeBool();
  config.randomize_ports = data_provider->ConsumeBool();
  config.ndots = data_provider->ConsumeIntegralInRange(0, 3);
  config.attempts = data_provider->ConsumeIntegralInRange(1, 3);

  // Timeouts don't really work for fuzzing. Even a timeout of 0 milliseconds
  // will be increased after the first timeout, resulting in inconsistent
  // behavior.
  config.timeout = base::TimeDelta::FromDays(10);

  config.rotate = data_provider->ConsumeBool();

  config.use_local_ipv6 = data_provider->ConsumeBool();

  return config;
}

// HostResolverProc that returns a random set of results, and can succeed or
// fail. Must only be run on the thread it's created on.
class FuzzedHostResolverProc : public HostResolverProc {
 public:
  // Can safely be used after the destruction of |data_provider|. This can
  // happen if a request is issued but the code never waits for the result
  // before the test ends.
  explicit FuzzedHostResolverProc(
      base::WeakPtr<FuzzedDataProvider> data_provider)
      : HostResolverProc(nullptr),
        data_provider_(data_provider),
        network_task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

  int Resolve(const std::string& host,
              AddressFamily address_family,
              HostResolverFlags host_resolver_flags,
              AddressList* addrlist,
              int* os_error) override {
    DCHECK(network_task_runner_->BelongsToCurrentThread());

    if (os_error)
      *os_error = 0;

    // If the data provider is no longer avaiable, just fail. The HostResolver
    // has already been deleted by this point, anyways.
    if (!data_provider_)
      return ERR_FAILED;

    AddressList result;

    // Put IPv6 addresses before IPv4 ones. This code doesn't sort addresses
    // correctly, but when sorted according to spec, IPv6 addresses are
    // generally before IPv4 ones.
    if (address_family == ADDRESS_FAMILY_UNSPECIFIED ||
        address_family == ADDRESS_FAMILY_IPV6) {
      uint8_t num_ipv6_addresses = data_provider_->ConsumeIntegral<uint8_t>();
      for (uint8_t i = 0; i < num_ipv6_addresses; ++i) {
        result.push_back(
            net::IPEndPoint(FuzzIPv6Address(data_provider_.get()), 0));
      }
    }

    if (address_family == ADDRESS_FAMILY_UNSPECIFIED ||
        address_family == ADDRESS_FAMILY_IPV4) {
      uint8_t num_ipv4_addresses = data_provider_->ConsumeIntegral<uint8_t>();
      for (uint8_t i = 0; i < num_ipv4_addresses; ++i) {
        result.push_back(
            net::IPEndPoint(FuzzIPv4Address(data_provider_.get()), 0));
      }
    }

    if (result.empty())
      return ERR_NAME_NOT_RESOLVED;

    if (host_resolver_flags & HOST_RESOLVER_CANONNAME) {
      // Don't bother to fuzz this - almost nothing cares.
      result.set_canonical_name("foo.com");
    }

    *addrlist = result;
    return OK;
  }

 private:
  ~FuzzedHostResolverProc() override = default;

  base::WeakPtr<FuzzedDataProvider> data_provider_;

  // Just used for thread-safety checks.
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(FuzzedHostResolverProc);
};

const Error kMdnsErrors[] = {ERR_FAILED,
                             ERR_ACCESS_DENIED,
                             ERR_INTERNET_DISCONNECTED,
                             ERR_TIMED_OUT,
                             ERR_CONNECTION_RESET,
                             ERR_CONNECTION_ABORTED,
                             ERR_CONNECTION_REFUSED,
                             ERR_ADDRESS_UNREACHABLE};
// Fuzzed socket implementation to handle the limited functionality used by
// MDnsClientImpl. Uses a FuzzedDataProvider to generate errors or responses for
// RecvFrom calls.
class FuzzedMdnsSocket : public DatagramServerSocket {
 public:
  explicit FuzzedMdnsSocket(FuzzedDataProvider* data_provider)
      : data_provider_(data_provider),
        local_address_(FuzzIPAddress(data_provider_), 5353) {}

  int Listen(const IPEndPoint& address) override { return OK; }

  int RecvFrom(IOBuffer* buffer,
               int buffer_length,
               IPEndPoint* out_address,
               CompletionOnceCallback callback) override {
    if (data_provider_->ConsumeBool())
      return GenerateResponse(buffer, buffer_length, out_address);

    // Maybe never receive any responses.
    if (data_provider_->ConsumeBool()) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&FuzzedMdnsSocket::CompleteRecv,
                         weak_factory_.GetWeakPtr(), std::move(callback),
                         base::RetainedRef(buffer), buffer_length,
                         out_address));
    }

    return ERR_IO_PENDING;
  }

  int SendTo(IOBuffer* buf,
             int buf_len,
             const IPEndPoint& address,
             CompletionOnceCallback callback) override {
    if (data_provider_->ConsumeBool()) {
      return data_provider_->ConsumeBool()
                 ? OK
                 : data_provider_->PickValueInArray(kMdnsErrors);
    }

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&FuzzedMdnsSocket::CompleteSend,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
    return ERR_IO_PENDING;
  }

  int SetReceiveBufferSize(int32_t size) override { return OK; }
  int SetSendBufferSize(int32_t size) override { return OK; }

  void AllowAddressReuse() override {}
  void AllowBroadcast() override {}
  void AllowAddressSharingForMulticast() override {}

  int JoinGroup(const IPAddress& group_address) const override { return OK; }
  int LeaveGroup(const IPAddress& group_address) const override { return OK; }
  int SetMulticastInterface(uint32_t interface_index) override { return OK; }
  int SetMulticastTimeToLive(int time_to_live) override { return OK; }
  int SetMulticastLoopbackMode(bool loopback) override { return OK; }

  int SetDiffServCodePoint(DiffServCodePoint dscp) override { return OK; }

  void DetachFromThread() override {}

  void Close() override {}
  int GetPeerAddress(IPEndPoint* address) const override {
    return ERR_SOCKET_NOT_CONNECTED;
  }
  int GetLocalAddress(IPEndPoint* address) const override {
    *address = local_address_;
    return OK;
  }
  void UseNonBlockingIO() override {}
  int SetDoNotFragment() override { return OK; }
  void SetMsgConfirm(bool confirm) override {}
  const NetLogWithSource& NetLog() const override { return net_log_; }

 private:
  void CompleteRecv(CompletionOnceCallback callback,
                    IOBuffer* buffer,
                    int buffer_length,
                    IPEndPoint* out_address) {
    int rv = GenerateResponse(buffer, buffer_length, out_address);
    std::move(callback).Run(rv);
  }

  int GenerateResponse(IOBuffer* buffer,
                       int buffer_length,
                       IPEndPoint* out_address) {
    if (data_provider_->ConsumeBool()) {
      std::string data =
          data_provider_->ConsumeRandomLengthString(buffer_length);
      std::copy(data.begin(), data.end(), buffer->data());
      *out_address =
          IPEndPoint(FuzzIPAddress(data_provider_), FuzzPort(data_provider_));
      return data.size();
    }

    return data_provider_->PickValueInArray(kMdnsErrors);
  }

  void CompleteSend(CompletionOnceCallback callback) {
    if (data_provider_->ConsumeBool())
      std::move(callback).Run(OK);
    else
      std::move(callback).Run(data_provider_->PickValueInArray(kMdnsErrors));
  }

  FuzzedDataProvider* const data_provider_;
  const IPEndPoint local_address_;
  const NetLogWithSource net_log_;

  base::WeakPtrFactory<FuzzedMdnsSocket> weak_factory_{this};
};

class FuzzedMdnsSocketFactory : public MDnsSocketFactory {
 public:
  explicit FuzzedMdnsSocketFactory(FuzzedDataProvider* data_provider)
      : data_provider_(data_provider) {}

  void CreateSockets(
      std::vector<std::unique_ptr<DatagramServerSocket>>* sockets) override {
    int num_sockets = data_provider_->ConsumeIntegralInRange(1, 4);
    for (int i = 0; i < num_sockets; ++i)
      sockets->push_back(std::make_unique<FuzzedMdnsSocket>(data_provider_));
  }

 private:
  FuzzedDataProvider* const data_provider_;
};

class FuzzedHostResolverManager : public HostResolverManager {
 public:
  // |data_provider| and |net_log| must outlive the FuzzedHostResolver.
  // TODO(crbug.com/971411): Fuzz system DNS config changes through a non-null
  // SystemDnsConfigChangeNotifier.
  FuzzedHostResolverManager(const HostResolver::ManagerOptions& options,
                            NetLog* net_log,
                            FuzzedDataProvider* data_provider)
      : HostResolverManager(options,
                            nullptr /* system_dns_config_notifier */,
                            net_log),
        data_provider_(data_provider),
        is_ipv6_reachable_(data_provider->ConsumeBool()),
        socket_factory_(data_provider_),
        net_log_(net_log),
        data_provider_weak_factory_(data_provider) {
    ProcTaskParams proc_task_params(
        new FuzzedHostResolverProc(data_provider_weak_factory_.GetWeakPtr()),
        // Retries are only used when the original request hangs, which this
        // class currently can't simulate.
        0 /* max_retry_attempts */);
    set_proc_params_for_test(proc_task_params);
    SetTaskRunnerForTesting(base::SequencedTaskRunnerHandle::Get());
    SetMdnsSocketFactoryForTesting(
        std::make_unique<FuzzedMdnsSocketFactory>(data_provider_));
    std::unique_ptr<DnsClient> dns_client = DnsClient::CreateClientForTesting(
        net_log_, &socket_factory_,
        base::Bind(&FuzzedDataProvider::ConsumeIntegralInRange<int32_t>,
                   base::Unretained(data_provider_)));
    dns_client->SetSystemConfig(GetFuzzedDnsConfig(data_provider_));
    HostResolverManager::SetDnsClientForTesting(std::move(dns_client));
  }

  ~FuzzedHostResolverManager() override = default;

  void SetDnsClientForTesting(std::unique_ptr<DnsClient> dns_client) {
    // The only DnsClient that is supported is the one created by the
    // FuzzedHostResolverManager since that DnsClient contains the necessary
    // fuzzing logic.
    NOTREACHED();
  }

 private:
  // HostResolverManager implementation:
  bool IsGloballyReachable(const IPAddress& dest,
                           const NetLogWithSource& net_log) override {
    return is_ipv6_reachable_;
  }

  void RunLoopbackProbeJob() override {
    SetHaveOnlyLoopbackAddresses(data_provider_->ConsumeBool());
  }

  FuzzedDataProvider* const data_provider_;

  // Fixed value to be returned by IsIPv6Reachable.
  const bool is_ipv6_reachable_;

  // Used for UDP and TCP sockets if the async resolver is enabled.
  FuzzedSocketFactory socket_factory_;

  NetLog* const net_log_;

  base::WeakPtrFactory<FuzzedDataProvider> data_provider_weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FuzzedHostResolverManager);
};

}  // namespace

std::unique_ptr<ContextHostResolver> CreateFuzzedContextHostResolver(
    const HostResolver::ManagerOptions& options,
    NetLog* net_log,
    FuzzedDataProvider* data_provider,
    bool enable_caching) {
  auto manager = std::make_unique<FuzzedHostResolverManager>(options, net_log,
                                                             data_provider);
  return std::make_unique<ContextHostResolver>(
      std::move(manager),
      enable_caching ? HostCache::CreateDefaultCache() : nullptr);
}

}  // namespace net
