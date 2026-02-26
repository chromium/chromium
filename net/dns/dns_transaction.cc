// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_transaction.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/base64url.h"
#include "base/byte_count.h"
#include "base/containers/circular_deque.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/safe_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/backoff_entry.h"
#include "net/base/completion_once_callback.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/idempotency.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/dns/dns_attempt.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_http_attempt.h"
#include "net/dns/dns_names_util.h"
#include "net/dns/dns_query.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_response_result_extractor.h"
#include "net/dns/dns_server_iterator.h"
#include "net/dns/dns_session.h"
#include "net/dns/dns_tcp_attempt.h"
#include "net/dns/dns_udp_attempt.h"
#include "net/dns/dns_udp_tracker.h"
#include "net/dns/dns_util.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver_internal_result.h"
#include "net/dns/public/dns_over_https_config.h"
#include "net/dns/public/dns_over_https_server_config.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/dns/resolve_context.h"
#include "net/http/http_request_headers.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_values.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/datagram_client_socket.h"
#include "net/socket/stream_socket.h"
#include "net/third_party/uri_template/uri_template.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "url/url_constants.h"

namespace net {

namespace {

// Count labels in the fully-qualified name in DNS format.
int CountLabels(base::span<const uint8_t> name) {
  size_t count = 0;
  for (size_t i = 0; i < name.size() && name[i]; i += name[i] + 1)
    ++count;
  return count;
}

bool IsIPLiteral(const std::string& hostname) {
  IPAddress ip;
  return ip.AssignFromIPLiteral(hostname);
}

base::DictValue NetLogStartParams(const std::string& hostname, uint16_t qtype) {
  base::DictValue dict;
  dict.Set("hostname", hostname);
  dict.Set("query_type", qtype);
  return dict;
}

void ConstructDnsHTTPAttempt(base::WeakPtr<ResolveContext> resolve_context,
                             DnsSession* session,
                             size_t doh_server_index,
                             base::span<const uint8_t> qname,
                             uint16_t qtype,
                             const OptRecordRdata* opt_rdata,
                             std::vector<std::unique_ptr<DnsAttempt>>* attempts,
                             URLRequestContext* url_request_context,
                             const IsolationInfo& isolation_info,
                             RequestPriority request_priority,
                             bool is_probe) {
  DCHECK(url_request_context);

  std::unique_ptr<DnsQuery> query;
  if (attempts->empty()) {
    query =
        std::make_unique<DnsQuery>(/*id=*/0, qname, qtype, opt_rdata,
                                   DnsQuery::PaddingStrategy::BLOCK_LENGTH_128);
  } else {
    query = std::make_unique<DnsQuery>(*attempts->at(0)->GetQuery());
  }

  DCHECK_LT(doh_server_index, session->config().doh_config.servers().size());
  const DnsOverHttpsServerConfig& doh_server =
      session->config().doh_config.servers()[doh_server_index];
  GURL gurl_without_parameters(
      GetURLFromTemplateWithoutParameters(doh_server.server_template()));
  attempts->push_back(std::make_unique<DnsHTTPAttempt>(
      std::move(resolve_context), session, doh_server_index, std::move(query),
      doh_server.server_template(), gurl_without_parameters,
      doh_server.use_post(), url_request_context, isolation_info,
      request_priority, is_probe));
}

// ----------------------------------------------------------------------------

const net::BackoffEntry::Policy kProbeBackoffPolicy = {
    // Apply exponential backoff rules after the first error.
    0,
    // Begin with a 1s delay between probes.
    1000,
    // Increase the delay between consecutive probes by a factor of 1.5.
    1.5,
    // Fuzz the delay between consecutive probes between 80%-100% of the
    // calculated time.
    0.2,
    // Cap the maximum delay between consecutive probes at 1 hour.
    1000 * 60 * 60,
    // Never expire entries.
    -1,
    // Do not apply an initial delay.
    false,
};

// Probe runner that continually sends test queries (with backoff) to DoH
// servers to determine availability.
//
// Expected to be contained in request classes owned externally to HostResolver,
// so no assumptions are made regarding cancellation compared to the DnsSession
// or ResolveContext. Instead, uses WeakPtrs to gracefully clean itself up and
// stop probing after session or context destruction.
class DnsOverHttpsProbeRunner : public DnsProbeRunner {
 public:
  DnsOverHttpsProbeRunner(base::WeakPtr<DnsSession> session,
                          base::WeakPtr<ResolveContext> context)
      : session_(session), context_(context) {
    DCHECK(session_);
    DCHECK(!session_->config().doh_config.servers().empty());
    DCHECK(context_);

    std::optional<std::vector<uint8_t>> qname =
        dns_names_util::DottedNameToNetwork(kDohProbeHostname);
    DCHECK(qname.has_value());
    formatted_probe_qname_ = std::move(qname).value();

    for (size_t i = 0; i < session_->config().doh_config.servers().size();
         i++) {
      probe_stats_list_.push_back(nullptr);
    }
  }

