// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_stream_factory.h"

#include <algorithm>
#include <memory>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "crypto/openssl_util.h"
#include "net/base/features.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/base/trace_constants.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/ct_verifier.h"
#include "net/dns/host_resolver.h"
#include "net/dns/public/secure_dns_mode.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source_type.h"
#include "net/quic/address_utils.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/quic/properties_based_quic_server_info.h"
#include "net/quic/quic_chromium_alarm_factory.h"
#include "net/quic/quic_chromium_connection_helper.h"
#include "net/quic/quic_chromium_packet_reader.h"
#include "net/quic/quic_chromium_packet_writer.h"
#include "net/quic/quic_client_session_cache.h"
#include "net/quic/quic_context.h"
#include "net/quic/quic_crypto_client_stream_factory.h"
#include "net/quic/quic_http_stream.h"
#include "net/quic/quic_server_info.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_performance_watcher.h"
#include "net/socket/socket_performance_watcher_factory.h"
#include "net/socket/udp_client_socket.h"
#include "net/third_party/quiche/src/quic/core/crypto/null_decrypter.h"
#include "net/third_party/quiche/src/quic/core/crypto/proof_verifier.h"
#include "net/third_party/quiche/src/quic/core/crypto/quic_random.h"
#include "net/third_party/quiche/src/quic/core/http/quic_client_promised_info.h"
#include "net/third_party/quiche/src/quic/core/http/quic_client_push_promise_index.h"
#include "net/third_party/quiche/src/quic/core/quic_clock.h"
#include "net/third_party/quiche/src/quic/core/quic_connection.h"
#include "net/third_party/quiche/src/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_flags.h"
#include "third_party/boringssl/src/include/openssl/aead.h"
#include "url/gurl.h"
#include "url/url_constants.h"

using NetworkHandle = net::NetworkChangeNotifier::NetworkHandle;

namespace net {

// Returns the estimate of dynamically allocated memory of an IPEndPoint in
// bytes. Used in tracking IPAliasMap.
size_t EstimateMemoryUsage(const IPEndPoint& end_point) {
  return 0;
}

namespace {

enum CreateSessionFailure {
  CREATION_ERROR_CONNECTING_SOCKET,
  CREATION_ERROR_SETTING_RECEIVE_BUFFER,
  CREATION_ERROR_SETTING_SEND_BUFFER,
  CREATION_ERROR_SETTING_DO_NOT_FRAGMENT,
  CREATION_ERROR_MAX
};

enum InitialRttEstimateSource {
  INITIAL_RTT_DEFAULT,
  INITIAL_RTT_CACHED,
  INITIAL_RTT_2G,
  INITIAL_RTT_3G,
  INITIAL_RTT_SOURCE_MAX,
};

// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "ConnectionStateAfterDNS" in src/tools/metrics/histograms/enums.xml.
enum class ConnectionStateAfterDNS {
  kDNSFailed = 0,
  kIpPooled = 1,
  kWaitingForCryptoDnsMatched = 2,
  kWaitingForCryptoDnsNoMatch = 3,
  kCryptoFinishedDnsMatch = 4,
  kCryptoFinishedDnsNoMatch = 5,
  kMaxValue = kCryptoFinishedDnsNoMatch,
};

base::Value NetLogQuicStreamFactoryJobParams(
    const QuicStreamFactory::QuicSessionAliasKey* key) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("host", key->server_id().host());
  dict.SetIntKey("port", key->server_id().port());
  dict.SetStringKey("privacy_mode", PrivacyModeToDebugString(
                                        key->session_key().privacy_mode()));
  dict.SetStringKey("network_isolation_key",
                    key->session_key().network_isolation_key().ToDebugString());
  return dict;
}

std::string QuicPlatformNotificationToString(
    QuicPlatformNotification notification) {
  switch (notification) {
    case NETWORK_CONNECTED:
      return "OnNetworkConnected";
    case NETWORK_MADE_DEFAULT:
      return "OnNetworkMadeDefault";
    case NETWORK_DISCONNECTED:
      return "OnNetworkDisconnected";
    case NETWORK_SOON_TO_DISCONNECT:
      return "OnNetworkSoonToDisconnect";
    case NETWORK_IP_ADDRESS_CHANGED:
      return "OnIPAddressChanged";
    default:
      QUIC_NOTREACHED();
      break;
  }
  return "InvalidNotification";
}

void HistogramCreateSessionFailure(enum CreateSessionFailure error) {
  UMA_HISTOGRAM_ENUMERATION("Net.QuicSession.CreationError", error,
                            CREATION_ERROR_MAX);
}

void LogConnectionIpPooling(bool pooled) {
  UMA_HISTOGRAM_BOOLEAN("Net.QuicSession.ConnectionIpPooled", pooled);
}

void LogRacingStatus(ConnectionStateAfterDNS status) {
  UMA_HISTOGRAM_ENUMERATION("Net.QuicSession.ConnectionStateAfterDNS", status);
}

void LogStaleConnectionTime(base::TimeTicks start_time) {
  UMA_HISTOGRAM_TIMES("Net.QuicSession.StaleConnectionTime",
                      base::TimeTicks::Now() - start_time);
}

void LogValidConnectionTime(base::TimeTicks start_time) {
  UMA_HISTOGRAM_TIMES("Net.QuicSession.ValidConnectionTime",
                      base::TimeTicks::Now() - start_time);
}

void LogFreshDnsResolveTime(base::TimeTicks start_time) {
  UMA_HISTOGRAM_TIMES("Net.QuicSession.FreshDnsResolutionTime",
                      base::TimeTicks::Now() - start_time);
}

void SetInitialRttEstimate(base::TimeDelta estimate,
                           enum InitialRttEstimateSource source,
                           quic::QuicConfig* config) {
  UMA_HISTOGRAM_ENUMERATION("Net.QuicSession.InitialRttEsitmateSource", source,
                            INITIAL_RTT_SOURCE_MAX);
  if (estimate != base::TimeDelta())
    config->SetInitialRoundTripTimeUsToSend(estimate.InMicroseconds());
}

// An implementation of quic::QuicCryptoClientConfig::ServerIdFilter that wraps
// an |origin_filter|.
class ServerIdOriginFilter
    : public quic::QuicCryptoClientConfig::ServerIdFilter {
 public:
  explicit ServerIdOriginFilter(
      const base::RepeatingCallback<bool(const GURL&)> origin_filter)
      : origin_filter_(origin_filter) {}

  bool Matches(const quic::QuicServerId& server_id) const override {
    if (origin_filter_.is_null())
      return true;

    GURL url(base::StringPrintf("%s%s%s:%d", url::kHttpsScheme,
                                url::kStandardSchemeSeparator,
                                server_id.host().c_str(), server_id.port()));
    DCHECK(url.is_valid());
    return origin_filter_.Run(url);
  }

 private:
  const base::RepeatingCallback<bool(const GURL&)> origin_filter_;
};

std::set<std::string> HostsFromOrigins(std::set<HostPortPair> origins) {
  std::set<std::string> hosts;
  for (const auto& origin : origins) {
    hosts.insert(origin.host());
  }
  return hosts;
}

}  // namespace

// Refcounted class that owns quic::QuicCryptoClientConfig and tracks how many
// consumers are using it currently. When the last reference is freed, the
// QuicCryptoClientConfigHandle informs the owning QuicStreamFactory, moves it
// into an MRU cache.
class QuicStreamFactory::QuicCryptoClientConfigOwner {
 public:
  QuicCryptoClientConfigOwner(
      std::unique_ptr<quic::ProofVerifier> proof_verifier,
      std::unique_ptr<QuicClientSessionCache> session_cache,
      QuicStreamFactory* quic_stream_factory)
      : config_(std::move(proof_verifier), std::move(session_cache)),
        quic_stream_factory_(quic_stream_factory) {
    DCHECK(quic_stream_factory_);
  }

  ~QuicCryptoClientConfigOwner() { DCHECK_EQ(num_refs_, 0); }

  quic::QuicCryptoClientConfig* config() { return &config_; }

  int num_refs() const { return num_refs_; }

  QuicStreamFactory* quic_stream_factory() { return quic_stream_factory_; }

 private:
  friend class CryptoClientConfigHandle;

  // Simple ref counting. Not using scoped_refptr allows for both keeping around
  // an MRU cache of 0-reference objects, and DCHECKing that there are no
  // outstanding referenced QuicCryptoClientConfigOwner on destruction. Private
  // so that only CryptoClientConfigHandle can add and remove refs.

  void AddRef() { num_refs_++; }

  void ReleaseRef() {
    DCHECK_GT(num_refs_, 0);
    num_refs_--;
  }

  int num_refs_ = 0;
  quic::QuicCryptoClientConfig config_;
  QuicStreamFactory* const quic_stream_factory_;

  DISALLOW_COPY_AND_ASSIGN(QuicCryptoClientConfigOwner);
};

// Class that owns a reference to a QuicCryptoClientConfigOwner. Handles
// incrementing the refcount on construction, and decrementing it on
// destruction.
class QuicStreamFactory::CryptoClientConfigHandle
    : public QuicCryptoClientConfigHandle {
 public:
  explicit CryptoClientConfigHandle(
      const QuicCryptoClientConfigMap::iterator& map_iterator)
      : map_iterator_(map_iterator) {
    DCHECK_GE(map_iterator_->second->num_refs(), 0);
    map_iterator->second->AddRef();
  }

  explicit CryptoClientConfigHandle(const CryptoClientConfigHandle& other)
      : CryptoClientConfigHandle(other.map_iterator_) {}

  ~CryptoClientConfigHandle() override {
    DCHECK_GT(map_iterator_->second->num_refs(), 0);
    map_iterator_->second->ReleaseRef();
    if (map_iterator_->second->num_refs() == 0) {
      map_iterator_->second->quic_stream_factory()
          ->OnAllCryptoClientRefReleased(map_iterator_);
    }
  }

  quic::QuicCryptoClientConfig* GetConfig() const override {
    return map_iterator_->second->config();
  }

 private:
  QuicCryptoClientConfigMap::iterator map_iterator_;

  DISALLOW_ASSIGN(CryptoClientConfigHandle);
};

// Responsible for creating a new QUIC session to the specified server, and
// for notifying any associated requests when complete. |client_config_handle|
// is not actually used, but serves to keep the corresponding CryptoClientConfig
// alive until the Job completes.
class QuicStreamFactory::Job {
 public:
  Job(QuicStreamFactory* factory,
      quic::ParsedQuicVersion quic_version,
      HostResolver* host_resolver,
      const QuicSessionAliasKey& key,
      std::unique_ptr<CryptoClientConfigHandle> client_config_handle,
      bool was_alternative_service_recently_broken,
      bool retry_on_alternate_network_before_handshake,
      bool race_stale_dns_on_connection,
      RequestPriority priority,
      int cert_verify_flags,
      const NetLogWithSource& net_log);

  ~Job();

  int Run(CompletionOnceCallback callback);

  int DoLoop(int rv);
  int DoResolveHost();
  int DoResolveHostComplete(int rv);
  int DoConnect();
  int DoConnectComplete(int rv);
  int DoConfirmConnection(int rv);
  int DoValidateHost();

