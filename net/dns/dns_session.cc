// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_session.h"

#include <stdint.h>

#include <cstdlib>
#include <limits>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sample_vector.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_socket_pool.h"
#include "net/dns/dns_util.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/datagram_client_socket.h"
#include "net/socket/stream_socket.h"

namespace net {

namespace {

// Set min timeout, in case we are talking to a local DNS proxy.
const unsigned kMinTimeoutMs = 10;

// Default maximum timeout between queries, even with exponential backoff.
// (Can be overridden by field trial.)
const unsigned kDefaultMaxTimeoutMs = 5000;

// Maximum RTT that will fit in the RTT histograms.
const int32_t kRTTMaxMs = 30000;
// Number of buckets in the histogram of observed RTTs.
const size_t kRTTBucketCount = 350;
// Target percentile in the RTT histogram used for retransmission timeout.
const unsigned kRTOPercentile = 99;
// Number of samples to seed the histogram with.
const unsigned kNumSeeds = 2;

}  // namespace

// Runtime statistics of DNS server.
struct DnsSession::ServerStats {
  ServerStats(base::TimeDelta rtt_estimate_param, RttBuckets* buckets)
    : last_failure_count(0), rtt_estimate(rtt_estimate_param) {
    rtt_histogram.reset(new base::SampleVector(buckets));
    // Seed histogram with 2 samples at |rtt_estimate| timeout.
    rtt_histogram->Accumulate(
        static_cast<base::HistogramBase::Sample>(rtt_estimate.InMilliseconds()),
        kNumSeeds);
  }

  // Count of consecutive failures after last success.
  int last_failure_count;

  // Last time when server returned failure or timeout.
  base::Time last_failure;
  // Last time when server returned success.
  base::Time last_success;

  // Estimated RTT using moving average.
  base::TimeDelta rtt_estimate;
  // Estimated error in the above.
  base::TimeDelta rtt_deviation;

  // A histogram of observed RTT .
  std::unique_ptr<base::SampleVector> rtt_histogram;