  ~DnsOverHttpsProbeRunner() override = default;

  void Start(bool network_change) override {
    DCHECK(session_);
    DCHECK(context_);

    const auto& config = session_->config().doh_config;
    // Start probe sequences for any servers where it is not currently running.
    for (size_t i = 0; i < config.servers().size(); i++) {
      if (!probe_stats_list_[i]) {
        probe_stats_list_[i] = std::make_unique<ProbeStats>();
        ContinueProbe(i, probe_stats_list_[i]->weak_factory.GetWeakPtr(),
                      network_change,
                      base::TimeTicks::Now() /* sequence_start_time */);
      }
    }
  }

  base::TimeDelta GetDelayUntilNextProbeForTest(
      size_t doh_server_index) const override {
    if (doh_server_index >= probe_stats_list_.size() ||
        !probe_stats_list_[doh_server_index])
      return base::TimeDelta();

    return probe_stats_list_[doh_server_index]
        ->backoff_entry->GetTimeUntilRelease();
  }

 private:
  struct ProbeStats {
    ProbeStats()
        : backoff_entry(
              std::make_unique<net::BackoffEntry>(&kProbeBackoffPolicy)) {}

    std::unique_ptr<net::BackoffEntry> backoff_entry;
    std::vector<std::unique_ptr<DnsAttempt>> probe_attempts;
    base::WeakPtrFactory<ProbeStats> weak_factory{this};
  };

  void ContinueProbe(size_t doh_server_index,
                     base::WeakPtr<ProbeStats> probe_stats,
                     bool network_change,
                     base::TimeTicks sequence_start_time) {
    // If the DnsSession or ResolveContext has been destroyed, no reason to
    // continue probing.
    if (!session_ || !context_) {
      probe_stats_list_.clear();
      return;
    }

    // If the ProbeStats for which this probe was scheduled has been deleted,
    // don't continue to send probes.
    if (!probe_stats)
      return;

    // Cancel the probe sequence for this server if the server is already
    // available.
    if (context_->GetDohServerAvailability(doh_server_index, session_.get())) {
      probe_stats_list_[doh_server_index] = nullptr;
      return;
    }

    // Schedule a new probe assuming this one will fail. The newly scheduled
    // probe will not run if an earlier probe has already succeeded. Probes may
    // take awhile to fail, which is why we schedule the next one here rather
    // than on probe completion.
    DCHECK(probe_stats);
    DCHECK(probe_stats->backoff_entry);
    probe_stats->backoff_entry->InformOfRequest(false /* success */);
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DnsOverHttpsProbeRunner::ContinueProbe,
                       weak_ptr_factory_.GetWeakPtr(), doh_server_index,
                       probe_stats, network_change, sequence_start_time),
        probe_stats->backoff_entry->GetTimeUntilRelease());

    uint32_t attempt_number = probe_stats->probe_attempts.size();
    ConstructDnsHTTPAttempt(
        context_->GetWeakPtr(), session_.get(), doh_server_index,
        formatted_probe_qname_, dns_protocol::kTypeA, /*opt_rdata=*/nullptr,
        &probe_stats->probe_attempts, context_->url_request_context(),
        context_->isolation_info(), RequestPriority::DEFAULT_PRIORITY,
        /*is_probe=*/true);

    DnsAttempt* probe_attempt = probe_stats->probe_attempts.back().get();
    probe_attempt->Start(base::BindOnce(
        &DnsOverHttpsProbeRunner::ProbeComplete, weak_ptr_factory_.GetWeakPtr(),
        attempt_number, doh_server_index, std::move(probe_stats),
        network_change, sequence_start_time,
        base::TimeTicks::Now() /* query_start_time */));
  }