  void OnResolveHostComplete(int rv);
  void OnConnectComplete(int rv);
  void OnSessionClosed(QuicChromiumClientSession* session);

  const QuicSessionAliasKey& key() const { return key_; }

  const NetLogWithSource& net_log() const { return net_log_; }

  base::WeakPtr<Job> GetWeakPtr() { return weak_factory_.GetWeakPtr(); }

  void PopulateNetErrorDetails(NetErrorDetails* details) const;

  // Returns the estimate of dynamically allocated memory in bytes.
  size_t EstimateMemoryUsage() const;

  void AddRequest(QuicStreamRequest* request) {
    stream_requests_.insert(request);
    if (!host_resolution_finished_) {
      request->ExpectOnHostResolution();
    }
  }

  void RemoveRequest(QuicStreamRequest* request) {
    auto request_iter = stream_requests_.find(request);
    DCHECK(request_iter != stream_requests_.end());
    stream_requests_.erase(request_iter);
  }

  void SetPriority(RequestPriority priority) {
    if (priority_ == priority)
      return;

    priority_ = priority;
    if (resolve_host_request_ && !host_resolution_finished_) {
      if (fresh_resolve_host_request_) {
        fresh_resolve_host_request_->ChangeRequestPriority(priority);
      } else {
        resolve_host_request_->ChangeRequestPriority(priority);
      }
    }
  }

  const std::set<QuicStreamRequest*>& stream_requests() {
    return stream_requests_;
  }

  RequestPriority priority() const { return priority_; }

 private:
  enum IoState {
    STATE_NONE,
    STATE_RESOLVE_HOST,
    STATE_RESOLVE_HOST_COMPLETE,
    STATE_CONNECT,
    STATE_CONNECT_COMPLETE,
    STATE_HOST_VALIDATION,
    STATE_CONFIRM_CONNECTION,
  };

  void CloseStaleHostConnection() {
    DVLOG(1) << "Closing connection from stale host.";
    if (session_) {
      QuicChromiumClientSession* session = session_;
      session_ = nullptr;
      // Use ERR_FAILED instead of ERR_ABORTED out of paranoia - ERR_ABORTED
      // should only be used when the next layer up cancels a request, and has
      // special semantic meaning for some consumers when they see it.
      session->CloseSessionOnErrorLater(
          ERR_FAILED, quic::QUIC_STALE_CONNECTION_CANCELLED,
          quic::ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
    }
  }

  bool DoesPeerAddressMatchWithFreshAddressList() {
    if (!session_)
      return false;

    std::vector<net::IPEndPoint> endpoints =
        fresh_resolve_host_request_->GetAddressResults().value().endpoints();

    IPEndPoint stale_address =
        resolve_host_request_->GetAddressResults().value().front();

    if (std::find(endpoints.begin(), endpoints.end(), stale_address) !=
        endpoints.end()) {
      return true;
    }
    return false;
  }

  void LogStaleHostRacing(bool used) {
    if (used) {
      net_log_.AddEvent(
          NetLogEventType::
              QUIC_STREAM_FACTORY_JOB_STALE_HOST_TRIED_ON_CONNECTION);
    } else {
      net_log_.AddEvent(
          NetLogEventType::
              QUIC_STREAM_FACTORY_JOB_STALE_HOST_NOT_USED_ON_CONNECTION);
    }
    UMA_HISTOGRAM_BOOLEAN("Net.QuicSession.StaleHostRacing", used);
  }

  void LogStaleAndFreshHostMatched(bool matched) {
    if (matched) {
      net_log_.AddEvent(
          NetLogEventType::
              QUIC_STREAM_FACTORY_JOB_STALE_HOST_RESOLUTION_MATCHED);
    } else {
      net_log_.AddEvent(
          NetLogEventType::
              QUIC_STREAM_FACTORY_JOB_STALE_HOST_RESOLUTION_NO_MATCH);
    }
    UMA_HISTOGRAM_BOOLEAN("Net.QuicSession.StaleAndFreshHostMatched", matched);
  }

  IoState io_state_;
  QuicStreamFactory* factory_;
  quic::ParsedQuicVersion quic_version_;
  HostResolver* host_resolver_;
  const QuicSessionAliasKey key_;
  const std::unique_ptr<CryptoClientConfigHandle> client_config_handle_;
  RequestPriority priority_;
  const int cert_verify_flags_;
  const bool was_alternative_service_recently_broken_;
  const bool retry_on_alternate_network_before_handshake_;
  const bool race_stale_dns_on_connection_;
  const NetLogWithSource net_log_;
  bool host_resolution_finished_;
  bool connection_retried_;
  QuicChromiumClientSession* session_;
  // If connection migraiton is supported, |network_| denotes the network on
  // which |session_| is created.
  NetworkChangeNotifier::NetworkHandle network_;
  CompletionOnceCallback host_resolution_callback_;
  CompletionOnceCallback callback_;
  std::unique_ptr<HostResolver::ResolveHostRequest> resolve_host_request_;
  // Only set during DNS race. After completion, cleared or replaces
  // |resolve_host_request_|.
  std::unique_ptr<HostResolver::ResolveHostRequest> fresh_resolve_host_request_;
  base::TimeTicks dns_resolution_start_time_;
  base::TimeTicks dns_resolution_end_time_;
  base::TimeTicks quic_connection_start_time_;
  std::set<QuicStreamRequest*> stream_requests_;
  base::WeakPtrFactory<Job> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(Job);
};

QuicStreamFactory::Job::Job(
    QuicStreamFactory* factory,
    quic::ParsedQuicVersion quic_version,
    HostResolver* host_resolver,
    const QuicSessionAliasKey& key,
    std::unique_ptr<CryptoClientConfigHandle> client_config_handle,
    bool was_alternative_service_recently_broken,
    bool retry_on_alternate_network_before_handshake,
    bool race_stale_dns_on_connection,
    RequestPriority priority,
    int cert_verify_flags,
    const NetLogWithSource& net_log)
    : io_state_(STATE_RESOLVE_HOST),
      factory_(factory),
      quic_version_(quic_version),
      host_resolver_(host_resolver),
      key_(key),
      client_config_handle_(std::move(client_config_handle)),
      priority_(priority),
      cert_verify_flags_(cert_verify_flags),
      was_alternative_service_recently_broken_(
          was_alternative_service_recently_broken),
      retry_on_alternate_network_before_handshake_(
          retry_on_alternate_network_before_handshake),
      race_stale_dns_on_connection_(race_stale_dns_on_connection),
      net_log_(
          NetLogWithSource::Make(net_log.net_log(),
                                 NetLogSourceType::QUIC_STREAM_FACTORY_JOB)),
      host_resolution_finished_(false),
      connection_retried_(false),
      session_(nullptr),
      network_(NetworkChangeNotifier::kInvalidNetworkHandle) {
  net_log_.BeginEvent(NetLogEventType::QUIC_STREAM_FACTORY_JOB,
                      [&] { return NetLogQuicStreamFactoryJobParams(&key_); });
  // Associate |net_log_| with |net_log|.
  net_log_.AddEventReferencingSource(
      NetLogEventType::QUIC_STREAM_FACTORY_JOB_BOUND_TO_HTTP_STREAM_JOB,
      net_log.source());
  net_log.AddEventReferencingSource(
      NetLogEventType::HTTP_STREAM_JOB_BOUND_TO_QUIC_STREAM_FACTORY_JOB,
      net_log_.source());
}

QuicStreamFactory::Job::~Job() {
  net_log_.EndEvent(NetLogEventType::QUIC_STREAM_FACTORY_JOB);
  // If |this| is destroyed in QuicStreamFactory's destructor, |callback_| is
  // non-null.
}

int QuicStreamFactory::Job::Run(CompletionOnceCallback callback) {
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    callback_ = std::move(callback);

  return rv > 0 ? OK : rv;
}

int QuicStreamFactory::Job::DoLoop(int rv) {
  TRACE_EVENT0(NetTracingCategory(), "QuicStreamFactory::Job::DoLoop");

  do {
    IoState state = io_state_;
    io_state_ = STATE_NONE;
    switch (state) {
      case STATE_RESOLVE_HOST:
        CHECK_EQ(OK, rv);
        rv = DoResolveHost();
        break;
      case STATE_RESOLVE_HOST_COMPLETE:
        rv = DoResolveHostComplete(rv);
        break;
      case STATE_CONNECT:
        CHECK_EQ(OK, rv);
        rv = DoConnect();
        break;
      case STATE_CONNECT_COMPLETE:
        rv = DoConnectComplete(rv);
        break;
      case STATE_HOST_VALIDATION:
        rv = DoValidateHost();
        break;
      case STATE_CONFIRM_CONNECTION:
        rv = DoConfirmConnection(rv);
        break;
      default:
        NOTREACHED() << "io_state_: " << io_state_;
        break;
    }
  } while (io_state_ != STATE_NONE && rv != ERR_IO_PENDING);
  return rv;
}

void QuicStreamFactory::Job::OnSessionClosed(
    QuicChromiumClientSession* session) {
  // When dns racing experiment is on, the job needs to know that the stale
  // session is closed so that it will start the fresh session without matching
  // dns results.
  if (io_state_ == STATE_HOST_VALIDATION && session_ == session) {
    DCHECK(race_stale_dns_on_connection_);
    DCHECK(fresh_resolve_host_request_);
    resolve_host_request_ = std::move(fresh_resolve_host_request_);
    session_ = nullptr;
    io_state_ = STATE_RESOLVE_HOST_COMPLETE;
  }
}

void QuicStreamFactory::Job::OnResolveHostComplete(int rv) {
  DCHECK(!host_resolution_finished_);

  LogFreshDnsResolveTime(dns_resolution_start_time_);

  if (fresh_resolve_host_request_) {
    DCHECK(race_stale_dns_on_connection_);
    dns_resolution_end_time_ = base::TimeTicks::Now();
    if (rv != OK) {
      LogRacingStatus(ConnectionStateAfterDNS::kDNSFailed);
      CloseStaleHostConnection();
      resolve_host_request_ = std::move(fresh_resolve_host_request_);
      io_state_ = STATE_RESOLVE_HOST_COMPLETE;
    } else if (factory_->HasMatchingIpSession(
                   key_,
                   fresh_resolve_host_request_->GetAddressResults().value())) {
      // Session with resolved IP has already existed, so close racing
      // connection, run callback, and return.
      LogRacingStatus(ConnectionStateAfterDNS::kIpPooled);
      LogConnectionIpPooling(true);
      CloseStaleHostConnection();
      if (!callback_.is_null())
        std::move(callback_).Run(OK);
      return;
    } else if (io_state_ != STATE_HOST_VALIDATION) {
      // Case where host resolution returns successfully, but stale connection
      // hasn't finished yet.
      if (DoesPeerAddressMatchWithFreshAddressList()) {
        LogRacingStatus(ConnectionStateAfterDNS::kWaitingForCryptoDnsMatched);
        LogStaleAndFreshHostMatched(true);
        fresh_resolve_host_request_ = nullptr;
        return;
      }
      LogRacingStatus(ConnectionStateAfterDNS::kWaitingForCryptoDnsNoMatch);
      LogStaleAndFreshHostMatched(false);
      CloseStaleHostConnection();
      resolve_host_request_ = std::move(fresh_resolve_host_request_);
      io_state_ = STATE_RESOLVE_HOST_COMPLETE;
    }  // Else stale connection has already finished successfully.
  } else {
    // If not in DNS race, we should have been waiting for this callback in
    // STATE_RESOLVE_HOST_COMPLETE.
    DCHECK_EQ(STATE_RESOLVE_HOST_COMPLETE, io_state_);
  }

  rv = DoLoop(rv);

  // Expect to be marked by either DoResolveHostComplete() or DoValidateHost().
  DCHECK(host_resolution_finished_);

  // DNS race should be completed either above or by DoValidateHost().
  DCHECK(!fresh_resolve_host_request_);

  for (auto* request : stream_requests_) {
    request->OnHostResolutionComplete(rv);
  }

  if (rv != ERR_IO_PENDING && !callback_.is_null())
    std::move(callback_).Run(rv);
}

void QuicStreamFactory::Job::OnConnectComplete(int rv) {
  // This early return will be triggered when CloseSessionOnError is called
  // before crypto handshake has completed.
  if (!session_) {
    LogStaleConnectionTime(quic_connection_start_time_);
    return;
  }

  rv = DoLoop(rv);
  if (rv != ERR_IO_PENDING && !callback_.is_null())
    std::move(callback_).Run(rv);
}

void QuicStreamFactory::Job::PopulateNetErrorDetails(
    NetErrorDetails* details) const {
  if (!session_)
    return;
  details->connection_info = QuicHttpStream::ConnectionInfoFromQuicVersion(
      session_->connection()->version());
  details->quic_connection_error = session_->error();
}

size_t QuicStreamFactory::Job::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(key_);
}

