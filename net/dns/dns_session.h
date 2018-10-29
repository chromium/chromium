// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_SESSION_H_
#define NET_DNS_DNS_SESSION_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/bucket_ranges.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"
#include "net/base/rand_callback.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_socket_pool.h"

namespace base {
class BucketRanges;
}

namespace net {

class DatagramClientSocket;
class NetLog;
struct NetLogSource;
class StreamSocket;

// Session parameters and state shared between DNS transactions.
// Ref-counted so that DnsClient::Request can keep working in absence of
// DnsClient. A DnsSession must be recreated when DnsConfig changes.
class NET_EXPORT_PRIVATE DnsSession
    : public base::RefCounted<DnsSession>,
      public NetworkChangeNotifier::ConnectionTypeObserver {
 public:
  typedef base::Callback<int()> RandCallback;

  class NET_EXPORT_PRIVATE SocketLease {
   public:
    SocketLease(scoped_refptr<DnsSession> session,
                unsigned server_index,
                std::unique_ptr<DatagramClientSocket> socket);
    ~SocketLease();

    unsigned server_index() const { return server_index_; }

    DatagramClientSocket* socket() { return socket_.get(); }

   private:
    scoped_refptr<DnsSession> session_;
    unsigned server_index_;
    std::unique_ptr<DatagramClientSocket> socket_;

    DISALLOW_COPY_AND_ASSIGN(SocketLease);
  };

  DnsSession(const DnsConfig& config,
             std::unique_ptr<DnsSocketPool> socket_pool,
             const RandIntCallback& rand_int_callback,
             NetLog* net_log);

  const DnsConfig& config() const { return config_; }
  NetLog* net_log() const { return net_log_; }

  // Return the next random query ID.
  uint16_t NextQueryId() const;

  // Return the index of the first configured server to use on first attempt.
  unsigned NextFirstServerIndex();

  // Start with |server_index| and find the index of the next known
  // good non-dns-over-https server to use on this attempt. Returns
  // |server_index| if this server has no recorded failures, or if
  // there are no other servers that have not failed or have failed
  // longer time ago.
  unsigned NextGoodServerIndex(unsigned server_index);

  // Same as above, but for DNS over HTTPS servers and ignoring
  // non-dns-over-https servers
  unsigned NextGoodDnsOverHttpsServerIndex(unsigned server_index);

  // Record that server failed to respond (due to SRV_FAIL or timeout).
  void RecordServerFailure(unsigned server_index);

  // Record that server responded successfully.
  void RecordServerSuccess(unsigned server_index);

  // Record how long it took to receive a response from the server.
  void RecordRTT(unsigned server_index, base::TimeDelta rtt);

  // Record suspected loss of a packet for a specific server.
  void RecordLostPacket(unsigned server_index, int attempt);

  // Record server stats before it is destroyed.
  void RecordServerStats();

  // Return the timeout for the next query. |attempt| counts from 0 and is used
  // for exponential backoff.
  base::TimeDelta NextTimeout(unsigned server_index, int attempt);

  // Allocate a socket, already connected to the server address.
  // When the SocketLease is destroyed, the socket will be freed.
  std::unique_ptr<SocketLease> AllocateSocket(unsigned server_index,
                                              const NetLogSource& source);

  // Creates a StreamSocket from the factory for a transaction over TCP. These
  // sockets are not pooled.
  std::unique_ptr<StreamSocket> CreateTCPSocket(unsigned server_index,
                                                const NetLogSource& source);

 private:
  friend class base::RefCounted<DnsSession>;
  ~DnsSession() override;

  void UpdateTimeouts(NetworkChangeNotifier::ConnectionType type);
  void InitializeServerStats();

  // Release a socket.
  void FreeSocket(unsigned server_index,
                  std::unique_ptr<DatagramClientSocket> socket);

  // Return the timeout using the TCP timeout method.
  base::TimeDelta NextTimeoutFromJacobson(unsigned server_index, int attempt);

  // Compute the timeout using the histogram method.
  base::TimeDelta NextTimeoutFromHistogram(unsigned server_index, int attempt);

  // NetworkChangeNotifier::ConnectionTypeObserver:
  void OnConnectionTypeChanged(
      NetworkChangeNotifier::ConnectionType type) override;

  const DnsConfig config_;
  std::unique_ptr<DnsSocketPool> socket_pool_;
  RandCallback rand_callback_;
  NetLog* net_log_;

  // Current index into |config_.nameservers| to begin resolution with.
  int server_index_;

  base::TimeDelta initial_timeout_;
  base::TimeDelta max_timeout_;

  struct ServerStats;

  // Track runtime statistics of each DNS server. This combines both
  // dns-over-https servers and non-dns-over-https servers.
  // non-dns-over-https servers come first and dns-over-https servers
  // started at the index of nameservers.size().
  std::vector<std::unique_ptr<ServerStats>> server_stats_;

  // Buckets shared for all |ServerStats::rtt_histogram|.
  struct RttBuckets : public base::BucketRanges {
    RttBuckets();
  };
  static base::LazyInstance<RttBuckets>::Leaky rtt_buckets_;

  DISALLOW_COPY_AND_ASSIGN(DnsSession);
};

}  // namespace net

#endif  // NET_DNS_DNS_SESSION_H_