  void ProbeComplete(uint32_t attempt_number,
                     size_t doh_server_index,
                     base::WeakPtr<ProbeStats> probe_stats,
                     bool network_change,
                     base::TimeTicks sequence_start_time,
                     base::TimeTicks query_start_time,
                     int rv) {
    bool success = false;
    while (probe_stats && session_ && context_) {
      if (rv != OK) {
        // The DoH probe queries don't go through the standard DnsAttempt path,
        // so the ServerStats have not been updated yet.
        context_->RecordServerFailure(doh_server_index, /*is_doh_server=*/true,
                                      rv, session_.get());
        break;
      }
      // Check that the response parses properly before considering it a
      // success.
      DCHECK_LT(attempt_number, probe_stats->probe_attempts.size());
      const DnsAttempt* attempt =
          probe_stats->probe_attempts[attempt_number].get();
      const DnsResponse* response = attempt->GetResponse();
      if (response) {
        DnsResponseResultExtractor extractor(*response);
        DnsResponseResultExtractor::ResultsOrError results =
            extractor.ExtractDnsResults(
                DnsQueryType::A,
                /*original_domain_name=*/kDohProbeHostname,
                /*request_port=*/0);

        if (results.has_value()) {
          for (const auto& result : results.value()) {
            if (result->type() == HostResolverInternalResult::Type::kData &&
                !result->AsData().endpoints().empty()) {
              context_->RecordServerSuccess(
                  doh_server_index, /*is_doh_server=*/true, session_.get());
              context_->RecordRtt(doh_server_index, /*is_doh_server=*/true,
                                  base::TimeTicks::Now() - query_start_time, rv,
                                  session_.get());
              success = true;

              // Do not delete the ProbeStats and cancel the probe sequence. It
              // will cancel itself on the next scheduled ContinueProbe() call
              // if the server is still available. This way, the backoff
              // schedule will be maintained if a server quickly becomes
              // unavailable again before that scheduled call.
              break;
            }
          }
        }
      }
      if (!success) {
        context_->RecordServerFailure(
            doh_server_index, /*is_doh_server=*/true,
            /*rv=*/ERR_DNS_SECURE_PROBE_RECORD_INVALID, session_.get());
      }
      break;
    }

    base::UmaHistogramLongTimes(
        base::JoinString({"Net.DNS.ProbeSequence",
                          network_change ? "NetworkChange" : "ConfigChange",
                          success ? "Success" : "Failure", "AttemptTime"},
                         "."),
        base::TimeTicks::Now() - sequence_start_time);
  }

  base::WeakPtr<DnsSession> session_;
  base::WeakPtr<ResolveContext> context_;
  std::vector<uint8_t> formatted_probe_qname_;

  // List of ProbeStats, one for each DoH server, indexed by the DoH server
  // config index.
  std::vector<std::unique_ptr<ProbeStats>> probe_stats_list_;

  base::WeakPtrFactory<DnsOverHttpsProbeRunner> weak_ptr_factory_{this};
};

// ----------------------------------------------------------------------------

// Implements DnsTransaction. Configuration is supplied by DnsSession.
// The suffix list is built according to the DnsConfig from the session.
// The fallback period for each DnsUDPAttempt is given by
// ResolveContext::NextClassicFallbackPeriod(). The first server to attempt on
// each query is given by ResolveContext::NextFirstServerIndex, and the order is
// round-robin afterwards. Each server is attempted DnsConfig::attempts times.
class DnsTransactionImpl final : public DnsTransaction {
 public:
  DnsTransactionImpl(
      DnsSession* session,
      std::string hostname,
      uint16_t qtype,
      const NetLogWithSource& parent_net_log,
      // TODO(crbug.com/396483553): Remove `opt_rdata` and all plumbing between
      // DnsTransactionImpl and DnsQuery in a follow-up CL now that structured
      // DNS error (EDE) requests are injected at the DnsQuery layer.
      const OptRecordRdata* opt_rdata,
      DnsTransactionFactory::AttemptMode attempt_mode,
      SecureDnsMode secure_dns_mode,
      ResolveContext* resolve_context,
      bool fast_timeout)
      : session_(session),
        hostname_(std::move(hostname)),
        qtype_(qtype),
        opt_rdata_(opt_rdata),
        attempt_mode_(attempt_mode),
        secure_dns_mode_(secure_dns_mode),
        fast_timeout_(fast_timeout),
        net_log_(NetLogWithSource::Make(NetLog::Get(),
                                        NetLogSourceType::DNS_TRANSACTION)),
        resolve_context_(resolve_context->AsSafeRef()) {
    DCHECK(session_.get());
    DCHECK(!hostname_.empty());
    DCHECK(!IsIPLiteral(hostname_));
    parent_net_log.AddEventReferencingSource(NetLogEventType::DNS_TRANSACTION,
                                             net_log_.source());
  }

  DnsTransactionImpl(const DnsTransactionImpl&) = delete;
  DnsTransactionImpl& operator=(const DnsTransactionImpl&) = delete;