int QuicStreamFactory::Job::DoResolveHost() {
  dns_resolution_start_time_ = base::TimeTicks::Now();

  io_state_ = STATE_RESOLVE_HOST_COMPLETE;

  HostResolver::ResolveHostParameters parameters;
  parameters.initial_priority = priority_;
  if (race_stale_dns_on_connection_) {
    // Allow host resolver to return stale result immediately.
    parameters.cache_usage =
        HostResolver::ResolveHostParameters::CacheUsage::STALE_ALLOWED;
  }
  if (key_.session_key().disable_secure_dns())
    parameters.secure_dns_mode_override = SecureDnsMode::kOff;
  resolve_host_request_ = host_resolver_->CreateRequest(
      key_.destination(), key_.session_key().network_isolation_key(), net_log_,
      parameters);
  // Unretained is safe because |this| owns the request, ensuring cancellation
  // on destruction.
  // When race_stale_dns_on_connection_ is on, this request will query for stale
  // cache if no fresh host result is available.
  int rv = resolve_host_request_->Start(base::BindOnce(
      &QuicStreamFactory::Job::OnResolveHostComplete, base::Unretained(this)));

  if (rv == ERR_IO_PENDING || !resolve_host_request_->GetStaleInfo() ||
      !resolve_host_request_->GetStaleInfo().value().is_stale()) {
    // Returns non-stale result synchronously.
    if (rv != ERR_IO_PENDING) {
      LogFreshDnsResolveTime(dns_resolution_start_time_);
    }
    // Not a stale result.
    if (race_stale_dns_on_connection_)
      LogStaleHostRacing(false);
    return rv;
  }

  // If request resulted in a stale cache entry, start request for fresh results
  DCHECK(race_stale_dns_on_connection_);

  parameters.cache_usage =
      HostResolver::ResolveHostParameters::CacheUsage::DISALLOWED;
  fresh_resolve_host_request_ = host_resolver_->CreateRequest(
      key_.destination(), key_.session_key().network_isolation_key(), net_log_,
      parameters);
  // Unretained is safe because |this| owns the request, ensuring cancellation
  // on destruction.
  // This request will only query fresh host resolution.
  int fresh_rv = fresh_resolve_host_request_->Start(base::BindOnce(
      &QuicStreamFactory::Job::OnResolveHostComplete, base::Unretained(this)));
  if (fresh_rv != ERR_IO_PENDING) {
    // Fresh request returned immediate results.
    LogFreshDnsResolveTime(dns_resolution_start_time_);
    LogStaleHostRacing(false);
    resolve_host_request_ = std::move(fresh_resolve_host_request_);
    return fresh_rv;
  }

  // Check to make sure stale host request does produce valid results.
  if (!resolve_host_request_->GetAddressResults()) {
    LogStaleHostRacing(false);
    resolve_host_request_ = std::move(fresh_resolve_host_request_);
    return fresh_rv;
  }

  // No fresh host resolution is available at this time, but there is available
  // stale result. End time for stale host resolution is recorded and connection
  // from stale host will be tried.
  dns_resolution_end_time_ = base::TimeTicks().Now();
  io_state_ = STATE_CONNECT;
  LogStaleHostRacing(true);
  return OK;
}

int QuicStreamFactory::Job::DoResolveHostComplete(int rv) {
  host_resolution_finished_ = true;
  dns_resolution_end_time_ = base::TimeTicks::Now();
  if (rv != OK)
    return rv;

  DCHECK(!fresh_resolve_host_request_);
  DCHECK(!factory_->HasActiveSession(key_.session_key()));

  // Inform the factory of this resolution, which will set up
  // a session alias, if possible.
  if (factory_->HasMatchingIpSession(
          key_, resolve_host_request_->GetAddressResults().value())) {
    LogConnectionIpPooling(true);
    return OK;
  }

  io_state_ = STATE_CONNECT;
  return OK;
}

int QuicStreamFactory::Job::DoConnect() {
  quic_connection_start_time_ = base::TimeTicks::Now();
  DCHECK(dns_resolution_end_time_ != base::TimeTicks());
  io_state_ = STATE_CONNECT_COMPLETE;
  bool require_confirmation = was_alternative_service_recently_broken_;
  net_log_.AddEntryWithBoolParams(
      NetLogEventType::QUIC_STREAM_FACTORY_JOB_CONNECT, NetLogEventPhase::BEGIN,
      "require_confirmation", require_confirmation);

  DCHECK_NE(quic_version_, quic::ParsedQuicVersion::Unsupported());
  int rv = factory_->CreateSession(
      key_, quic_version_, cert_verify_flags_, require_confirmation,
      resolve_host_request_->GetAddressResults().value(),
      dns_resolution_start_time_, dns_resolution_end_time_, net_log_, &session_,
      &network_);
  DVLOG(1) << "Created session on network: " << network_;

  if (rv != OK) {
    DCHECK(rv != ERR_IO_PENDING);
    DCHECK(!session_);
    return rv;
  }

  if (!session_->connection()->connected())
    return ERR_CONNECTION_CLOSED;

  session_->StartReading();
  if (!session_->connection()->connected())
    return ERR_QUIC_PROTOCOL_ERROR;

  rv = session_->CryptoConnect(
      base::BindOnce(&QuicStreamFactory::Job::OnConnectComplete, GetWeakPtr()));

  if (!session_->connection()->connected() &&
      session_->error() == quic::QUIC_PROOF_INVALID) {
    return ERR_QUIC_HANDSHAKE_FAILED;
  }

  return rv;
}

int QuicStreamFactory::Job::DoConnectComplete(int rv) {
  if (!fresh_resolve_host_request_) {
    LogValidConnectionTime(quic_connection_start_time_);
    io_state_ = STATE_CONFIRM_CONNECTION;
    return rv;
  }

  if (rv == OK) {
    io_state_ = STATE_HOST_VALIDATION;
    return ERR_IO_PENDING;
  }

  // Connection from stale host resolution failed, has been closed and will
  // be deleted soon. Update Job status accordingly to wait for fresh host
  // resolution.
  LogStaleConnectionTime(quic_connection_start_time_);
  resolve_host_request_ = std::move(fresh_resolve_host_request_);
  session_ = nullptr;
  io_state_ = STATE_RESOLVE_HOST_COMPLETE;
  return ERR_IO_PENDING;
}

// This state is reached iff both host resolution and connection from stale dns
// have finished successfully.
int QuicStreamFactory::Job::DoValidateHost() {
  if (DoesPeerAddressMatchWithFreshAddressList()) {
    LogValidConnectionTime(quic_connection_start_time_);
    LogRacingStatus(ConnectionStateAfterDNS::kCryptoFinishedDnsMatch);
    LogStaleAndFreshHostMatched(true);
    fresh_resolve_host_request_ = nullptr;
    host_resolution_finished_ = true;
    io_state_ = STATE_CONFIRM_CONNECTION;
    return OK;
  }

  LogStaleConnectionTime(quic_connection_start_time_);
  LogRacingStatus(ConnectionStateAfterDNS::kCryptoFinishedDnsNoMatch);
  LogStaleAndFreshHostMatched(false);
  resolve_host_request_ = std::move(fresh_resolve_host_request_);
  CloseStaleHostConnection();
  io_state_ = STATE_RESOLVE_HOST_COMPLETE;
  return OK;
}

