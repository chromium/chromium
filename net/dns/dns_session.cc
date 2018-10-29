// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_session.h"

#include <stdint.h>

#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sample_vector.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
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

}  // namespace

// Runtime statistics of DNS server.
struct DnsSession::ServerStats {
  ServerStats(base::TimeDelta rtt_estimate_param, RttBuckets* buckets)
    : last_failure_count(0), rtt_estimate(rtt_estimate_param) {
    rtt_histogram.reset(new base::SampleVector(buckets));
    // Seed histogram with 2 samples at |rtt_estimate| timeout.
    rtt_histogram->Accumulate(
        static_cast<base::HistogramBase::Sample>(rtt_estimate.InMilliseconds()),
        2);
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
  NetworkChangeNotifier::AddConnectionTypeObserver(this);
}

DnsSession::~DnsSession() {
  RecordServerStats();
  NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
}

void DnsSession::UpdateTimeouts(NetworkChangeNotifier::ConnectionType type) {
  initial_timeout_ = GetTimeDeltaForConnectionTypeFromFieldTrialOrDefault(
      "AsyncDnsInitialTimeoutMsByConnectionType", config_.timeout, type);
  max_timeout_ = GetTimeDeltaForConnectionTypeFromFieldTrialOrDefault(
      "AsyncDnsMaxTimeoutMsByConnectionType",
      base::TimeDelta::FromMilliseconds(kDefaultMaxTimeoutMs), type);
}

void DnsSession::InitializeServerStats() {
  server_stats_.clear();
  for (size_t i = 0;
       i < config_.nameservers.size() + config_.dns_over_https_servers.size();
       ++i) {
    server_stats_.push_back(std::make_unique<ServerStats>(
        initial_timeout_, rtt_buckets_.Pointer()));
  }
}

void DnsSession::OnConnectionTypeChanged(
    NetworkChangeNotifier::ConnectionType type) {
  UpdateTimeouts(type);
  const char* kTrialName = "AsyncDnsFlushServerStatsOnConnectionTypeChange";
  if (base::FieldTrialList::FindFullName(kTrialName) == "enable") {
    RecordServerStats();
    InitializeServerStats();
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
    base::Time cur_server_failure = server_stats_[index]->last_failure;
    // If number of failures on this server doesn't exceed number of allowed
    // attempts, return its index.
    if (server_stats_[server_index]->last_failure_count < config_.attempts) {
      return index;
    }
    // Track oldest failed server.
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

unsigned DnsSession::NextGoodDnsOverHttpsServerIndex(unsigned server_index) {
  DCHECK_GE(server_index, config_.nameservers.size());
  DCHECK_LT(server_index,
            config_.nameservers.size() + config_.dns_over_https_servers.size());
  unsigned index = server_index;
  base::Time oldest_server_failure(base::Time::Now());
  unsigned oldest_server_failure_index = config_.nameservers.size();

  do {
    base::Time cur_server_failure = server_stats_[index]->last_failure;
    // If number of failures on this server doesn't exceed number of allowed
    // attempts, return its index.
    if (server_stats_[index]->last_failure_count < config_.attempts) {
      return index;
    }
    // Track oldest failed server.
    if (cur_server_failure < oldest_server_failure) {
      oldest_server_failure = cur_server_failure;
      oldest_server_failure_index = index;
    }
    // Index of dns over https servers begins at nameservers.size().
    unsigned doh_index = index - config_.nameservers.size();
    doh_index = ((doh_index + 1) % config_.dns_over_https_servers.size());
    index = doh_index + config_.nameservers.size();
  } while (index != server_index);

  // If we are here it means that there are no successful servers, so we have
  // to use one that has failed oldest.
  return oldest_server_failure_index;
}

void DnsSession::RecordServerFailure(unsigned server_index) {
  UMA_HISTOGRAM_CUSTOM_COUNTS("AsyncDNS.ServerFailureIndex", server_index, 1,
                              10, 11);
  ++(server_stats_[server_index]->last_failure_count);
  server_stats_[server_index]->last_failure = base::Time::Now();
}

void DnsSession::RecordServerSuccess(unsigned server_index) {
  if (server_stats_[server_index]->last_success.is_null()) {
    UMA_HISTOGRAM_COUNTS_100("AsyncDNS.ServerFailuresAfterNetworkChange",
                           server_stats_[server_index]->last_failure_count);
  } else {
    UMA_HISTOGRAM_COUNTS_100("AsyncDNS.ServerFailuresBeforeSuccess",
                           server_stats_[server_index]->last_failure_count);
  }
  server_stats_[server_index]->last_failure_count = 0;
  server_stats_[server_index]->last_failure = base::Time();
  server_stats_[server_index]->last_success = base::Time::Now();
}

void DnsSession::RecordRTT(unsigned server_index, base::TimeDelta rtt) {
  DCHECK_LT(server_index, server_stats_.size());

  // For measurement, assume it is the first attempt (no backoff).
  base::TimeDelta timeout_jacobson = NextTimeoutFromJacobson(server_index, 0);
  base::TimeDelta timeout_histogram = NextTimeoutFromHistogram(server_index, 0);
  UMA_HISTOGRAM_TIMES("AsyncDNS.TimeoutErrorJacobson", rtt - timeout_jacobson);
  UMA_HISTOGRAM_TIMES("AsyncDNS.TimeoutErrorHistogram",
                      rtt - timeout_histogram);
  UMA_HISTOGRAM_TIMES("AsyncDNS.TimeoutErrorJacobsonUnder",
                      timeout_jacobson - rtt);
  UMA_HISTOGRAM_TIMES("AsyncDNS.TimeoutErrorHistogramUnder",
                      timeout_histogram - rtt);

  // Jacobson/Karels algorithm for TCP.
  // Using parameters: alpha = 1/8, delta = 1/4, beta = 4
  base::TimeDelta& estimate = server_stats_[server_index]->rtt_estimate;
  base::TimeDelta& deviation = server_stats_[server_index]->rtt_deviation;
  base::TimeDelta current_error = rtt - estimate;
  estimate += current_error / 8;  // * alpha
  base::TimeDelta abs_error = base::TimeDelta::FromInternalValue(
      std::abs(current_error.ToInternalValue()));
  deviation += (abs_error - deviation) / 4;  // * delta

  // RTT values shouldn't be less than 0, but it shouldn't cause a crash if they
  // are anyway, so clip to 0. See https://crbug.com/753568.
  int32_t rtt_ms = rtt.InMilliseconds();
  if (rtt_ms < 0)
    rtt_ms = 0;

  // Histogram-based method.
  server_stats_[server_index]->rtt_histogram->Accumulate(
      static_cast<base::HistogramBase::Sample>(rtt_ms), 1);
}

void DnsSession::RecordLostPacket(unsigned server_index, int attempt) {
  base::TimeDelta timeout_jacobson =
      NextTimeoutFromJacobson(server_index, attempt);
  base::TimeDelta timeout_histogram =
      NextTimeoutFromHistogram(server_index, attempt);
  UMA_HISTOGRAM_TIMES("AsyncDNS.TimeoutSpentJacobson", timeout_jacobson);
  UMA_HISTOGRAM_TIMES("AsyncDNS.TimeoutSpentHistogram", timeout_histogram);
}

void DnsSession::RecordServerStats() {
  for (size_t index = 0; index < server_stats_.size(); ++index) {
    if (server_stats_[index]->last_failure_count) {
      if (server_stats_[index]->last_success.is_null()) {
        UMA_HISTOGRAM_COUNTS_1M("AsyncDNS.ServerFailuresWithoutSuccess",
                                server_stats_[index]->last_failure_count);
      } else {
        UMA_HISTOGRAM_COUNTS_1M("AsyncDNS.ServerFailuresAfterSuccess",
                                server_stats_[index]->last_failure_count);
      }
    }
  }
}


base::TimeDelta DnsSession::NextTimeout(unsigned server_index, int attempt) {
  // Respect initial timeout (from config or field trial) if it exceeds max.
  if (initial_timeout_ > max_timeout_)
    return initial_timeout_;
  return NextTimeoutFromHistogram(server_index, attempt);
}

// Allocate a socket, already connected to the server address.
std::unique_ptr<DnsSession::SocketLease> DnsSession::AllocateSocket(
    unsigned server_index,
    const NetLogSource& source) {
  std::unique_ptr<DatagramClientSocket> socket;

  socket = socket_pool_->AllocateSocket(server_index);
  if (!socket.get())
    return std::unique_ptr<SocketLease>();

  socket->NetLog().BeginEvent(NetLogEventType::SOCKET_IN_USE,
                              source.ToEventParametersCallback());

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

base::TimeDelta DnsSession::NextTimeoutFromJacobson(unsigned server_index,
                                                    int attempt) {
  DCHECK_LT(server_index, server_stats_.size());

  base::TimeDelta timeout = server_stats_[server_index]->rtt_estimate +
                            4 * server_stats_[server_index]->rtt_deviation;

  timeout = std::max(timeout, base::TimeDelta::FromMilliseconds(kMinTimeoutMs));

  // The timeout doubles every full round.
  unsigned num_backoffs = attempt / config_.nameservers.size();

  return std::min(timeout * (1 << num_backoffs), max_timeout_);
}

base::TimeDelta DnsSession::NextTimeoutFromHistogram(unsigned server_index,
                                                     int attempt) {
  DCHECK_LT(server_index, server_stats_.size());

  static_assert(std::numeric_limits<base::HistogramBase::Count>::is_signed,
                "histogram base count assumed to be signed");

  // Use fixed percentile of observed samples.
  const base::SampleVector& samples =
      *server_stats_[server_index]->rtt_histogram;

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

  // The timeout still doubles every full round.
  unsigned num_backoffs = attempt / config_.nameservers.size();

  return std::min(timeout * (1 << num_backoffs), max_timeout_);
}

}  // namespace net