  ~DnsTransactionImpl() override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    if (!callback_.is_null()) {
      net_log_.EndEventWithNetErrorCode(NetLogEventType::DNS_TRANSACTION,
                                        ERR_ABORTED);
    }  // otherwise logged in DoCallback or Start
  }

  const std::string& GetHostname() const override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return hostname_;
  }

  uint16_t GetType() const override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return qtype_;
  }

  void Start(ResponseCallback callback) override {
    DCHECK(!callback.is_null());
    DCHECK(callback_.is_null());
    DCHECK(attempts_.empty());

    callback_ = std::move(callback);

    net_log_.BeginEvent(NetLogEventType::DNS_TRANSACTION,
                        [&] { return NetLogStartParams(hostname_, qtype_); });
    time_from_start_ = std::make_unique<base::ElapsedTimer>();
    AttemptResult result(PrepareSearch(), nullptr);
    if (result.rv == OK) {
      qnames_initial_size_ = qnames_.size();
      result = ProcessAttemptResult(StartQuery());
    }

    // Must always return result asynchronously, to avoid reentrancy.
    if (result.rv != ERR_IO_PENDING) {
      // Clear all other non-completed attempts. They are no longer needed and
      // they may interfere with this posted result.
      ClearAttempts(result.attempt);
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&DnsTransactionImpl::DoCallback,
                                    weak_ptr_factory_.GetWeakPtr(), result));
    }
  }

  void SetRequestPriority(RequestPriority priority) override {
    request_priority_ = priority;
  }

 private:
  // Wrapper for the result of a DnsUDPAttempt.
  struct AttemptResult {
    AttemptResult() = default;
    AttemptResult(int rv, const DnsAttempt* attempt)
        : rv(rv), attempt(attempt) {}

    int rv;
    raw_ptr<const DnsAttempt, AcrossTasksDanglingUntriaged> attempt;
  };

  // Used in UMA (DNS.AttemptType). Do not renumber or remove values.
  enum class DnsAttemptType {
    kUdp = 0,
    kTcpLowEntropy = 1,
    kTcpTruncationRetry = 2,
    kHttp = 3,
    kMaxValue = kHttp,
  };

  // Prepares |qnames_| according to the DnsConfig.
  int PrepareSearch() {
    const DnsConfig& config = session_->config();

    std::optional<std::vector<uint8_t>> labeled_qname =
        dns_names_util::DottedNameToNetwork(
            hostname_,
            /*require_valid_internet_hostname=*/true);
    if (!labeled_qname.has_value())
      return ERR_INVALID_ARGUMENT;

    if (hostname_.back() == '.') {
      // It's a fully-qualified name, no suffix search.
      qnames_.push_back(std::move(labeled_qname).value());
      return OK;
    }

    int ndots = CountLabels(labeled_qname.value()) - 1;

    if (ndots > 0 && !config.append_to_multi_label_name) {
      qnames_.push_back(std::move(labeled_qname).value());
      return OK;
    }

    // Set true when `labeled_qname` is put on the list.
    bool had_qname = false;

    if (ndots >= config.ndots) {
      qnames_.push_back(labeled_qname.value());
      had_qname = true;
    }

    for (const auto& suffix : config.search) {
      std::optional<std::vector<uint8_t>> qname =
          dns_names_util::DottedNameToNetwork(
              hostname_ + "." + suffix,
              /*require_valid_internet_hostname=*/true);
      // Ignore invalid (too long) combinations.
      if (!qname.has_value())
        continue;
      if (qname.value().size() == labeled_qname.value().size()) {
        if (had_qname)
          continue;
        had_qname = true;
      }
      qnames_.push_back(std::move(qname).value());
    }

    if (ndots > 0 && !had_qname)
      qnames_.push_back(std::move(labeled_qname).value());

    return qnames_.empty() ? ERR_DNS_SEARCH_EMPTY : OK;
  }

  void DoCallback(AttemptResult result) {
    DCHECK_NE(ERR_IO_PENDING, result.rv);

    // TODO(mgersh): consider changing back to a DCHECK once
    // https://crbug.com/779589 is fixed.
    if (callback_.is_null())
      return;

    const DnsResponse* response =
        result.attempt ? result.attempt->GetResponse() : nullptr;
    CHECK(result.rv != OK || response != nullptr);

    timer_.Stop();

    net_log_.EndEventWithNetErrorCode(NetLogEventType::DNS_TRANSACTION,
                                      result.rv);

    std::move(callback_).Run(result.rv, response);
  }

  void RecordAttemptUma(DnsAttemptType attempt_type) {
    UMA_HISTOGRAM_ENUMERATION("Net.DNS.DnsTransaction.AttemptType",
                              attempt_type);
  }

  AttemptResult MakeAttempt() {
    DCHECK(MoreAttemptsAllowed());

    DnsConfig config = session_->config();
    switch (attempt_mode_) {
      case DnsTransactionFactory::AttemptMode::kHttp:
        DCHECK(!config.doh_config.servers().empty());
        RecordAttemptUma(DnsAttemptType::kHttp);
        return MakeHTTPAttempt();
      case DnsTransactionFactory::AttemptMode::kClassic:
        DCHECK_GT(config.nameservers.size(), 0u);
        return MakeClassicDnsAttempt();
      default:
        NOTREACHED();
    }
  }

  AttemptResult MakeClassicDnsAttempt() {
    uint16_t id = session_->NextQueryId();
    std::unique_ptr<DnsQuery> query;
    if (attempts_.empty()) {
      query =
          std::make_unique<DnsQuery>(id, qnames_.front(), qtype_, opt_rdata_);
    } else {
      query = attempts_[0]->GetQuery()->CloneWithNewId(id);
    }
    DCHECK(dns_server_iterator_->AttemptAvailable());
    size_t server_index = dns_server_iterator_->GetNextAttemptIndex();

    size_t attempt_number = attempts_.size();
    AttemptResult result;
    if (session_->udp_tracker()->low_entropy()) {
      result = MakeTcpAttempt(server_index, std::move(query));
      RecordAttemptUma(DnsAttemptType::kTcpLowEntropy);
    } else {
      result = MakeUdpAttempt(server_index, std::move(query));
      RecordAttemptUma(DnsAttemptType::kUdp);
    }

    if (result.rv == ERR_IO_PENDING) {
      base::TimeDelta fallback_period =
          resolve_context_->NextClassicFallbackPeriod(
              server_index, attempt_number, session_.get());
      timer_.Start(FROM_HERE, fallback_period, this,
                   &DnsTransactionImpl::OnFallbackPeriodExpired);
    }

    return result;
  }

  // Makes another attempt at the current name, |qnames_.front()|, using the
  // next nameserver.
  AttemptResult MakeUdpAttempt(size_t server_index,
                               std::unique_ptr<DnsQuery> query) {
    DCHECK_EQ(attempt_mode_, DnsTransactionFactory::AttemptMode::kClassic);
    DCHECK(!session_->udp_tracker()->low_entropy());

    const DnsConfig& config = session_->config();
    DCHECK_LT(server_index, config.nameservers.size());
    size_t attempt_number = attempts_.size();

    std::unique_ptr<DatagramClientSocket> socket =
        resolve_context_->url_request_context()
            ->GetNetworkSessionContext()
            ->client_socket_factory->CreateDatagramClientSocket(
                DatagramSocket::RANDOM_BIND, net_log_.net_log(),
                net_log_.source());

    attempts_.push_back(std::make_unique<DnsUDPAttempt>(
        server_index, std::move(socket), config.nameservers[server_index],
        std::move(query), session_->udp_tracker()));
    ++attempts_count_;

    DnsAttempt* attempt = attempts_.back().get();
    net_log_.AddEventReferencingSource(NetLogEventType::DNS_TRANSACTION_ATTEMPT,
                                       attempt->GetSocketNetLog().source());

    int rv = attempt->Start(base::BindOnce(
        &DnsTransactionImpl::OnAttemptComplete, base::Unretained(this),
        attempt_number, true /* record_rtt */, base::TimeTicks::Now()));
    return AttemptResult(rv, attempt);
  }

  AttemptResult MakeHTTPAttempt() {
    DCHECK_EQ(attempt_mode_, DnsTransactionFactory::AttemptMode::kHttp);

    size_t doh_server_index = dns_server_iterator_->GetNextAttemptIndex();

    uint32_t attempt_number = attempts_.size();
    ConstructDnsHTTPAttempt(resolve_context_->GetWeakPtr(), session_.get(),
                            doh_server_index, qnames_.front(), qtype_,
                            opt_rdata_, &attempts_,
                            resolve_context_->url_request_context(),
                            resolve_context_->isolation_info(),
                            request_priority_, /*is_probe=*/false);
    ++attempts_count_;
    DnsAttempt* attempt = attempts_.back().get();
    // Associate this attempt with the DoH request in NetLog.
    net_log_.AddEventReferencingSource(
        NetLogEventType::DNS_TRANSACTION_HTTPS_ATTEMPT,
        attempt->GetSocketNetLog().source());
    attempt->GetSocketNetLog().AddEventReferencingSource(
        NetLogEventType::DNS_TRANSACTION_HTTPS_ATTEMPT, net_log_.source());
    int rv = attempt->Start(base::BindOnce(
        &DnsTransactionImpl::OnAttemptComplete, base::Unretained(this),
        attempt_number, true /* record_rtt */, base::TimeTicks::Now()));
    if (rv == ERR_IO_PENDING) {
      base::TimeDelta fallback_period = resolve_context_->NextDohFallbackPeriod(
          doh_server_index, session_.get());
      timer_.Start(FROM_HERE, fallback_period, this,
                   &DnsTransactionImpl::OnFallbackPeriodExpired);
    }
    return AttemptResult(rv, attempts_.back().get());
  }

  AttemptResult RetryUdpAttemptAsTcp(const DnsAttempt* previous_attempt) {
    DCHECK(previous_attempt);
    DCHECK(!had_tcp_retry_);

    // Only allow a single TCP retry per query.
    had_tcp_retry_ = true;

    size_t server_index = previous_attempt->server_index();
    // Use a new query ID instead of reusing the same one from the UDP attempt.
    // RFC5452, section 9.2 requires an unpredictable ID for all outgoing
    // queries, with no distinction made between queries made via TCP or UDP.
    std::unique_ptr<DnsQuery> query =
        previous_attempt->GetQuery()->CloneWithNewId(session_->NextQueryId());

    // Cancel all attempts that have not received a response, as they will
    // likely similarly require TCP retry.
    ClearAttempts(nullptr);

    AttemptResult result = MakeTcpAttempt(server_index, std::move(query));
    RecordAttemptUma(DnsAttemptType::kTcpTruncationRetry);

    if (result.rv == ERR_IO_PENDING) {
      // On TCP upgrade, use 2x the upgraded fallback period.
      base::TimeDelta fallback_period = timer_.GetCurrentDelay() * 2;
      timer_.Start(FROM_HERE, fallback_period, this,
                   &DnsTransactionImpl::OnFallbackPeriodExpired);
    }

    return result;
  }

  AttemptResult MakeTcpAttempt(size_t server_index,
                               std::unique_ptr<DnsQuery> query) {
    DCHECK_EQ(attempt_mode_, DnsTransactionFactory::AttemptMode::kClassic);
    const DnsConfig& config = session_->config();
    DCHECK_LT(server_index, config.nameservers.size());

    // TODO(crbug.com/40146880): Pass a non-null NetworkQualityEstimator.
    NetworkQualityEstimator* network_quality_estimator = nullptr;

    std::unique_ptr<StreamSocket> socket =
        resolve_context_->url_request_context()
            ->GetNetworkSessionContext()
            ->client_socket_factory->CreateTransportClientSocket(
                AddressList(config.nameservers[server_index]), nullptr,
                network_quality_estimator, net_log_.net_log(),
                net_log_.source());

    uint32_t attempt_number = attempts_.size();

    attempts_.push_back(std::make_unique<DnsTCPAttempt>(
        server_index, std::move(socket), std::move(query)));
    ++attempts_count_;

    DnsAttempt* attempt = attempts_.back().get();
    net_log_.AddEventReferencingSource(
        NetLogEventType::DNS_TRANSACTION_TCP_ATTEMPT,
        attempt->GetSocketNetLog().source());

    int rv = attempt->Start(base::BindOnce(
        &DnsTransactionImpl::OnAttemptComplete, base::Unretained(this),
        attempt_number, false /* record_rtt */, base::TimeTicks::Now()));
    return AttemptResult(rv, attempt);
  }

  // Begins query for the current name. Makes the first attempt.
  AttemptResult StartQuery() {
    std::optional<std::string> dotted_qname =
        dns_names_util::NetworkToDottedName(qnames_.front());
    net_log_.BeginEventWithStringParams(
        NetLogEventType::DNS_TRANSACTION_QUERY, "qname",
        dotted_qname.value_or("???MALFORMED_NAME???"));

    attempts_.clear();
    had_tcp_retry_ = false;
    switch (attempt_mode_) {
      case DnsTransactionFactory::AttemptMode::kHttp:
        dns_server_iterator_ = resolve_context_->GetDohIterator(
            session_->config(), secure_dns_mode_, session_.get());
        break;
      case DnsTransactionFactory::AttemptMode::kClassic:
        dns_server_iterator_ = resolve_context_->GetClassicDnsIterator(
            session_->config(), session_.get());
        break;
      default:
        NOTREACHED();
    }
    DCHECK(dns_server_iterator_);
    // Check for available server before starting as DoH servers might be
    // unavailable.
    if (!dns_server_iterator_->AttemptAvailable())
      return AttemptResult(ERR_BLOCKED_BY_CLIENT, nullptr);

    return MakeAttempt();
  }

  void OnAttemptComplete(uint32_t attempt_number,
                         bool record_rtt,
                         base::TimeTicks start,
                         int rv) {
    DCHECK_LT(attempt_number, attempts_.size());
    const DnsAttempt* attempt = attempts_[attempt_number].get();
    if (record_rtt && attempt->GetResponse()) {
      resolve_context_->RecordRtt(
          attempt->server_index(),
          attempt_mode_ ==
              DnsTransactionFactory::AttemptMode::kHttp /* is_doh_server */,
          base::TimeTicks::Now() - start, rv, session_.get());
    }
    if (callback_.is_null())
      return;
    AttemptResult result = ProcessAttemptResult(AttemptResult(rv, attempt));
    if (result.rv != ERR_IO_PENDING)
      DoCallback(result);
  }

  void LogResponse(const DnsAttempt* attempt) {
    if (attempt) {
      net_log_.AddEvent(NetLogEventType::DNS_TRANSACTION_RESPONSE,
                        [&](NetLogCaptureMode capture_mode) {
                          return attempt->NetLogResponseParams(capture_mode);
                        });
    }
  }

  bool MoreAttemptsAllowed() const {
    if (had_tcp_retry_)
      return false;

    return dns_server_iterator_->AttemptAvailable();
  }

  // Resolves the result of a DnsAttempt until a terminal result is reached
  // or it will complete asynchronously (ERR_IO_PENDING).
  AttemptResult ProcessAttemptResult(AttemptResult result) {
    while (result.rv != ERR_IO_PENDING) {
      LogResponse(result.attempt);

      switch (result.rv) {
        case OK:
          resolve_context_->RecordServerSuccess(
              result.attempt->server_index(),
              attempt_mode_ ==
                  DnsTransactionFactory::AttemptMode::kHttp /* is_doh_server */,
              session_.get());
          net_log_.EndEventWithNetErrorCode(
              NetLogEventType::DNS_TRANSACTION_QUERY, result.rv);
          DCHECK(result.attempt);
          DCHECK(result.attempt->GetResponse());
          return result;
        case ERR_NAME_NOT_RESOLVED:
          resolve_context_->RecordServerSuccess(
              result.attempt->server_index(),
              attempt_mode_ ==
                  DnsTransactionFactory::AttemptMode::kHttp /* is_doh_server */,
              session_.get());
          net_log_.EndEventWithNetErrorCode(
              NetLogEventType::DNS_TRANSACTION_QUERY, result.rv);
          // Try next suffix. Check that qnames_ isn't already empty first,
          // which can happen when there are two attempts running at once.
          // TODO(mgersh): remove this workaround for https://crbug.com/774846
          // when https://crbug.com/779589 is fixed.
          if (!qnames_.empty())
            qnames_.pop_front();
          if (qnames_.empty()) {
            return result;
          } else {
            result = StartQuery();
          }
          break;
        case ERR_DNS_TIMED_OUT:
          timer_.Stop();

          if (result.attempt) {
            DCHECK(result.attempt == attempts_.back().get());
            resolve_context_->RecordServerFailure(
                result.attempt->server_index(),
                attempt_mode_ ==
                    DnsTransactionFactory::AttemptMode::kHttp /* is_doh_server */,
                result.rv, session_.get());
          }
          if (MoreAttemptsAllowed()) {
            result = MakeAttempt();
            break;
          }

          if (!fast_timeout_ && AnyAttemptPending()) {
            StartTimeoutTimer();
            return AttemptResult(ERR_IO_PENDING, nullptr);
          }

          return result;
        case ERR_DNS_SERVER_REQUIRES_TCP:
          result = RetryUdpAttemptAsTcp(result.attempt);
          break;
        case ERR_BLOCKED_BY_CLIENT:
          net_log_.EndEventWithNetErrorCode(
              NetLogEventType::DNS_TRANSACTION_QUERY, result.rv);
          return result;
        default:
          // Server failure.
          DCHECK(result.attempt);

          // If attempt is not the most recent attempt, means this error is for
          // a previous attempt that already passed its fallback period and
          // continued attempting in parallel with new attempts (see the
          // ERR_DNS_TIMED_OUT case above). As the failure was already recorded
          // at fallback time and is no longer being waited on, ignore this
          // failure.
          if (result.attempt == attempts_.back().get()) {
            timer_.Stop();
            resolve_context_->RecordServerFailure(
                result.attempt->server_index(),
                attempt_mode_ ==
                    DnsTransactionFactory::AttemptMode::kHttp /* is_doh_server */,
                result.rv, session_.get());

            if (MoreAttemptsAllowed()) {
              result = MakeAttempt();
              break;
            }

            if (fast_timeout_) {
              return result;
            }

            // No more attempts can be made, but there may be other attempts
            // still pending, so start the timeout timer.
            StartTimeoutTimer();
          }

          // If any attempts are still pending, continue to wait for them.
          if (AnyAttemptPending()) {
            DCHECK(timer_.IsRunning());
            return AttemptResult(ERR_IO_PENDING, nullptr);
          }

          return result;
      }
    }
    return result;
  }

  // Clears and cancels all pending attempts. If |leave_attempt| is not
  // null, that attempt is not cleared even if pending.
  void ClearAttempts(const DnsAttempt* leave_attempt) {
    for (auto it = attempts_.begin(); it != attempts_.end();) {
      if ((*it)->IsPending() && it->get() != leave_attempt) {
        it = attempts_.erase(it);
      } else {
        ++it;
      }
    }
  }

  bool AnyAttemptPending() {
    return std::ranges::any_of(attempts_,
                               [](std::unique_ptr<DnsAttempt>& attempt) {
                                 return attempt->IsPending();
                               });
  }

  void OnFallbackPeriodExpired() {
    if (callback_.is_null())
      return;
    DCHECK(!attempts_.empty());
    AttemptResult result = ProcessAttemptResult(
        AttemptResult(ERR_DNS_TIMED_OUT, attempts_.back().get()));
    if (result.rv != ERR_IO_PENDING)
      DoCallback(result);
  }

  void StartTimeoutTimer() {
    DCHECK(!fast_timeout_);
    DCHECK(!timer_.IsRunning());
    DCHECK(!callback_.is_null());

    base::TimeDelta timeout;
    switch (attempt_mode_) {
      case DnsTransactionFactory::AttemptMode::kHttp:
        timeout = resolve_context_->SecureTransactionTimeout(secure_dns_mode_,
                                                             session_.get());
        break;
      case DnsTransactionFactory::AttemptMode::kClassic:
        timeout = resolve_context_->ClassicTransactionTimeout(session_.get());
        break;
      default:
        NOTREACHED();
    }
    timeout -= time_from_start_->Elapsed();

    timer_.Start(FROM_HERE, timeout, this, &DnsTransactionImpl::OnTimeout);
  }

  void OnTimeout() {
    if (callback_.is_null())
      return;
    DoCallback(AttemptResult(ERR_DNS_TIMED_OUT, nullptr));
  }

  scoped_refptr<DnsSession> session_;
  std::string hostname_;
  uint16_t qtype_;
  raw_ptr<const OptRecordRdata, DanglingUntriaged> opt_rdata_;
  const DnsTransactionFactory::AttemptMode attempt_mode_;
  const SecureDnsMode secure_dns_mode_;
  // Cleared in DoCallback.
  ResponseCallback callback_;

  // When true, transaction should time out immediately on expiration of the
  // last attempt fallback period rather than waiting the overall transaction
  // timeout period.
  const bool fast_timeout_;

  NetLogWithSource net_log_;

  // Search list of fully-qualified DNS names to query next (in DNS format).
  base::circular_deque<std::vector<uint8_t>> qnames_;
  size_t qnames_initial_size_ = 0;

  // List of attempts for the current name.
  std::vector<std::unique_ptr<DnsAttempt>> attempts_;
  // Count of attempts, not reset when |attempts_| vector is cleared.
  int attempts_count_ = 0;

  // Records when an attempt was retried via TCP due to a truncation error.
  bool had_tcp_retry_ = false;

  // Iterator to get the index of the DNS server for each search query.
  std::unique_ptr<DnsServerIterator> dns_server_iterator_;

  base::OneShotTimer timer_;
  std::unique_ptr<base::ElapsedTimer> time_from_start_;

  base::SafeRef<ResolveContext> resolve_context_;
  RequestPriority request_priority_ = DEFAULT_PRIORITY;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<DnsTransactionImpl> weak_ptr_factory_{this};
};