int QuicStreamFactory::Job::DoConfirmConnection(int rv) {
  UMA_HISTOGRAM_TIMES("Net.QuicSession.TimeFromResolveHostToConfirmConnection",
                      base::TimeTicks::Now() - dns_resolution_start_time_);
  net_log_.EndEvent(NetLogEventType::QUIC_STREAM_FACTORY_JOB_CONNECT);

  if (was_alternative_service_recently_broken_)
    UMA_HISTOGRAM_BOOLEAN("Net.QuicSession.ConnectAfterBroken", rv == OK);

  if (retry_on_alternate_network_before_handshake_ && session_ &&
      !session_->OneRttKeysAvailable() &&
      network_ == factory_->default_network()) {
    if (session_->error() == quic::QUIC_NETWORK_IDLE_TIMEOUT ||
        session_->error() == quic::QUIC_HANDSHAKE_TIMEOUT ||
        session_->error() == quic::QUIC_PACKET_WRITE_ERROR) {
      // Retry the connection on an alternate network if crypto handshake failed
      // with network idle time out or handshake time out.
      DCHECK(network_ != NetworkChangeNotifier::kInvalidNetworkHandle);
      network_ = factory_->FindAlternateNetwork(network_);
      connection_retried_ =
          network_ != NetworkChangeNotifier::kInvalidNetworkHandle;
      UMA_HISTOGRAM_BOOLEAN(
          "Net.QuicStreamFactory.AttemptMigrationBeforeHandshake",
          connection_retried_);
      UMA_HISTOGRAM_ENUMERATION(
          "Net.QuicStreamFactory.AttemptMigrationBeforeHandshake."
          "FailedConnectionType",
          NetworkChangeNotifier::GetNetworkConnectionType(
              factory_->default_network()),
          NetworkChangeNotifier::ConnectionType::CONNECTION_LAST + 1);
      if (connection_retried_) {
        UMA_HISTOGRAM_ENUMERATION(
            "Net.QuicStreamFactory.MigrationBeforeHandshake.NewConnectionType",
            NetworkChangeNotifier::GetNetworkConnectionType(network_),
            NetworkChangeNotifier::ConnectionType::CONNECTION_LAST + 1);
        net_log_.AddEvent(
            NetLogEventType::
                QUIC_STREAM_FACTORY_JOB_RETRY_ON_ALTERNATE_NETWORK);
        // Notify requests that connection on the default network failed.
        for (auto* request : stream_requests_) {
          request->OnConnectionFailedOnDefaultNetwork();
        }
        DVLOG(1) << "Retry connection on alternate network: " << network_;
        session_ = nullptr;
        io_state_ = STATE_CONNECT;
        return OK;
      }
    }
  }

  if (connection_retried_) {
    UMA_HISTOGRAM_BOOLEAN("Net.QuicStreamFactory.MigrationBeforeHandshake2",
                          rv == OK);
    if (rv == OK) {
      UMA_HISTOGRAM_BOOLEAN(
          "Net.QuicStreamFactory.NetworkChangeDuringMigrationBeforeHandshake",
          network_ == factory_->default_network());
    } else {
      base::UmaHistogramSparse(
          "Net.QuicStreamFactory.MigrationBeforeHandshakeFailedReason", -rv);
    }
  } else if (network_ != NetworkChangeNotifier::kInvalidNetworkHandle &&
             network_ != factory_->default_network()) {
    UMA_HISTOGRAM_BOOLEAN("Net.QuicStreamFactory.ConnectionOnNonDefaultNetwork",
                          rv == OK);
  }

  if (rv != OK)
    return rv;

  DCHECK(!factory_->HasActiveSession(key_.session_key()));
  // There may well now be an active session for this IP.  If so, use the
  // existing session instead.
  AddressList address(ToIPEndPoint(session_->connection()->peer_address()));
  if (factory_->HasMatchingIpSession(key_, address)) {
    LogConnectionIpPooling(true);
    session_->connection()->CloseConnection(
        quic::QUIC_CONNECTION_IP_POOLED,
        "An active session exists for the given IP.",
        quic::ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
    session_ = nullptr;
    return OK;
  }
  LogConnectionIpPooling(false);

  factory_->ActivateSession(key_, session_);

  return OK;
}

QuicStreamRequest::QuicStreamRequest(QuicStreamFactory* factory)
    : factory_(factory), expect_on_host_resolution_(false) {}

QuicStreamRequest::~QuicStreamRequest() {
  if (factory_ && !callback_.is_null())
    factory_->CancelRequest(this);
}

int QuicStreamRequest::Request(
    const HostPortPair& destination,
    quic::ParsedQuicVersion quic_version,
    PrivacyMode privacy_mode,
    RequestPriority priority,
    const SocketTag& socket_tag,
    const NetworkIsolationKey& network_isolation_key,
    bool disable_secure_dns,
    int cert_verify_flags,
    const GURL& url,
    const NetLogWithSource& net_log,
    NetErrorDetails* net_error_details,
    CompletionOnceCallback failed_on_default_network_callback,
    CompletionOnceCallback callback) {
  DCHECK_NE(quic_version, quic::ParsedQuicVersion::Unsupported());
  DCHECK(net_error_details);
  DCHECK(callback_.is_null());
  DCHECK(host_resolution_callback_.is_null());
  DCHECK(factory_);

  net_error_details_ = net_error_details;
  failed_on_default_network_callback_ =
      std::move(failed_on_default_network_callback);
  session_key_ =
      QuicSessionKey(HostPortPair::FromURL(url), privacy_mode, socket_tag,
                     network_isolation_key, disable_secure_dns);

  int rv = factory_->Create(session_key_, destination, quic_version, priority,
                            cert_verify_flags, url, net_log, this);
  if (rv == ERR_IO_PENDING) {
    net_log_ = net_log;
    callback_ = std::move(callback);
  } else {
    DCHECK(!expect_on_host_resolution_);
    factory_ = nullptr;
  }

  if (rv == OK)
    DCHECK(session_);
  return rv;
}

bool QuicStreamRequest::WaitForHostResolution(CompletionOnceCallback callback) {
  DCHECK(host_resolution_callback_.is_null());
  if (expect_on_host_resolution_) {
    host_resolution_callback_ = std::move(callback);
  }
  return expect_on_host_resolution_;
}

void QuicStreamRequest::ExpectOnHostResolution() {
  expect_on_host_resolution_ = true;
}

void QuicStreamRequest::OnHostResolutionComplete(int rv) {
  DCHECK(expect_on_host_resolution_);
  expect_on_host_resolution_ = false;
  if (!host_resolution_callback_.is_null()) {
    std::move(host_resolution_callback_).Run(rv);
  }
}

void QuicStreamRequest::OnRequestComplete(int rv) {
  factory_ = nullptr;
  std::move(callback_).Run(rv);
}

void QuicStreamRequest::OnConnectionFailedOnDefaultNetwork() {
  if (!failed_on_default_network_callback_.is_null())
    std::move(failed_on_default_network_callback_).Run(OK);
}

base::TimeDelta QuicStreamRequest::GetTimeDelayForWaitingJob() const {
  if (!factory_)
    return base::TimeDelta();
  return factory_->GetTimeDelayForWaitingJob(session_key_);
}

void QuicStreamRequest::SetPriority(RequestPriority priority) {
  if (factory_)
    factory_->SetRequestPriority(this, priority);
}

std::unique_ptr<QuicChromiumClientSession::Handle>
QuicStreamRequest::ReleaseSessionHandle() {
  if (!session_ || !session_->IsConnected())
    return nullptr;

  return std::move(session_);
}

void QuicStreamRequest::SetSession(
    std::unique_ptr<QuicChromiumClientSession::Handle> session) {
  session_ = move(session);
}

QuicStreamFactory::QuicSessionAliasKey::QuicSessionAliasKey(
    const HostPortPair& destination,
    const QuicSessionKey& session_key)
    : destination_(destination), session_key_(session_key) {}

bool QuicStreamFactory::QuicSessionAliasKey::operator<(
    const QuicSessionAliasKey& other) const {
  return std::tie(destination_, session_key_) <
         std::tie(other.destination_, other.session_key_);
}

bool QuicStreamFactory::QuicSessionAliasKey::operator==(
    const QuicSessionAliasKey& other) const {
  return destination_.Equals(other.destination_) &&
         session_key_ == other.session_key_;
}

size_t QuicStreamFactory::QuicSessionAliasKey::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(destination_) +
         base::trace_event::EstimateMemoryUsage(session_key_.server_id());
}

QuicStreamFactory::QuicStreamFactory(
    NetLog* net_log,
    HostResolver* host_resolver,
    SSLConfigService* ssl_config_service,
    ClientSocketFactory* client_socket_factory,
    HttpServerProperties* http_server_properties,
    CertVerifier* cert_verifier,
    CTPolicyEnforcer* ct_policy_enforcer,
    TransportSecurityState* transport_security_state,
    CTVerifier* cert_transparency_verifier,
    SCTAuditingDelegate* sct_auditing_delegate,
    SocketPerformanceWatcherFactory* socket_performance_watcher_factory,
    QuicCryptoClientStreamFactory* quic_crypto_client_stream_factory,
    QuicContext* quic_context)
    : is_quic_known_to_work_on_current_network_(false),
      net_log_(net_log),
      host_resolver_(host_resolver),
      client_socket_factory_(client_socket_factory),
      http_server_properties_(http_server_properties),
      push_delegate_(nullptr),
      cert_verifier_(cert_verifier),
      ct_policy_enforcer_(ct_policy_enforcer),
      transport_security_state_(transport_security_state),
      cert_transparency_verifier_(cert_transparency_verifier),
      sct_auditing_delegate_(sct_auditing_delegate),
      quic_crypto_client_stream_factory_(quic_crypto_client_stream_factory),
      random_generator_(quic_context->random_generator()),
      clock_(quic_context->clock()),
      // TODO(vasilvv): figure out how to avoid having multiple copies of
      // QuicParams.
      params_(*quic_context->params()),
      clock_skew_detector_(base::TimeTicks::Now(), base::Time::Now()),
      socket_performance_watcher_factory_(socket_performance_watcher_factory),
      recent_crypto_config_map_(kMaxRecentCryptoConfigs),
      config_(InitializeQuicConfig(*quic_context->params())),
      ping_timeout_(quic::QuicTime::Delta::FromSeconds(quic::kPingTimeoutSecs)),
      reduced_ping_timeout_(quic::QuicTime::Delta::FromMicroseconds(
          quic_context->params()->reduced_ping_timeout.InMicroseconds())),
      retransmittable_on_wire_timeout_(quic::QuicTime::Delta::FromMicroseconds(
          quic_context->params()
              ->retransmittable_on_wire_timeout.InMicroseconds())),
      yield_after_packets_(kQuicYieldAfterPacketsRead),
      yield_after_duration_(quic::QuicTime::Delta::FromMilliseconds(
          kQuicYieldAfterDurationMilliseconds)),
      default_network_(NetworkChangeNotifier::kInvalidNetworkHandle),
      need_to_check_persisted_supports_quic_(true),
      prefer_aes_gcm_recorded_(false),
      num_push_streams_created_(0),
      connectivity_monitor_(default_network_),
      tick_clock_(nullptr),
      task_runner_(nullptr),
      ssl_config_service_(ssl_config_service),
      use_network_isolation_key_for_crypto_configs_(
          base::FeatureList::IsEnabled(
              features::kPartitionHttpServerPropertiesByNetworkIsolationKey)) {
  DCHECK(transport_security_state_);
  DCHECK(http_server_properties_);
  if (params_.disable_tls_zero_rtt)
    SetQuicRestartFlag(quic_enable_zero_rtt_for_tls_v2, false);
  InitializeMigrationOptions();
}

QuicStreamFactory::~QuicStreamFactory() {
  UMA_HISTOGRAM_COUNTS_1000("Net.NumQuicSessionsAtShutdown",
                            all_sessions_.size());
  CloseAllSessions(ERR_ABORTED, quic::QUIC_CONNECTION_CANCELLED);
  while (!all_sessions_.empty()) {
    delete all_sessions_.begin()->first;
    all_sessions_.erase(all_sessions_.begin());
  }
  active_jobs_.clear();

  // This should have been moved to the recent map when all consumers of
  // QuicCryptoClientConfigs were deleted, in the above lines.
  DCHECK(active_crypto_config_map_.empty());

  if (params_.close_sessions_on_ip_change ||
      params_.goaway_sessions_on_ip_change) {
    NetworkChangeNotifier::RemoveIPAddressObserver(this);
  }
  if (NetworkChangeNotifier::AreNetworkHandlesSupported()) {
    NetworkChangeNotifier::RemoveNetworkObserver(this);
  }
}

bool QuicStreamFactory::CanUseExistingSession(const QuicSessionKey& session_key,
                                              const HostPortPair& destination) {
  // TODO(zhongyi): delete active_sessions_.empty() checks once the
  // android crash issue(crbug.com/498823) is resolved.
  if (active_sessions_.empty())
    return false;

  if (base::Contains(active_sessions_, session_key))
    return true;

  for (const auto& key_value : active_sessions_) {
    QuicChromiumClientSession* session = key_value.second;
    if (destination.Equals(all_sessions_[session].destination()) &&
        session->CanPool(session_key.host(), session_key)) {
      return true;
    }
  }

  return false;
}