  DISALLOW_COPY_AND_ASSIGN(ServerStats);
};

// static
base::LazyInstance<DnsSession::RttBuckets>::Leaky DnsSession::rtt_buckets_ =
    LAZY_INSTANCE_INITIALIZER;

DnsSession::RttBuckets::RttBuckets() : base::BucketRanges(kRTTBucketCount + 1) {
  base::Histogram::InitializeBucketRanges(1, kRTTMaxMs, this);
}

DnsSession::SocketLease::SocketLease(
    scoped_refptr<DnsSession> session,
    unsigned server_index,
    std::unique_ptr<DatagramClientSocket> socket)
    : session_(session),
      server_index_(server_index),
      socket_(std::move(socket)) {}

DnsSession::SocketLease::~SocketLease() {
  session_->FreeSocket(server_index_, std::move(socket_));
}

DnsSession::DnsSession(const DnsConfig& config,
                       std::unique_ptr<DnsSocketPool> socket_pool,
                       const RandIntCallback& rand_int_callback,
                       NetLog* net_log)
    : config_(config),
      socket_pool_(std::move(socket_pool)),
      rand_callback_(base::Bind(rand_int_callback,
                                0,
                                std::numeric_limits<uint16_t>::max())),
      net_log_(net_log),
      server_index_(0) {
  socket_pool_->Initialize(&config_.nameservers, net_log);
  UMA_HISTOGRAM_CUSTOM_COUNTS("AsyncDNS.ServerCount",
                              config_.nameservers.size(), 1, 10, 11);
  UpdateTimeouts(NetworkChangeNotifier::GetConnectionType());
  InitializeServerStats();
}

DnsSession::~DnsSession() = default;

void DnsSession::UpdateTimeouts(NetworkChangeNotifier::ConnectionType type) {
  initial_timeout_ = GetTimeDeltaForConnectionTypeFromFieldTrialOrDefault(
      "AsyncDnsInitialTimeoutMsByConnectionType", config_.timeout, type);
  max_timeout_ = GetTimeDeltaForConnectionTypeFromFieldTrialOrDefault(
      "AsyncDnsMaxTimeoutMsByConnectionType",
      base::TimeDelta::FromMilliseconds(kDefaultMaxTimeoutMs), type);
}

void DnsSession::InitializeServerStats() {
  server_stats_.clear();
  for (size_t i = 0; i < config_.nameservers.size(); ++i) {
    server_stats_.push_back(std::make_unique<ServerStats>(
        initial_timeout_, rtt_buckets_.Pointer()));
  }

  doh_server_stats_.clear();
  for (size_t i = 0; i < config_.dns_over_https_servers.size(); ++i) {
    doh_server_stats_.push_back(std::make_pair(
        std::make_unique<ServerStats>(initial_timeout_, rtt_buckets_.Pointer()),
        false));
  }
}

uint16_t DnsSession::NextQueryId() const {
  return static_cast<uint16_t>(rand_callback_.Run());
}

unsigned DnsSession::NextFirstServerIndex() {
  unsigned index = NextGoodServerIndex(server_index_);
  if (config_.rotate)
    server_index_ = (server_index_ + 1) % config_.nameservers.size();
  return index;
}

unsigned DnsSession::NextGoodServerIndex(unsigned server_index) {
  DCHECK_GE(server_index, 0u);
  DCHECK_LT(server_index, config_.nameservers.size());
  unsigned index = server_index;
  base::Time oldest_server_failure(base::Time::Now());
  unsigned oldest_server_failure_index = 0;

  do {
    // If number of failures on this server doesn't exceed number of allowed
    // attempts, return its index.
    if (server_stats_[server_index]->last_failure_count < config_.attempts) {
      return index;
    }
    // Track oldest failed server.
    base::Time cur_server_failure = server_stats_[index]->last_failure;
    if (cur_server_failure < oldest_server_failure) {
      oldest_server_failure = cur_server_failure;
      oldest_server_failure_index = index;
    }
    index = (index + 1) % config_.nameservers.size();
  } while (index != server_index);

  // If we are here it means that there are no successful servers, so we have
  // to use one that has failed oldest.
  return oldest_server_failure_index;
}

int DnsSession::NextGoodDohServerIndex(
    unsigned doh_server_index,
    DnsConfig::SecureDnsMode secure_dns_mode) {
  DCHECK_GE(doh_server_index, 0u);
  DCHECK_LT(doh_server_index, config_.dns_over_https_servers.size());
  unsigned index = doh_server_index;
  base::Time oldest_server_failure(base::Time::Now());
  int oldest_available_server_failure_index = -1;

  do {
    // For a server to be considered "available", the server must have a
    // successful probe status if we are in AUTOMATIC mode.
    if (secure_dns_mode == DnsConfig::SecureDnsMode::SECURE ||
        doh_server_stats_[index].second) {
      // If number of failures on this server doesn't exceed |config_.attempts|,
      // return its index. |config_.attempts| will generally be more restrictive
      // than |kAutomaticModeFailureLimit|, although this is not guaranteed.
      const ServerStats* stats =
          GetServerStats(index, true /* is_doh_server */);
      if (stats->last_failure_count < config_.attempts) {
        return index;
      }
      // Track oldest failed available server.
      base::Time cur_server_failure = stats->last_failure;
      if (cur_server_failure < oldest_server_failure) {
        oldest_server_failure = cur_server_failure;
        oldest_available_server_failure_index = index;
      }
    }
    index = (index + 1) % config_.dns_over_https_servers.size();
  } while (index != doh_server_index);

  // If we are here it means that there are either no available DoH servers or
  // that all available DoH servers have at least |config_.attempts| consecutive
  // failures. In the latter case, we'll return the available DoH server that
  // failed least recently. In the former case we return -1.
  return oldest_available_server_failure_index;
}

bool DnsSession::HasAvailableDohServer() {
  for (const auto& doh_stats_ : doh_server_stats_) {
    if (doh_stats_.second)
      return true;
  }
  return false;
}

unsigned DnsSession::NumAvailableDohServers() {
  unsigned count = 0;
  for (const auto& doh_stats_ : doh_server_stats_) {
    if (doh_stats_.second)
      count++;
  }
  return count;
}

DnsSession::ServerStats* DnsSession::GetServerStats(unsigned server_index,
                                                    bool is_doh_server) {
  DCHECK_GE(server_index, 0u);
  if (!is_doh_server) {
    DCHECK_LT(server_index, config_.nameservers.size());
    return server_stats_[server_index].get();
  } else {
    DCHECK_LT(server_index, config_.dns_over_https_servers.size());
    return doh_server_stats_[server_index].first.get();
  }
}

void DnsSession::RecordServerFailure(unsigned server_index,
                                     bool is_doh_server) {
  ServerStats* stats = GetServerStats(server_index, is_doh_server);
  ++(stats->last_failure_count);
  stats->last_failure = base::Time::Now();

  if (is_doh_server &&
      stats->last_failure_count >= kAutomaticModeFailureLimit) {
    SetProbeSuccess(server_index, false /* success */);
  }
}

void DnsSession::RecordServerSuccess(unsigned server_index,
                                     bool is_doh_server) {
  ServerStats* stats = GetServerStats(server_index, is_doh_server);

  // DoH queries can be sent using more than one URLRequestContext. A success
  // from one URLRequestContext shouldn't zero out failures that may be
  // consistently occurring for another URLRequestContext.
  if (!is_doh_server)
    stats->last_failure_count = 0;
  stats->last_failure = base::Time();
  stats->last_success = base::Time::Now();
}

void DnsSession::SetProbeSuccess(unsigned doh_server_index, bool success) {
  DCHECK_GE(doh_server_index, 0u);
  DCHECK_LT(doh_server_index, config_.dns_over_https_servers.size());

  bool doh_available_before = HasAvailableDohServer();
  doh_server_stats_[doh_server_index].second = success;

  if (doh_available_before != HasAvailableDohServer())
    NetworkChangeNotifier::TriggerNonSystemDnsChange();
}

void DnsSession::RecordRTT(unsigned server_index,
                           bool is_doh_server,
                           base::TimeDelta rtt,
                           int rv) {
  RecordRTTForHistogram(server_index, is_doh_server, rtt, rv);

  ServerStats* stats = GetServerStats(server_index, is_doh_server);

  // Jacobson/Karels algorithm for TCP.
  // Using parameters: alpha = 1/8, delta = 1/4, beta = 4
  base::TimeDelta& estimate = stats->rtt_estimate;
  base::TimeDelta& deviation = stats->rtt_deviation;
  base::TimeDelta current_error = rtt - estimate;
  estimate += current_error / 8;  // * alpha
  base::TimeDelta abs_error = base::TimeDelta::FromInternalValue(
      std::abs(current_error.ToInternalValue()));
  deviation += (abs_error - deviation) / 4;  // * delta

  // RTT values shouldn't be less than 0, but it shouldn't cause a crash if
  // they are anyway, so clip to 0. See https://crbug.com/753568.
  int32_t rtt_ms = rtt.InMilliseconds();
  if (rtt_ms < 0)
    rtt_ms = 0;

  // Histogram-based method.
  stats->rtt_histogram->Accumulate(
      static_cast<base::HistogramBase::Sample>(rtt_ms), 1);
}

base::TimeDelta DnsSession::NextTimeout(unsigned server_index, int attempt) {
  return NextTimeoutHelper(
      GetServerStats(server_index, false /* is _doh_server */),
      attempt / config_.nameservers.size());
}

base::TimeDelta DnsSession::NextDohTimeout(unsigned doh_server_index) {
  return NextTimeoutHelper(
      GetServerStats(doh_server_index, true /* is _doh_server */),
      0 /* num_backoffs */);
}

base::TimeDelta DnsSession::NextTimeoutHelper(ServerStats* server_stats,
                                              int num_backoffs) {
  // Respect initial timeout (from config or field trial) if it exceeds max.
  if (initial_timeout_ > max_timeout_)
    return initial_timeout_;

  static_assert(std::numeric_limits<base::HistogramBase::Count>::is_signed,
                "histogram base count assumed to be signed");

  // Use fixed percentile of observed samples.
  const base::SampleVector& samples = *server_stats->rtt_histogram;

  base::HistogramBase::Count total = samples.TotalCount();
  base::HistogramBase::Count remaining_count = kRTOPercentile * total / 100;
  size_t index = 0;
  while (remaining_count > 0 && index < rtt_buckets_.Get().size()) {
    remaining_count -= samples.GetCountAtIndex(index);
    ++index;
  }

  base::TimeDelta timeout =
      base::TimeDelta::FromMilliseconds(rtt_buckets_.Get().range(index));

  timeout = std::max(timeout, base::TimeDelta::FromMilliseconds(kMinTimeoutMs));

  return std::min(timeout * (1 << num_backoffs), max_timeout_);
}

// Allocate a socket, already connected to the server address.
std::unique_ptr<DnsSession::SocketLease> DnsSession::AllocateSocket(
    unsigned server_index,
    const NetLogSource& source) {
  std::unique_ptr<DatagramClientSocket> socket;

  socket = socket_pool_->AllocateSocket(server_index);
  if (!socket.get())
    return std::unique_ptr<SocketLease>();

  socket->NetLog().BeginEventReferencingSource(NetLogEventType::SOCKET_IN_USE,
                                               source);

  SocketLease* lease = new SocketLease(this, server_index, std::move(socket));
  return std::unique_ptr<SocketLease>(lease);
}

std::unique_ptr<StreamSocket> DnsSession::CreateTCPSocket(
    unsigned server_index,
    const NetLogSource& source) {
  return socket_pool_->CreateTCPSocket(server_index, source);
}

// Release a socket.
void DnsSession::FreeSocket(unsigned server_index,
                            std::unique_ptr<DatagramClientSocket> socket) {
  DCHECK(socket.get());

  socket->NetLog().EndEvent(NetLogEventType::SOCKET_IN_USE);

  socket_pool_->FreeSocket(server_index, std::move(socket));
}

void DnsSession::RecordRTTForHistogram(unsigned server_index,
                                       bool is_doh_server,
                                       base::TimeDelta rtt,
                                       int rv) {
  std::string query_type;
  std::string provider_id;
  if (is_doh_server) {
    // Secure queries are validated if the DoH server state is available.
    if (doh_server_stats_[server_index].second)
      query_type = "SecureValidated";
    else
      query_type = "SecureNotValidated";
    provider_id = GetDohProviderIdForHistogramFromDohConfig(
        config_.dns_over_https_servers[server_index]);
  } else {
    query_type = "Insecure";
    provider_id = GetDohProviderIdForHistogramFromNameserver(
        config_.nameservers[server_index]);
  }
  if (rv == OK || rv == ERR_NAME_NOT_RESOLVED) {
    base::UmaHistogramMediumTimes(
        base::StringPrintf("Net.DNS.DnsTransaction.%s.%s.SuccessTime",
                           query_type.c_str(), provider_id.c_str()),
        rtt);
  } else {
    base::UmaHistogramMediumTimes(
        base::StringPrintf("Net.DNS.DnsTransaction.%s.%s.FailureTime",
                           query_type.c_str(), provider_id.c_str()),
        rtt);
    if (is_doh_server) {
      base::UmaHistogramSparse(
          base::StringPrintf("Net.DNS.DnsTransaction.%s.%s.FailureError",
                             query_type.c_str(), provider_id.c_str()),
          std::abs(rv));
    }
  }
}

}  // namespace net