// ----------------------------------------------------------------------------

// Implementation of DnsTransactionFactory that returns instances of
// DnsTransactionImpl.
class DnsTransactionFactoryImpl : public DnsTransactionFactory {
 public:
  explicit DnsTransactionFactoryImpl(DnsSession* session) {
    session_ = session;
  }

  std::unique_ptr<DnsTransaction> CreateTransaction(
      std::string hostname,
      uint16_t qtype,
      const NetLogWithSource& net_log,
      AttemptMode attempt_mode,
      SecureDnsMode secure_dns_mode,
      ResolveContext* resolve_context,
      bool fast_timeout) override {
    return std::make_unique<DnsTransactionImpl>(
        session_.get(), std::move(hostname), qtype, net_log,
        // No factory-level EDNS option injection; per-transaction options are
        // passed through other call sites when needed.
        /*opt_rdata=*/nullptr, attempt_mode, secure_dns_mode, resolve_context,
        fast_timeout);
  }

  std::unique_ptr<DnsProbeRunner> CreateDohProbeRunner(
      ResolveContext* resolve_context) override {
    // Start a timer that will emit metrics after a timeout to indicate whether
    // DoH auto-upgrade was successful for this session.
    resolve_context->StartDohAutoupgradeSuccessTimer(session_.get());

    return std::make_unique<DnsOverHttpsProbeRunner>(
        session_->GetWeakPtr(), resolve_context->GetWeakPtr());
  }

  SecureDnsMode GetSecureDnsModeForTest() override {
    return session_->config().secure_dns_mode;
  }

 private:
  scoped_refptr<DnsSession> session_;
};

}  // namespace

DnsTransactionFactory::DnsTransactionFactory() = default;
DnsTransactionFactory::~DnsTransactionFactory() = default;

// static
std::unique_ptr<DnsTransactionFactory> DnsTransactionFactory::CreateFactory(
    DnsSession* session) {
  return std::make_unique<DnsTransactionFactoryImpl>(session);
}

}  // namespace net