int QuicStreamFactory::Create(const QuicSessionKey& session_key,
                              const HostPortPair& destination,
                              quic::ParsedQuicVersion quic_version,
                              RequestPriority priority,
                              int cert_verify_flags,
                              const GURL& url,
                              const NetLogWithSource& net_log,
                              QuicStreamRequest* request) {
  if (clock_skew_detector_.ClockSkewDetected(base::TimeTicks::Now(),
                                             base::Time::Now())) {
    MarkAllActiveSessionsGoingAway(kClockSkewDetected);
  }
  DCHECK(HostPortPair(session_key.server_id().host(),
                      session_key.server_id().port())
             .Equals(HostPortPair::FromURL(url)));

  // Search sessions for a matching promised stream.
  for (auto session : active_sessions_) {
    quic::QuicClientPromisedInfo* promised =
        session.second->GetPromised(url, session_key);
    if (!promised)
      continue;
    DCHECK_EQ(promised->session(), session.second);
    request->SetSession(session.second->CreateHandle(destination));
    ++num_push_streams_created_;
    return OK;
  }

  // Use active session for |session_key| if such exists.
  // TODO(rtenneti): crbug.com/498823 - delete active_sessions_.empty() checks.
  if (!active_sessions_.empty()) {
    auto it = active_sessions_.find(session_key);
    if (it != active_sessions_.end()) {
      QuicChromiumClientSession* session = it->second;
      request->SetSession(session->CreateHandle(destination));
      return OK;
    }
  }

  // Associate with active job to |session_key| if such exists.
  auto it = active_jobs_.find(session_key);
  if (it != active_jobs_.end()) {
    const NetLogWithSource& job_net_log = it->second->net_log();
    job_net_log.AddEventReferencingSource(
        NetLogEventType::QUIC_STREAM_FACTORY_JOB_BOUND_TO_HTTP_STREAM_JOB,
        net_log.source());
    net_log.AddEventReferencingSource(
        NetLogEventType::HTTP_STREAM_JOB_BOUND_TO_QUIC_STREAM_FACTORY_JOB,
        job_net_log.source());
    it->second->AddRequest(request);
    return ERR_IO_PENDING;
  }

  // Pool to active session to |destination| if possible.
  if (!active_sessions_.empty()) {
    for (const auto& key_value : active_sessions_) {
      QuicChromiumClientSession* session = key_value.second;
      if (destination.Equals(all_sessions_[session].destination()) &&
          session->CanPool(session_key.server_id().host(), session_key)) {
        request->SetSession(session->CreateHandle(destination));
        return OK;
      }
    }
  }

  // TODO(rtenneti): |task_runner_| is used by the Job. Initialize task_runner_
  // in the constructor after WebRequestActionWithThreadsTest.* tests are fixed.
  if (!task_runner_)
    task_runner_ = base::ThreadTaskRunnerHandle::Get().get();

  if (!tick_clock_)
    tick_clock_ = base::DefaultTickClock::GetInstance();

  QuicSessionAliasKey key(destination, session_key);
  std::unique_ptr<Job> job = std::make_unique<Job>(
      this, quic_version, host_resolver_, key,
      CreateCryptoConfigHandle(session_key.network_isolation_key()),
      WasQuicRecentlyBroken(session_key),
      params_.retry_on_alternate_network_before_handshake,
      params_.race_stale_dns_on_connection, priority, cert_verify_flags,
      net_log);
  int rv = job->Run(base::BindOnce(&QuicStreamFactory::OnJobComplete,
                                   base::Unretained(this), job.get()));
  if (rv == ERR_IO_PENDING) {
    job->AddRequest(request);
    active_jobs_[session_key] = std::move(job);
    return rv;
  }
  if (rv == OK) {
    // TODO(rtenneti): crbug.com/498823 - revert active_sessions_.empty()
    // related changes.
    if (active_sessions_.empty())
      return ERR_QUIC_PROTOCOL_ERROR;
    auto it = active_sessions_.find(session_key);
    DCHECK(it != active_sessions_.end());
    if (it == active_sessions_.end())
      return ERR_QUIC_PROTOCOL_ERROR;
    QuicChromiumClientSession* session = it->second;
    request->SetSession(session->CreateHandle(destination));
  }
  return rv;
}

void QuicStreamFactory::OnSessionGoingAway(QuicChromiumClientSession* session) {
  const AliasSet& aliases = session_aliases_[session];
  for (auto it = aliases.begin(); it != aliases.end(); ++it) {
    const QuicSessionKey& session_key = it->session_key();
    DCHECK(active_sessions_.count(session_key));
    DCHECK_EQ(session, active_sessions_[session_key]);
    // Track sessions which have recently gone away so that we can disable
    // port suggestions.
    if (session->goaway_received())
      gone_away_aliases_.insert(*it);

    active_sessions_.erase(session_key);
    ProcessGoingAwaySession(session, session_key.server_id(), true);
  }
  ProcessGoingAwaySession(session, all_sessions_[session].server_id(), false);
  if (!aliases.empty()) {
    DCHECK(base::Contains(session_peer_ip_, session));
    const IPEndPoint peer_address = session_peer_ip_[session];
    ip_aliases_[peer_address].erase(session);
    if (ip_aliases_[peer_address].empty())
      ip_aliases_.erase(peer_address);
    session_peer_ip_.erase(session);
  }
  session_aliases_.erase(session);
}

void QuicStreamFactory::OnSessionClosed(QuicChromiumClientSession* session) {
  DCHECK_EQ(0u, session->GetNumActiveStreams());
  OnSessionGoingAway(session);
  for (const auto& iter : active_jobs_) {
    if (iter.first == session->quic_session_key()) {
      iter.second->OnSessionClosed(session);
    }
  }
  delete session;
  all_sessions_.erase(session);
}

void QuicStreamFactory::OnBlackholeAfterHandshakeConfirmed(
    QuicChromiumClientSession* session) {
  // Reduce PING timeout when connection blackholes after the handshake.
  if (ping_timeout_ > reduced_ping_timeout_)
    ping_timeout_ = reduced_ping_timeout_;
}

void QuicStreamFactory::CancelRequest(QuicStreamRequest* request) {
  auto job_iter = active_jobs_.find(request->session_key());
  CHECK(job_iter != active_jobs_.end());
  job_iter->second->RemoveRequest(request);
}

void QuicStreamFactory::SetRequestPriority(QuicStreamRequest* request,
                                           RequestPriority priority) {
  auto job_iter = active_jobs_.find(request->session_key());
  if (job_iter == active_jobs_.end())
    return;
  job_iter->second->SetPriority(priority);
}

