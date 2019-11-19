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

// Number of failures allowed before a DoH server is designated 'unavailable'.
// In AUTOMATIC mode, non-probe DoH queries should not be sent to DoH servers
// that have reached this limit.
// This limit is different from the failure limit that governs insecure async
// resolver bypass in several ways: the failures need not be consecutive,
// NXDOMAIN responses are never counted as failures, and the outcome of
// fallback queries is not taken into account.
const int kAutomaticModeFailureLimit = 10;

// Session parameters and state shared between DNS transactions.
// Ref-counted so that DnsClient::Request can keep working in absence of
// DnsClient. A DnsSession must be recreated when DnsConfig changes.
class NET_EXPORT_PRIVATE DnsSession : public base::RefCounted<DnsSession> {
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

  void UpdateTimeouts(NetworkChangeNotifier::ConnectionType type);
  void InitializeServerStats();

  // Return the next random query ID.
  uint16_t NextQueryId() const;

  // Return the index of the first configured server to use on first attempt.
  unsigned NextFirstServerIndex();

  // Start with |server_index| and find the index of the next known
  // good non-dns-over-https server to use on this attempt. Returns
  // |server_index| if this server is below the failure limit, or if there are
  // no other servers below the failure limit or with an older last failure.
  unsigned NextGoodServerIndex(unsigned server_index);

  // Similar to |NextGoodServerIndex| except for DoH servers. If no DoH server
  // is under the failure limit in AUTOMATIC mode, returns -1.
  int NextGoodDohServerIndex(unsigned doh_server_index,
                             DnsConfig::SecureDnsMode secure_dns_mode);

  // Returns whether there is a DoH server that has a successful probe state.
  bool HasAvailableDohServer();

  // Returns the number of DoH servers with successful probe states.
  unsigned NumAvailableDohServers();

  // Record that server failed to respond (due to SRV_FAIL or timeout). If
  // |is_doh_server| and the number of failures has surpassed a threshold,
  // sets the DoH probe state to unavailable.
  void RecordServerFailure(unsigned server_index, bool is_doh_server);

  // Record that server responded successfully.
  void RecordServerSuccess(unsigned server_index, bool is_doh_server);

  // Record the latest DoH probe state.
  void SetProbeSuccess(unsigned doh_server_index, bool success);

  // Record how long it took to receive a response from the server.
  void RecordRTT(unsigned server_index,
                 bool is_doh_server,
                 base::TimeDelta rtt,
                 int rv);

  // Return the timeout for the next query. |attempt| counts from 0 and is used
  // for exponential backoff.
  base::TimeDelta NextTimeout(unsigned server_index, int attempt);

  // Return the timeout for the next DoH query.
  base::TimeDelta NextDohTimeout(unsigned doh_server_index);

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
  struct ServerStats;
  ~DnsSession();

  // Release a socket.
  void FreeSocket(unsigned server_index,
                  std::unique_ptr<DatagramClientSocket> socket);

  // Returns the ServerStats for the designated server. Returns nullptr if no
  // ServerStats found.
  ServerStats* GetServerStats(unsigned server_index, bool is_doh_server);

  // Return the timeout for the next query.
  base::TimeDelta NextTimeoutHelper(ServerStats* server_stats, int attempt);

  // Record the time to perform a query.
  void RecordRTTForHistogram(unsigned server_index,
                             bool is_doh_server,
                             base::TimeDelta rtt,
                             int rv);

  const DnsConfig config_;
  std::unique_ptr<DnsSocketPool> socket_pool_;
  RandCallback rand_callback_;
  NetLog* net_log_;

  // Current index into |config_.nameservers| to begin resolution with.
  int server_index_;

  base::TimeDelta initial_timeout_;
  base::TimeDelta max_timeout_;

  // Track runtime statistics of each insecure DNS server.
  std::vector<std::unique_ptr<ServerStats>> server_stats_;

  // Track runtime statistics and availability of each DoH server.
  std::vector<std::pair<std::unique_ptr<ServerStats>, bool>> doh_server_stats_;

  // Buckets shared for all |ServerStats::rtt_histogram|.
  struct RttBuckets : public base::BucketRanges {
    RttBuckets();
  };
  static base::LazyInstance<RttBuckets>::Leaky rtt_buckets_;

  DISALLOW_COPY_AND_ASSIGN(DnsSession);
};

}  // namespace net

#endif  // NET_DNS_DNS_SESSION_H_