void QuicStreamFactory::CloseAllSessions(int error,
                                         quic::QuicErrorCode quic_error) {
  base::UmaHistogramSparse("Net.QuicSession.CloseAllSessionsError", -error);
  while (!active_sessions_.empty()) {
    size_t initial_size = active_sessions_.size();
    active_sessions_.begin()->second->CloseSessionOnError(
        error, quic_error,
        quic::ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
    DCHECK_NE(initial_size, active_sessions_.size());
  }
  while (!all_sessions_.empty()) {
    size_t initial_size = all_sessions_.size();
    all_sessions_.begin()->first->CloseSessionOnError(
        error, quic_error,
        quic::ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
    DCHECK_NE(initial_size, all_sessions_.size());
  }
  DCHECK(all_sessions_.empty());
}

std::unique_ptr<base::Value> QuicStreamFactory::QuicStreamFactoryInfoToValue()
    const {
  std::unique_ptr<base::ListValue> list(new base::ListValue());

  for (auto it = active_sessions_.begin(); it != active_sessions_.end(); ++it) {
    const quic::QuicServerId& server_id = it->first.server_id();
    QuicChromiumClientSession* session = it->second;
    const AliasSet& aliases = session_aliases_.find(session)->second;
    // Only add a session to the list once.
    if (server_id == aliases.begin()->server_id()) {
      std::set<HostPortPair> hosts;
      for (auto alias_it = aliases.begin(); alias_it != aliases.end();
           ++alias_it) {
        hosts.insert(HostPortPair(alias_it->server_id().host(),
                                  alias_it->server_id().port()));
      }
      list->Append(session->GetInfoAsValue(hosts));
    }
  }
  return std::move(list);
}

void QuicStreamFactory::ClearCachedStatesInCryptoConfig(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter) {
  ServerIdOriginFilter filter(origin_filter);
  for (const auto& crypto_config : active_crypto_config_map_) {
    crypto_config.second->config()->ClearCachedStates(filter);
  }

  for (const auto& crypto_config : recent_crypto_config_map_) {
    crypto_config.second->config()->ClearCachedStates(filter);
  }
}

int QuicStreamFactory::ConfigureSocket(DatagramClientSocket* socket,
                                       IPEndPoint addr,
                                       NetworkHandle network,
                                       const SocketTag& socket_tag) {
  socket->UseNonBlockingIO();

  int rv;
  if (params_.migrate_sessions_on_network_change_v2) {
    // If caller leaves network unspecified, use current default network.
    if (network == NetworkChangeNotifier::kInvalidNetworkHandle) {
      rv = socket->ConnectUsingDefaultNetwork(addr);
    } else {
      rv = socket->ConnectUsingNetwork(network, addr);
    }
  } else {
    rv = socket->Connect(addr);
  }
  if (rv != OK) {
    HistogramCreateSessionFailure(CREATION_ERROR_CONNECTING_SOCKET);
    return rv;
  }

  socket->ApplySocketTag(socket_tag);

  rv = socket->SetReceiveBufferSize(kQuicSocketReceiveBufferSize);
  if (rv != OK) {
    HistogramCreateSessionFailure(CREATION_ERROR_SETTING_RECEIVE_BUFFER);
    return rv;
  }

  rv = socket->SetDoNotFragment();
  // SetDoNotFragment is not implemented on all platforms, so ignore errors.
  if (rv != OK && rv != ERR_NOT_IMPLEMENTED) {
    HistogramCreateSessionFailure(CREATION_ERROR_SETTING_DO_NOT_FRAGMENT);
    return rv;
  }

  // Set a buffer large enough to contain the initial CWND's worth of packet
  // to work around the problem with CHLO packets being sent out with the
  // wrong encryption level, when the send buffer is full.
  rv = socket->SetSendBufferSize(quic::kMaxOutgoingPacketSize * 20);
  if (rv != OK) {
    HistogramCreateSessionFailure(CREATION_ERROR_SETTING_SEND_BUFFER);
    return rv;
  }

  socket->GetLocalAddress(&local_address_);
  if (need_to_check_persisted_supports_quic_) {
    need_to_check_persisted_supports_quic_ = false;
    if (http_server_properties_->WasLastLocalAddressWhenQuicWorked(
            local_address_.address())) {
      is_quic_known_to_work_on_current_network_ = true;
      // Clear the persisted IP address, in case the network no longer supports
      // QUIC so the next restart will require confirmation. It will be
      // re-persisted when the first job completes successfully.
      http_server_properties_->ClearLastLocalAddressWhenQuicWorked();
    }
  }

  return OK;
}

NetworkHandle QuicStreamFactory::FindAlternateNetwork(
    NetworkHandle old_network) {
  // Find a new network that sessions bound to |old_network| can be migrated to.
  NetworkChangeNotifier::NetworkList network_list;
  NetworkChangeNotifier::GetConnectedNetworks(&network_list);
  for (NetworkHandle new_network : network_list) {
    if (new_network != old_network)
      return new_network;
  }
  return NetworkChangeNotifier::kInvalidNetworkHandle;
}

std::unique_ptr<DatagramClientSocket> QuicStreamFactory::CreateSocket(
    NetLog* net_log,
    const NetLogSource& source) {
  auto socket = client_socket_factory_->CreateDatagramClientSocket(
      DatagramSocket::DEFAULT_BIND, net_log, source);
  if (params_.enable_socket_recv_optimization)
    socket->EnableRecvOptimization();
  return socket;
}

void QuicStreamFactory::OnIPAddressChanged() {
  CollectDataOnPlatformNotification(
      NETWORK_IP_ADDRESS_CHANGED, NetworkChangeNotifier::kInvalidNetworkHandle);
  // Do nothing if connection migration is turned on.
  if (params_.migrate_sessions_on_network_change_v2)
    return;

  connectivity_monitor_.OnIPAddressChanged();

  set_is_quic_known_to_work_on_current_network(false);
  if (params_.close_sessions_on_ip_change) {
    CloseAllSessions(ERR_NETWORK_CHANGED, quic::QUIC_IP_ADDRESS_CHANGED);
  } else {
    DCHECK(params_.goaway_sessions_on_ip_change);
    MarkAllActiveSessionsGoingAway(kIPAddressChanged);
  }
}

void QuicStreamFactory::OnNetworkConnected(NetworkHandle network) {
  CollectDataOnPlatformNotification(NETWORK_CONNECTED, network);
  if (params_.migrate_sessions_on_network_change_v2) {
    NetLogWithSource net_log = NetLogWithSource::Make(
        net_log_, NetLogSourceType::QUIC_CONNECTION_MIGRATION);
    net_log.AddEventWithStringParams(
        NetLogEventType::QUIC_CONNECTION_MIGRATION_PLATFORM_NOTIFICATION,
        "signal", "OnNetworkConnected");
  }
  // Broadcast network connected to all sessions.
  // If migration is not turned on, session will not migrate but collect data.
  auto it = all_sessions_.begin();
  // Sessions may be deleted while iterating through the map.
  while (it != all_sessions_.end()) {
    QuicChromiumClientSession* session = it->first;
    ++it;
    session->OnNetworkConnected(network);
  }
}

void QuicStreamFactory::OnNetworkDisconnected(NetworkHandle network) {
  CollectDataOnPlatformNotification(NETWORK_DISCONNECTED, network);
  if (params_.migrate_sessions_on_network_change_v2) {
    NetLogWithSource net_log = NetLogWithSource::Make(
        net_log_, NetLogSourceType::QUIC_CONNECTION_MIGRATION);
    net_log.AddEventWithStringParams(
        NetLogEventType::QUIC_CONNECTION_MIGRATION_PLATFORM_NOTIFICATION,
        "signal", "OnNetworkDisconnected");
  }
  // Broadcast network disconnected to all sessions.
  // If migration is not turned on, session will not migrate but collect data.
  auto it = all_sessions_.begin();
  // Sessions may be deleted while iterating through the map.
  while (it != all_sessions_.end()) {
    QuicChromiumClientSession* session = it->first;
    ++it;
    session->OnNetworkDisconnectedV2(/*disconnected_network*/ network);
  }
}

// This method is expected to only be called when migrating from Cellular to
// WiFi on Android, and should always be preceded by OnNetworkMadeDefault().
void QuicStreamFactory::OnNetworkSoonToDisconnect(NetworkHandle network) {
  CollectDataOnPlatformNotification(NETWORK_SOON_TO_DISCONNECT, network);
}

void QuicStreamFactory::OnNetworkMadeDefault(NetworkHandle network) {
  CollectDataOnPlatformNotification(NETWORK_MADE_DEFAULT, network);
  connectivity_monitor_.OnDefaultNetworkUpdated(network);

  // Clear alternative services that were marked as broken until default network
  // changes.
  if (params_.retry_on_alternate_network_before_handshake &&
      default_network_ != NetworkChangeNotifier::kInvalidNetworkHandle &&
      network != default_network_) {
    http_server_properties_->OnDefaultNetworkChanged();
  }

  DCHECK_NE(NetworkChangeNotifier::kInvalidNetworkHandle, network);
  default_network_ = network;

  if (params_.migrate_sessions_on_network_change_v2) {
    NetLogWithSource net_log = NetLogWithSource::Make(
        net_log_, NetLogSourceType::QUIC_CONNECTION_MIGRATION);
    net_log.AddEventWithStringParams(
        NetLogEventType::QUIC_CONNECTION_MIGRATION_PLATFORM_NOTIFICATION,
        "signal", "OnNetworkMadeDefault");
  }

  auto it = all_sessions_.begin();
  // Sessions may be deleted while iterating through the map.
  while (it != all_sessions_.end()) {
    QuicChromiumClientSession* session = it->first;
    ++it;
    session->OnNetworkMadeDefault(network);
  }
  if (params_.migrate_sessions_on_network_change_v2)
    set_is_quic_known_to_work_on_current_network(false);
}

void QuicStreamFactory::OnCertDBChanged() {
  // We should flush the sessions if we removed trust from a
  // cert, because a previously trusted server may have become
  // untrusted.
  //
  // We should not flush the sessions if we added trust to a cert.
  //
  // Since the OnCertDBChanged method doesn't tell us what
  // kind of change it is, we have to flush the socket
  // pools to be safe.
  MarkAllActiveSessionsGoingAway(kCertDBChanged);
}

void QuicStreamFactory::set_is_quic_known_to_work_on_current_network(
    bool is_quic_known_to_work_on_current_network) {
  is_quic_known_to_work_on_current_network_ =
      is_quic_known_to_work_on_current_network;
  if (!(local_address_ == IPEndPoint())) {
    if (is_quic_known_to_work_on_current_network_) {
      http_server_properties_->SetLastLocalAddressWhenQuicWorked(
          local_address_.address());
    } else {
      http_server_properties_->ClearLastLocalAddressWhenQuicWorked();
    }
  }
}

base::TimeDelta QuicStreamFactory::GetTimeDelayForWaitingJob(
    const QuicSessionKey& session_key) {
  // If |is_quic_known_to_work_on_current_network_| is false, then one of the
  // following is true:
  // 1) This is startup and QuicStreamFactory::CreateSession() and
  // ConfigureSocket() have yet to be called, and it is not yet known
  // if the current network is the last one where QUIC worked.
  // 2) Startup has been completed, and QUIC has not been used
  // successfully since startup, or on this network before.
  if (!is_quic_known_to_work_on_current_network_) {
    // If |need_to_check_persisted_supports_quic_| is false, this is case 1)
    // above. If HasLastLocalAddressWhenQuicWorked() is also true, then there's
    // a chance the current network is the last one on which QUIC worked. So
    // only delay the request if there's no chance that is the case.
    if (!need_to_check_persisted_supports_quic_ ||
        !http_server_properties_->HasLastLocalAddressWhenQuicWorked()) {
      return base::TimeDelta();
    }
  }

  // QUIC was recently broken. Do not delay the main job.
  if (WasQuicRecentlyBroken(session_key)) {
    return base::TimeDelta();
  }

  int64_t srtt =
      1.5 * GetServerNetworkStatsSmoothedRttInMicroseconds(
                session_key.server_id(), session_key.network_isolation_key());
  // Picked 300ms based on mean time from
  // Net.QuicSession.HostResolution.HandshakeConfirmedTime histogram.
  const int kDefaultRTT = 300 * quic::kNumMicrosPerMilli;
  if (!srtt)
    srtt = kDefaultRTT;
  return base::TimeDelta::FromMicroseconds(srtt);
}

void QuicStreamFactory::DumpMemoryStats(
    base::trace_event::ProcessMemoryDump* pmd,
    const std::string& parent_absolute_name) const {
  if (all_sessions_.empty() && active_jobs_.empty())
    return;
  base::trace_event::MemoryAllocatorDump* factory_dump =
      pmd->CreateAllocatorDump(parent_absolute_name + "/quic_stream_factory");
  size_t memory_estimate =
      base::trace_event::EstimateMemoryUsage(all_sessions_) +
      base::trace_event::EstimateMemoryUsage(active_sessions_) +
      base::trace_event::EstimateMemoryUsage(session_aliases_) +
      base::trace_event::EstimateMemoryUsage(ip_aliases_) +
      base::trace_event::EstimateMemoryUsage(session_peer_ip_) +
      base::trace_event::EstimateMemoryUsage(gone_away_aliases_) +
      base::trace_event::EstimateMemoryUsage(active_jobs_);
  factory_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                          base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                          memory_estimate);
  factory_dump->AddScalar("all_sessions",
                          base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                          all_sessions_.size());
  factory_dump->AddScalar("active_jobs",
                          base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                          active_jobs_.size());
}

bool QuicStreamFactory::HasMatchingIpSession(const QuicSessionAliasKey& key,
                                             const AddressList& address_list) {
  const quic::QuicServerId& server_id(key.server_id());
  DCHECK(!HasActiveSession(key.session_key()));
  for (const IPEndPoint& address : address_list) {
    if (!base::Contains(ip_aliases_, address))
      continue;

    const SessionSet& sessions = ip_aliases_[address];
    for (QuicChromiumClientSession* session : sessions) {
      if (!session->CanPool(server_id.host(), key.session_key()))
        continue;
      active_sessions_[key.session_key()] = session;
      session_aliases_[session].insert(key);
      return true;
    }
  }
  return false;
}

void QuicStreamFactory::OnJobComplete(Job* job, int rv) {
  auto iter = active_jobs_.find(job->key().session_key());
  DCHECK(iter != active_jobs_.end());
  if (rv == OK) {
    set_is_quic_known_to_work_on_current_network(true);

    auto session_it = active_sessions_.find(job->key().session_key());
    CHECK(session_it != active_sessions_.end());
    QuicChromiumClientSession* session = session_it->second;
    for (auto* request : iter->second->stream_requests()) {
      // Do not notify |request| yet.
      request->SetSession(session->CreateHandle(job->key().destination()));
    }
  }

  for (auto* request : iter->second->stream_requests()) {
    // Even though we're invoking callbacks here, we don't need to worry
    // about |this| being deleted, because the factory is owned by the
    // profile which can not be deleted via callbacks.
    if (rv < 0) {
      job->PopulateNetErrorDetails(request->net_error_details());
    }
    request->OnRequestComplete(rv);
  }
  active_jobs_.erase(iter);
}

bool QuicStreamFactory::HasActiveSession(
    const QuicSessionKey& session_key) const {
  // TODO(rtenneti): crbug.com/498823 - delete active_sessions_.empty() check.
  if (active_sessions_.empty())
    return false;
  return base::Contains(active_sessions_, session_key);
}

bool QuicStreamFactory::HasActiveJob(const QuicSessionKey& session_key) const {
  return base::Contains(active_jobs_, session_key);
}

int QuicStreamFactory::CreateSession(
    const QuicSessionAliasKey& key,
    quic::ParsedQuicVersion quic_version,
    int cert_verify_flags,
    bool require_confirmation,
    const AddressList& address_list,
    base::TimeTicks dns_resolution_start_time,
    base::TimeTicks dns_resolution_end_time,
    const NetLogWithSource& net_log,
    QuicChromiumClientSession** session,
    NetworkChangeNotifier::NetworkHandle* network) {
  TRACE_EVENT0(NetTracingCategory(), "QuicStreamFactory::CreateSession");
  IPEndPoint addr = *address_list.begin();
  const quic::QuicServerId& server_id = key.server_id();
  std::unique_ptr<DatagramClientSocket> socket(
      CreateSocket(net_log.net_log(), net_log.source()));

  // Passing in kInvalidNetworkHandle binds socket to default network.
  int rv = ConfigureSocket(socket.get(), addr, *network,
                           key.session_key().socket_tag());
  if (rv != OK)
    return rv;

  if (params_.migrate_sessions_on_network_change_v2 &&
      *network == NetworkChangeNotifier::kInvalidNetworkHandle) {
    *network = socket->GetBoundNetwork();
    if (default_network_ == NetworkChangeNotifier::kInvalidNetworkHandle) {
      // QuicStreamFactory may miss the default network signal before its
      // creation, update |default_network_| when the first socket is bound
      // to the default network.
      default_network_ = *network;
      connectivity_monitor_.SetInitialDefaultNetwork(default_network_);
    } else {
      UMA_HISTOGRAM_BOOLEAN("Net.QuicStreamFactory.DefaultNetworkMatch",
                            default_network_ == *network);
    }
  }

  if (!helper_.get()) {
    helper_.reset(new QuicChromiumConnectionHelper(clock_, random_generator_));
  }

  if (!alarm_factory_.get()) {
    alarm_factory_.reset(new QuicChromiumAlarmFactory(
        base::ThreadTaskRunnerHandle::Get().get(), clock_));
  }

  quic::QuicConnectionId connection_id =
      quic::QuicUtils::CreateRandomConnectionId(random_generator_);
  std::unique_ptr<QuicServerInfo> server_info;
  if (params_.max_server_configs_stored_in_properties > 0) {
    server_info = std::make_unique<PropertiesBasedQuicServerInfo>(
        server_id, key.session_key().network_isolation_key(),
        http_server_properties_);
  }
  std::unique_ptr<CryptoClientConfigHandle> crypto_config_handle =
      CreateCryptoConfigHandle(key.session_key().network_isolation_key());
  InitializeCachedStateInCryptoConfig(*crypto_config_handle, server_id,
                                      server_info, &connection_id);

  QuicChromiumPacketWriter* writer =
      new QuicChromiumPacketWriter(socket.get(), task_runner_);
  quic::QuicConnection* connection = new quic::QuicConnection(
      connection_id, ToQuicSocketAddress(addr), helper_.get(),
      alarm_factory_.get(), writer, true /* owns_writer */,
      quic::Perspective::IS_CLIENT, {quic_version});
  connection->set_ping_timeout(ping_timeout_);
  connection->SetMaxPacketLength(params_.max_packet_length);

  quic::QuicConfig config = config_;
  ConfigureInitialRttEstimate(
      server_id, key.session_key().network_isolation_key(), &config);
  // QUIC versions that use the IETF invariant header all have NSTP
  // enabled by default, so we only need to add it for those that don't.
  if (!quic_version.HasIetfInvariantHeader() &&
      !config.HasClientSentConnectionOption(quic::kNSTP,
                                            quic::Perspective::IS_CLIENT)) {
    // Enable the no stop waiting frames connection option by default.
    quic::QuicTagVector connection_options = config.SendConnectionOptions();
    connection_options.push_back(quic::kNSTP);
    config.SetConnectionOptionsToSend(connection_options);
  }

  // Use the factory to create a new socket performance watcher, and pass the
  // ownership to QuicChromiumClientSession.
  std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher;
  if (socket_performance_watcher_factory_) {
    socket_performance_watcher =
        socket_performance_watcher_factory_->CreateSocketPerformanceWatcher(
            SocketPerformanceWatcherFactory::PROTOCOL_QUIC, address_list);
  }

  // Wait for handshake confirmation before allowing streams to be created if
  // either this session or the factory require confirmation.
  if (!is_quic_known_to_work_on_current_network_)
    require_confirmation = true;

  *session = new QuicChromiumClientSession(
      connection, std::move(socket), this, quic_crypto_client_stream_factory_,
      clock_, transport_security_state_, ssl_config_service_,
      std::move(server_info), key.session_key(), require_confirmation,
      params_.max_allowed_push_id, params_.migrate_sessions_early_v2,
      params_.migrate_sessions_on_network_change_v2, default_network_,
      retransmittable_on_wire_timeout_, params_.migrate_idle_sessions,
      params_.allow_port_migration, params_.idle_session_migration_period,
      params_.max_time_on_non_default_network,
      params_.max_migrations_to_non_default_network_on_write_error,
      params_.max_migrations_to_non_default_network_on_path_degrading,
      yield_after_packets_, yield_after_duration_,
      params_.go_away_on_path_degrading,
      params_.headers_include_h2_stream_dependency, cert_verify_flags, config,
      std::move(crypto_config_handle),
      network_connection_.connection_description(), dns_resolution_start_time,
      dns_resolution_end_time,
      std::make_unique<quic::QuicClientPushPromiseIndex>(), push_delegate_,
      tick_clock_, task_runner_, std::move(socket_performance_watcher),
      net_log.net_log());

  all_sessions_[*session] = key;  // owning pointer
  writer->set_delegate(*session);
  (*session)->AddConnectivityObserver(&connectivity_monitor_);

  (*session)->Initialize();
  bool closed_during_initialize = !base::Contains(all_sessions_, *session) ||
                                  !(*session)->connection()->connected();
  UMA_HISTOGRAM_BOOLEAN("Net.QuicSession.ClosedDuringInitializeSession",
                        closed_during_initialize);
  if (closed_during_initialize) {
    DLOG(DFATAL) << "Session closed during initialize";
    *session = nullptr;
    return ERR_CONNECTION_CLOSED;
  }
  return OK;
}

void QuicStreamFactory::ActivateSession(const QuicSessionAliasKey& key,
                                        QuicChromiumClientSession* session) {
  DCHECK(!HasActiveSession(key.session_key()));
  UMA_HISTOGRAM_COUNTS_1M("Net.QuicActiveSessions", active_sessions_.size());
  active_sessions_[key.session_key()] = session;
  session_aliases_[session].insert(key);
  const IPEndPoint peer_address =
      ToIPEndPoint(session->connection()->peer_address());
  DCHECK(!base::Contains(ip_aliases_[peer_address], session));
  ip_aliases_[peer_address].insert(session);
  DCHECK(!base::Contains(session_peer_ip_, session));
  session_peer_ip_[session] = peer_address;
}

void QuicStreamFactory::MarkAllActiveSessionsGoingAway(
    AllActiveSessionsGoingAwayReason reason) {
  while (!active_sessions_.empty()) {
    QuicChromiumClientSession* session = active_sessions_.begin()->second;
    // If IP address change is detected, disable session's connectivity
    // monitoring by remove the Delegate.
    if (reason == kIPAddressChanged)
      connectivity_monitor_.OnSessionGoingAwayOnIPAddressChange(session);
    OnSessionGoingAway(session);
  }
}

void QuicStreamFactory::ConfigureInitialRttEstimate(
    const quic::QuicServerId& server_id,
    const NetworkIsolationKey& network_isolation_key,
    quic::QuicConfig* config) {
  const base::TimeDelta* srtt =
      GetServerNetworkStatsSmoothedRtt(server_id, network_isolation_key);
  if (srtt != nullptr) {
    SetInitialRttEstimate(*srtt, INITIAL_RTT_CACHED, config);
    return;
  }

  NetworkChangeNotifier::ConnectionType type =
      network_connection_.connection_type();
  if (type == NetworkChangeNotifier::CONNECTION_2G) {
    SetInitialRttEstimate(base::TimeDelta::FromMilliseconds(1200),
                          INITIAL_RTT_CACHED, config);
    return;
  }

  if (type == NetworkChangeNotifier::CONNECTION_3G) {
    SetInitialRttEstimate(base::TimeDelta::FromMilliseconds(400),
                          INITIAL_RTT_CACHED, config);
    return;
  }

  if (params_.initial_rtt_for_handshake > base::TimeDelta()) {
    SetInitialRttEstimate(
        base::TimeDelta::FromMicroseconds(
            params_.initial_rtt_for_handshake.InMicroseconds()),
        INITIAL_RTT_DEFAULT, config);
    return;
  }

  SetInitialRttEstimate(base::TimeDelta(), INITIAL_RTT_DEFAULT, config);
}

int64_t QuicStreamFactory::GetServerNetworkStatsSmoothedRttInMicroseconds(
    const quic::QuicServerId& server_id,
    const NetworkIsolationKey& network_isolation_key) const {
  const base::TimeDelta* srtt =
      GetServerNetworkStatsSmoothedRtt(server_id, network_isolation_key);
  return srtt == nullptr ? 0 : srtt->InMicroseconds();
}

const base::TimeDelta* QuicStreamFactory::GetServerNetworkStatsSmoothedRtt(
    const quic::QuicServerId& server_id,
    const NetworkIsolationKey& network_isolation_key) const {
  url::SchemeHostPort server("https", server_id.host(), server_id.port());
  const ServerNetworkStats* stats =
      http_server_properties_->GetServerNetworkStats(server,
                                                     network_isolation_key);
  if (stats == nullptr)
    return nullptr;
  return &(stats->srtt);
}

bool QuicStreamFactory::WasQuicRecentlyBroken(
    const QuicSessionKey& session_key) const {
  const AlternativeService alternative_service(
      kProtoQUIC, HostPortPair(session_key.server_id().host(),
                               session_key.server_id().port()));
  return http_server_properties_->WasAlternativeServiceRecentlyBroken(
      alternative_service, session_key.network_isolation_key());
}

void QuicStreamFactory::InitializeMigrationOptions() {
  // The following list of options cannot be set immediately until
  // prerequisites are met. Cache the initial setting in local variables and
  // reset them in |params_|.
  bool migrate_sessions_on_network_change =
      params_.migrate_sessions_on_network_change_v2;
  bool migrate_sessions_early = params_.migrate_sessions_early_v2;
  bool retry_on_alternate_network_before_handshake =
      params_.retry_on_alternate_network_before_handshake;
  bool migrate_idle_sessions = params_.migrate_idle_sessions;
  bool allow_port_migration = params_.allow_port_migration;
  params_.migrate_sessions_on_network_change_v2 = false;
  params_.migrate_sessions_early_v2 = false;
  params_.allow_port_migration = false;
  params_.retry_on_alternate_network_before_handshake = false;
  params_.migrate_idle_sessions = false;

  DCHECK(!(migrate_sessions_early && params_.go_away_on_path_degrading));
  DCHECK(!(allow_port_migration && params_.go_away_on_path_degrading));

  // TODO(zhongyi): deprecate |goaway_sessions_on_ip_change| if the experiment
  // is no longer needed.
  // goaway_sessions_on_ip_change and close_sessions_on_ip_change should never
  // be simultaneously set to true.
  DCHECK(!(params_.close_sessions_on_ip_change &&
           params_.goaway_sessions_on_ip_change));

  bool handle_ip_change = params_.close_sessions_on_ip_change ||
                          params_.goaway_sessions_on_ip_change;
  // If IP address changes are handled explicitly, connection migration should
  // not be set.
  DCHECK(!(handle_ip_change && migrate_sessions_on_network_change));

  if (handle_ip_change)
    NetworkChangeNotifier::AddIPAddressObserver(this);

  // Port migration and early migration both act on path degrading and thus can
  // not be simultaneously set.
  DCHECK(!allow_port_migration || !migrate_sessions_early);

  if (allow_port_migration)
    params_.allow_port_migration = true;

  if (!NetworkChangeNotifier::AreNetworkHandlesSupported())
    return;

  NetworkChangeNotifier::AddNetworkObserver(this);
  // Perform checks on the connection migration options.
  if (!migrate_sessions_on_network_change) {
    DCHECK(!migrate_sessions_early);
    return;
  }

  // Enable migration on platform notifications.
  params_.migrate_sessions_on_network_change_v2 = true;

  if (!migrate_sessions_early) {
    DCHECK(!migrate_idle_sessions &&
           !retry_on_alternate_network_before_handshake);
    return;
  }

  // Enable migration on path degrading.
  params_.migrate_sessions_early_v2 = true;
  // Set retransmittable on wire timeout for migration on path degrading if no
  // value is specified.
  if (retransmittable_on_wire_timeout_.IsZero()) {
    retransmittable_on_wire_timeout_ = quic::QuicTime::Delta::FromMicroseconds(
        kDefaultRetransmittableOnWireTimeout.InMicroseconds());
  }

  // Enable retry on alternate network before handshake.
  if (retry_on_alternate_network_before_handshake)
    params_.retry_on_alternate_network_before_handshake = true;

  // Enable migration for idle sessions.
  if (migrate_idle_sessions)
    params_.migrate_idle_sessions = true;
}

void QuicStreamFactory::InitializeCachedStateInCryptoConfig(
    const CryptoClientConfigHandle& crypto_config_handle,
    const quic::QuicServerId& server_id,
    const std::unique_ptr<QuicServerInfo>& server_info,
    quic::QuicConnectionId* connection_id) {
  quic::QuicCryptoClientConfig::CachedState* cached =
      crypto_config_handle.GetConfig()->LookupOrCreate(server_id);

  if (!cached->IsEmpty()) {
    return;
  }

  if (!server_info || !server_info->Load()) {
    return;
  }

  cached->Initialize(server_info->state().server_config,
                     server_info->state().source_address_token,
                     server_info->state().certs, server_info->state().cert_sct,
                     server_info->state().chlo_hash,
                     server_info->state().server_config_sig, clock_->WallNow(),
                     quic::QuicWallTime::Zero());
}

void QuicStreamFactory::ProcessGoingAwaySession(
    QuicChromiumClientSession* session,
    const quic::QuicServerId& server_id,
    bool session_was_active) {
  if (!http_server_properties_)
    return;

  const quic::QuicConnectionStats& stats = session->connection()->GetStats();
  const AlternativeService alternative_service(
      kProtoQUIC, HostPortPair(server_id.host(), server_id.port()));

  url::SchemeHostPort server("https", server_id.host(), server_id.port());
  // Do nothing if QUIC is currently marked as broken.
  if (http_server_properties_->IsAlternativeServiceBroken(
          alternative_service,
          session->quic_session_key().network_isolation_key())) {
    return;
  }

  if (session->OneRttKeysAvailable()) {
    http_server_properties_->ConfirmAlternativeService(
        alternative_service,
        session->quic_session_key().network_isolation_key());
    ServerNetworkStats network_stats;
    network_stats.srtt = base::TimeDelta::FromMicroseconds(stats.srtt_us);
    network_stats.bandwidth_estimate = stats.estimated_bandwidth;
    http_server_properties_->SetServerNetworkStats(
        server, session->quic_session_key().network_isolation_key(),
        network_stats);
    return;
  }

  http_server_properties_->ClearServerNetworkStats(
      server, session->quic_session_key().network_isolation_key());

  UMA_HISTOGRAM_COUNTS_1M("Net.QuicHandshakeNotConfirmedNumPacketsReceived",
                          stats.packets_received);

  if (!session_was_active)
    return;

  // TODO(rch):  In the special case where the session has received no packets
  // from the peer, we should consider blocking this differently so that we
  // still race TCP but we don't consider the session connected until the
  // handshake has been confirmed.
  HistogramBrokenAlternateProtocolLocation(
      BROKEN_ALTERNATE_PROTOCOL_LOCATION_QUIC_STREAM_FACTORY);

  // Since the session was active, there's no longer an HttpStreamFactory::Job
  // running which can mark it broken, unless the TCP job also fails. So to
  // avoid not using QUIC when we otherwise could, we mark it as recently
  // broken, which means that 0-RTT will be disabled but we'll still race.
  http_server_properties_->MarkAlternativeServiceRecentlyBroken(
      alternative_service, session->quic_session_key().network_isolation_key());
}

std::unique_ptr<QuicStreamFactory::CryptoClientConfigHandle>
QuicStreamFactory::CreateCryptoConfigHandle(
    const NetworkIsolationKey& network_isolation_key) {
  NetworkIsolationKey actual_network_isolation_key =
      use_network_isolation_key_for_crypto_configs_ ? network_isolation_key
                                                    : NetworkIsolationKey();

  // If there's a matching entry in |active_crypto_config_map_|, create a
  // CryptoClientConfigHandle for it.
  auto map_iterator =
      active_crypto_config_map_.find(actual_network_isolation_key);
  if (map_iterator != active_crypto_config_map_.end()) {
    DCHECK_GT(map_iterator->second->num_refs(), 0);

    // If there's an active matching crypto config, there shouldn't also be an
    // inactive matching crypto config.
    DCHECK(recent_crypto_config_map_.Peek(actual_network_isolation_key) ==
           recent_crypto_config_map_.end());

    return std::make_unique<CryptoClientConfigHandle>(map_iterator);
  }

  // If there's a matching entry in |recent_crypto_config_map_|, move it to
  // |active_crypto_config_map_| and create a CryptoClientConfigHandle for it.
  auto mru_iterator =
      recent_crypto_config_map_.Peek(actual_network_isolation_key);
  if (mru_iterator != recent_crypto_config_map_.end()) {
    DCHECK_EQ(mru_iterator->second->num_refs(), 0);

    map_iterator = active_crypto_config_map_
                       .emplace(std::make_pair(actual_network_isolation_key,
                                               std::move(mru_iterator->second)))
                       .first;
    recent_crypto_config_map_.Erase(mru_iterator);
    return std::make_unique<CryptoClientConfigHandle>(map_iterator);
  }

  // Otherwise, create a new QuicCryptoClientConfigOwner and add it to
  // |active_crypto_config_map_|.
  std::unique_ptr<QuicCryptoClientConfigOwner> crypto_config_owner =
      std::make_unique<QuicCryptoClientConfigOwner>(
          std::make_unique<ProofVerifierChromium>(
              cert_verifier_, ct_policy_enforcer_, transport_security_state_,
              cert_transparency_verifier_, sct_auditing_delegate_,
              HostsFromOrigins(params_.origins_to_force_quic_on),
              actual_network_isolation_key),
          std::make_unique<QuicClientSessionCache>(), this);

  quic::QuicCryptoClientConfig* crypto_config = crypto_config_owner->config();
  crypto_config->set_user_agent_id(params_.user_agent_id);
  crypto_config->AddCanonicalSuffix(".c.youtube.com");
  crypto_config->AddCanonicalSuffix(".ggpht.com");
  crypto_config->AddCanonicalSuffix(".googlevideo.com");
  crypto_config->AddCanonicalSuffix(".googleusercontent.com");
  crypto_config->AddCanonicalSuffix(".gvt1.com");

  if (!prefer_aes_gcm_recorded_) {
    bool prefer_aes_gcm =
        !crypto_config->aead.empty() && (crypto_config->aead[0] == quic::kAESG);
    UMA_HISTOGRAM_BOOLEAN("Net.QuicSession.PreferAesGcm", prefer_aes_gcm);
    prefer_aes_gcm_recorded_ = true;
  }

  map_iterator = active_crypto_config_map_
                     .emplace(std::make_pair(actual_network_isolation_key,
                                             std::move(crypto_config_owner)))
                     .first;
  return std::make_unique<CryptoClientConfigHandle>(map_iterator);
}

void QuicStreamFactory::OnAllCryptoClientRefReleased(
    QuicCryptoClientConfigMap::iterator& map_iterator) {
  DCHECK_EQ(0, map_iterator->second->num_refs());
  recent_crypto_config_map_.Put(map_iterator->first,
                                std::move(map_iterator->second));
  active_crypto_config_map_.erase(map_iterator);
}

void QuicStreamFactory::CollectDataOnPlatformNotification(
    enum QuicPlatformNotification notification,
    NetworkChangeNotifier::NetworkHandle affected_network) const {
  UMA_HISTOGRAM_ENUMERATION("Net.QuicSession.PlatformNotification",
                            notification, NETWORK_NOTIFICATION_MAX);
  connectivity_monitor_.RecordConnectivityStatsToHistograms(
      QuicPlatformNotificationToString(notification), affected_network);
}

std::unique_ptr<QuicCryptoClientConfigHandle>
QuicStreamFactory::GetCryptoConfigForTesting(
    const NetworkIsolationKey& network_isolation_key) {
  return CreateCryptoConfigHandle(network_isolation_key);
}

bool QuicStreamFactory::CryptoConfigCacheIsEmptyForTesting(
    const quic::QuicServerId& server_id,
    const NetworkIsolationKey& network_isolation_key) {
  quic::QuicCryptoClientConfig::CachedState* cached = nullptr;
  NetworkIsolationKey actual_network_isolation_key =
      use_network_isolation_key_for_crypto_configs_ ? network_isolation_key
                                                    : NetworkIsolationKey();
  auto map_iterator =
      active_crypto_config_map_.find(actual_network_isolation_key);
  if (map_iterator != active_crypto_config_map_.end()) {
    cached = map_iterator->second->config()->LookupOrCreate(server_id);
  } else {
    auto mru_iterator =
        recent_crypto_config_map_.Peek(actual_network_isolation_key);
    if (mru_iterator != recent_crypto_config_map_.end()) {
      cached = mru_iterator->second->config()->LookupOrCreate(server_id);
    }
  }
  return !cached || cached->IsEmpty();
}

}  // namespace net
