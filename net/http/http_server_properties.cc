// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_server_properties.h"

#include "base/check_op.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/values.h"
#include "net/base/features.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/url_util.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_manager.h"
#include "net/socket/ssl_client_socket.h"
#include "net/ssl/ssl_config.h"
#include "url/url_constants.h"

namespace net {

namespace {

// Time to wait before starting an update the preferences from the
// http_server_properties_impl_ cache. Scheduling another update during this
// period will be a no-op.
constexpr base::TimeDelta kUpdatePrefsDelay = base::Seconds(60);

url::SchemeHostPort NormalizeSchemeHostPort(
    const url::SchemeHostPort& scheme_host_port) {
  if (scheme_host_port.scheme() == url::kWssScheme) {
    return url::SchemeHostPort(url::kHttpsScheme, scheme_host_port.host(),
                               scheme_host_port.port());
  }
  if (scheme_host_port.scheme() == url::kWsScheme) {
    return url::SchemeHostPort(url::kHttpScheme, scheme_host_port.host(),
                               scheme_host_port.port());
  }
  return scheme_host_port;
}

}  // namespace

HttpServerProperties::PrefDelegate::~PrefDelegate() = default;

HttpServerProperties::ServerInfo::ServerInfo() = default;
HttpServerProperties::ServerInfo::ServerInfo(const ServerInfo& server_info) =
    default;
HttpServerProperties::ServerInfo::ServerInfo(ServerInfo&& server_info) =
    default;
HttpServerProperties::ServerInfo::~ServerInfo() = default;

bool HttpServerProperties::ServerInfo::empty() const {
  return !supports_spdy.has_value() && !alternative_services.has_value() &&
         !server_network_stats.has_value();
}

bool HttpServerProperties::ServerInfo::operator==(
    const ServerInfo& other) const {
  return supports_spdy == other.supports_spdy &&
         alternative_services == other.alternative_services &&
         server_network_stats == other.server_network_stats;
}

HttpServerProperties::ServerInfoMapKey::ServerInfoMapKey(
    url::SchemeHostPort server,
    const NetworkAnonymizationKey& network_anonymization_key,
    bool use_network_anonymization_key)
    : server(std::move(server)),
      network_anonymization_key(use_network_anonymization_key
                                    ? network_anonymization_key
                                    : NetworkAnonymizationKey()) {
  // Scheme should have been normalized before this method was called.
  DCHECK_NE(this->server.scheme(), url::kWsScheme);
  DCHECK_NE(this->server.scheme(), url::kWssScheme);
}

HttpServerProperties::ServerInfoMapKey::~ServerInfoMapKey() = default;

bool HttpServerProperties::ServerInfoMapKey::operator<(
    const ServerInfoMapKey& other) const {
  return std::tie(server, network_anonymization_key) <
         std::tie(other.server, other.network_anonymization_key);
}

HttpServerProperties::QuicServerInfoMapKey::QuicServerInfoMapKey(
    const quic::QuicServerId& server_id,
    PrivacyMode privacy_mode,
    const NetworkAnonymizationKey& network_anonymization_key,
    bool use_network_anonymization_key)
    : server_id(server_id),
      privacy_mode(privacy_mode),
      network_anonymization_key(use_network_anonymization_key
                                    ? network_anonymization_key
                                    : NetworkAnonymizationKey()) {}

HttpServerProperties::QuicServerInfoMapKey::~QuicServerInfoMapKey() = default;

bool HttpServerProperties::QuicServerInfoMapKey::operator<(
    const QuicServerInfoMapKey& other) const {
  return std::tie(server_id, privacy_mode, network_anonymization_key) <
         std::tie(other.server_id, other.privacy_mode,
                  other.network_anonymization_key);
}

// Used in tests.
bool HttpServerProperties::QuicServerInfoMapKey::operator==(
    const QuicServerInfoMapKey& other) const {
  return std::tie(server_id, privacy_mode, network_anonymization_key) ==
         std::tie(other.server_id, other.privacy_mode,
                  other.network_anonymization_key);
}

HttpServerProperties::ServerInfoMap::ServerInfoMap()
    : base::LRUCache<ServerInfoMapKey, ServerInfo>(kMaxServerInfoEntries) {}

HttpServerProperties::ServerInfoMap::iterator
HttpServerProperties::ServerInfoMap::GetOrPut(const ServerInfoMapKey& key) {
  auto it = Get(key);
  if (it != end())
    return it;
  return Put(key, ServerInfo());
}

HttpServerProperties::ServerInfoMap::iterator
HttpServerProperties::ServerInfoMap::EraseIfEmpty(iterator server_info_it) {
  if (server_info_it->second.empty())
    return Erase(server_info_it);
  return ++server_info_it;
}

HttpServerProperties::HttpServerProperties(
    std::unique_ptr<PrefDelegate> pref_delegate,
    NetLog* net_log,
    const base::TickClock* tick_clock,
    base::Clock* clock)
    : tick_clock_(tick_clock ? tick_clock
                             : base::DefaultTickClock::GetInstance()),
      clock_(clock ? clock : base::DefaultClock::GetInstance()),
      use_network_anonymization_key_(
          NetworkAnonymizationKey::IsPartitioningEnabled()),
      is_initialized_(pref_delegate.get() == nullptr),
      properties_manager_(
          pref_delegate
              ? std::make_unique<HttpServerPropertiesManager>(
                    std::move(pref_delegate),
                    base::BindOnce(&HttpServerProperties::OnPrefsLoaded,
                                   base::Unretained(this)),
                    kDefaultMaxQuicServerEntries,
                    net_log,
                    tick_clock_)
              : nullptr),
      broken_alternative_services_(kMaxRecentlyBrokenAlternativeServiceEntries,
                                   this,
                                   tick_clock_),
      canonical_suffixes_({".ggpht.com", ".c.youtube.com", ".googlevideo.com",
                           ".googleusercontent.com", ".gvt1.com"}),
      quic_server_info_map_(kDefaultMaxQuicServerEntries),
      max_server_configs_stored_in_properties_(kDefaultMaxQuicServerEntries) {
  // Identify known QUIC alternative services, if any.
  MaybeProcessQuicHints();
}

HttpServerProperties::~HttpServerProperties() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (properties_manager_) {
    // Stop waiting for initial settings.
    is_initialized_ = true;

    // Stop the timer if it's running, since this will write to the properties
    // file immediately.
    prefs_update_timer_.Stop();

    WriteProperties(base::OnceClosure());
  }
}

void HttpServerProperties::Clear(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  server_info_map_.Clear();
  broken_alternative_services_.Clear();
  canonical_alt_svc_map_.clear();
  last_local_address_when_quic_worked_ = IPAddress();
  quic_server_info_map_.Clear();
  canonical_server_info_map_.clear();

  if (properties_manager_) {
    // Stop waiting for initial settings.
    is_initialized_ = true;
    // Leaving this as-is doesn't actually have any effect, if it's true, but
    // seems best to be safe.
    queue_write_on_load_ = false;

    // Stop the timer if it's running, since this will write to the properties
    // file immediately.
    prefs_update_timer_.Stop();
    WriteProperties(std::move(callback));
  } else if (callback) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
  }
}

bool HttpServerProperties::SupportsRequestPriority(
    const url::SchemeHostPort& server,
    const NetworkAnonymizationKey& network_anonymization_key) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (server.host().empty())
    return false;

  if ((server.scheme() == url::kHttpScheme ||
       server.scheme() == url::kHttpsScheme) &&
      GetSupportsSpdy(server, network_anonymization_key)) {
    return true;
  }
  const AlternativeServiceInfoVector alternative_service_info_vector =
      GetAlternativeServiceInfos(server, network_anonymization_key);
  for (const AlternativeServiceInfo& alternative_service_info :
       alternative_service_info_vector) {
    if (alternative_service_info.alternative_service().protocol ==
        NextProto::kProtoQUIC) {
      return true;
    }
  }
  return false;
}

bool HttpServerProperties::GetSupportsSpdy(
    const url::SchemeHostPort& server,
    const NetworkAnonymizationKey& network_anonymization_key) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(server.scheme() == url::kHttpScheme ||
        server.scheme() == url::kHttpsScheme);
  if (server.host().empty()) {
    return false;
  }

  auto server_info = server_info_map_.Get(
      CreateServerInfoKey(server, network_anonymization_key));
  return server_info != server_info_map_.end() &&
         server_info->second.supports_spdy.value_or(false);
}

void HttpServerProperties::SetSupportsSpdy(
    const url::SchemeHostPort& server,
    const NetworkAnonymizationKey& network_anonymization_key,
    bool supports_spdy) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(server.scheme() == url::kHttpScheme ||
        server.scheme() == url::kHttpsScheme);
  if (server.host().empty()) {
    return;
  }

  auto server_info = server_info_map_.GetOrPut(
      CreateServerInfoKey(server, network_anonymization_key));
  // If value is already the same as `supports_spdy`, or value is unset and
  // `supports_spdy` is false, don't queue a write.
  bool queue_write =
      server_info->second.supports_spdy.value_or(false) != supports_spdy;
  server_info->second.supports_spdy = supports_spdy;

  if (queue_write) {
    MaybeQueueWriteProperties();
  }
}

bool HttpServerProperties::RequiresHTTP11(
    const url::SchemeHostPort& server,
    const NetworkAnonymizationKey& network_anonymization_key) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Avoid overhead of copying the SchemeHostPort and the NAK in the very likely
  // case the list of servers is empty.
  if (servers_requiring_http_11_.empty()) {
    return false;
  }
  return RequiresHTTP11Internal(NormalizeSchemeHostPort(server),
                                network_anonymization_key);
}

void HttpServerProperties::SetHTTP11Required(
    const url::SchemeHostPort& server,
    const NetworkAnonymizationKey& network_anonymization_key) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SetHTTP11RequiredInternal(NormalizeSchemeHostPort(server),
                            network_anonymization_key);
}

void HttpServerProperties::MaybeForceHTTP11(
    const url::SchemeHostPort& server,
    const NetworkAnonymizationKey& network_anonymization_key,
    SSLConfig* ssl_config) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // No need for separate internal method here, since this can use
  // RequiresHTTP11() to do the work normalizing `server`.
  if (RequiresHTTP11(server, network_anonymization_key)) {
    ssl_config->alpn_protos.clear();
    ssl_config->alpn_protos.push_back(NextProto::kProtoHTTP11);
  }
}

AlternativeServiceInfoVector HttpServerProperties::GetAlternativeServiceInfos(
    const url::SchemeHostPort& origin,
    const NetworkAnonymizationKey& network_anonymization_key) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return GetAlternativeServiceInfosInternal(NormalizeSchemeHostPort(origin),
                                            network_anonymization_key);
}

void HttpServerProperties::SetHttp2AlternativeService(
    const url::SchemeHostPort& origin,
    const NetworkAnonymizationKey& network_anonymization_key,
    const AlternativeService& alternative_service,
    base::Time expiration) {
  DCHECK_EQ(alternative_service.protocol, NextProto::kProtoHTTP2);

  SetAlternativeServices(
      origin, network_anonymization_key,
      AlternativeServiceInfoVector(
          /*size=*/1, AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
                          alternative_service, expiration)));
}

void HttpServerProperties::SetQuicAlternativeService(
    const url::SchemeHostPort& origin,
    const NetworkAnonymizationKey& network_anonymization_key,
    const AlternativeService& alternative_service,
    base::Time expiration,
    const quic::ParsedQuicVersionVector& advertised_versions) {
  DCHECK(alternative_service.protocol == NextProto::kProtoQUIC);

  SetAlternativeServices(
      origin, network_anonymization_key,
      AlternativeServiceInfoVector(
          /*size=*/1,
          AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
              alternative_service, expiration, advertised_versions)));
}

void HttpServerProperties::SetAlternativeServices(
    const url::SchemeHostPort& origin,
    const NetworkAnonymizationKey& network_anonymization_key,
    const AlternativeServiceInfoVector& alternative_service_info_vector) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SetAlternativeServicesInternal(NormalizeSchemeHostPort(origin),
                                 network_anonymization_key,
                                 alternative_service_info_vector);
}

void HttpServerProperties::MaybeProcessQuicHints() {
  if (!base::FeatureList::IsEnabled(features::kConfigureQuicHints)) {
    return;
  }

  // QUIC hints are in the format: host,port,alternate_port
  const std::string comma_separated = features::kQuicHintHostPortPairs.Get();
  auto split = base::SplitStringPiece(
      comma_separated, ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  // Only process QUIC hints if they are present and well-formed
  // i.e. every 3-tuple entry is complete
  if (!split.empty() && split.size() % 3 == 0) {
    for (size_t i = 0; i + 2 < split.size(); i += 3) {
      ValidateAndMaybeAddQuicHint(split[i], split[i + 1], split[i + 2]);
    }
  }

  // Wildcard QUIC hints are in the format: .wildcard_suffix,port,alternate_port
  // Note that a '*' is not included before the wildcard suffix to avoid
  // needlessly removing it from the parameter.
  const std::string comma_separated_wildcards =
      features::kWildcardQuicHintHostPortPairs.Get();
  auto wildcards_split =
      base::SplitStringPiece(comma_separated_wildcards, ",",
                             base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (!wildcards_split.empty() && wildcards_split.size() % 3 == 0) {
    for (size_t i = 0; i + 2 < wildcards_split.size(); i += 3) {
      ValidateAndMaybeAddQuicHint(wildcards_split[i], wildcards_split[i + 1],
                                  wildcards_split[i + 2], /*is_suffix=*/true);
    }
  }
}

void HttpServerProperties::ValidateAndMaybeAddQuicHint(
    std::string_view host,
    std::string_view port_string,
    std::string_view alternate_port_string,
    bool is_suffix) {
  url::CanonHostInfo host_info;
  std::string canon_host(net::CanonicalizeHost(host, &host_info));
  if (is_suffix) {
    // Suffixes are required to start with "." to prevent unintentional matching
    // i.e. "evil-example.com" with "example.com"
    if (!base::StartsWith(canon_host, ".")) {
      DLOG(ERROR) << "Invalid QUIC hint suffix: " << host;
      return;
    }
  } else {
    // IP addresses (e.g. DoH destinations) or well-formed hosts are valid
    // QUIC hints
    if (!host_info.IsIPAddress() &&
        !net::IsCanonicalizedHostCompliant(canon_host)) {
      DLOG(ERROR) << "Invalid QUIC hint host: " << host;
      return;
    }
  }

  int port = 0;
  if (!base::StringToInt(port_string, &port)) {
    DLOG(WARNING) << "Could not parse port number: " << port_string;
    return;
  }
  if (port <= std::numeric_limits<uint16_t>::min() ||
      port > std::numeric_limits<uint16_t>::max()) {
    DLOG(ERROR) << "Invalid QUIC hint port: " << port;
    return;
  }

  int alternate_port = 0;
  if (!base::StringToInt(alternate_port_string, &alternate_port)) {
    DLOG(WARNING) << "Could not parse alternate port number: "
                  << alternate_port_string;
    return;
  }
  if (alternate_port <= std::numeric_limits<uint16_t>::min() ||
      alternate_port > std::numeric_limits<uint16_t>::max()) {
    DLOG(ERROR) << "Invalid QUIC hint alternate port: " << alternate_port;
    return;
  }

  SetKnownQuicAlternativeService(canon_host, port, alternate_port, is_suffix);
}

void HttpServerProperties::SetKnownQuicAlternativeService(
    std::string_view canon_host,
    int port,
    int alternate_port,
    bool is_suffix) {
  if (!is_suffix) {
    url::SchemeHostPort quic_server(url::kHttpsScheme, canon_host, port);
    AlternativeService alternative_service(
        net::NextProto::kProtoQUIC, canon_host,
        static_cast<uint16_t>(alternate_port));
    known_alternative_service_map_[quic_server] =
        std::move(alternative_service);
    return;
  }

  // Wildcard suffixes are reversed and added to
  // `reversed_known_alternative_service_suffixes_set_` to allow matching
  // hostnames to use the corresponding known alternative service.
  std::string reversed_host(canon_host);
  std::ranges::reverse(reversed_host);
  url::SchemeHostPort quic_server(url::kHttpsScheme, reversed_host, port);
  AlternativeService alternative_service(net::NextProto::kProtoQUIC,
                                         reversed_host,
                                         static_cast<uint16_t>(alternate_port));
  wildcard_known_alternative_service_map_[quic_server] =
      std::move(alternative_service);
  reversed_known_alternative_service_suffixes_set_.insert(reversed_host);
}

void HttpServerProperties::MarkAlternativeServiceBroken(
    const AlternativeService& alternative_service,
    const NetworkAnonymizationKey& network_anonymization_key) {
  broken_alternative_services_.MarkBroken(
      BrokenAlternativeService(alternative_service, network_anonymization_key,
                               use_network_anonymization_key_));
  MaybeQueueWriteProperties();
}

void HttpServerProperties::
    MarkAlternativeServiceBrokenUntilDefaultNetworkChanges(
        const AlternativeService& alternative_service,
        const NetworkAnonymizationKey& network_anonymization_key) {
  broken_alternative_services_.MarkBrokenUntilDefaultNetworkChanges(
      BrokenAlternativeService(alternative_service, network_anonymization_key,
                               use_network_anonymization_key_));
  MaybeQueueWriteProperties();
}

void HttpServerProperties::MarkAlternativeServiceRecentlyBroken(
    const AlternativeService& alternative_service,
    const NetworkAnonymizationKey& network_anonymization_key) {
  broken_alternative_services_.MarkRecentlyBroken(
      BrokenAlternativeService(alternative_service, network_anonymization_key,
                               use_network_anonymization_key_));
  MaybeQueueWriteProperties();
}

bool HttpServerProperties::IsAlternativeServiceBroken(
    const AlternativeService& alternative_service,
    const NetworkAnonymizationKey& network_anonymization_key) const {
  return broken_alternative_services_.IsBroken(
      BrokenAlternativeService(alternative_service, network_anonymization_key,
                               use_network_anonymization_key_));
}

bool HttpServerProperties::WasAlternativeServiceRecentlyBroken(
    const AlternativeService& alternative_service,
    const NetworkAnonymizationKey& network_anonymization_key) {
  return broken_alternative_services_.WasRecentlyBroken(
      BrokenAlternativeService(alternative_service, network_anonymization_key,
                               use_network_anonymization_key_));
}

void HttpServerProperties::ConfirmAlternativeService(
    const AlternativeService& alternative_service,
    const NetworkAnonymizationKey& network_anonymization_key) {
  bool old_value = IsAlternativeServiceBroken(alternative_service,
                                              network_anonymization_key);
  broken_alternative_services_.Confirm(
      BrokenAlternativeService(alternative_service, network_anonymization_key,
                               use_network_anonymization_key_));
  bool new_value = IsAlternativeServiceBroken(alternative_service,
                                              network_anonymization_key);

  // For persisting, we only care about the value returned by
  // IsAlternativeServiceBroken. If that value changes, then call persist.
  if (old_value != new_value)
    MaybeQueueWriteProperties();
}

void HttpServerProperties::OnDefaultNetworkChanged() {
  bool changed = broken_alternative_services_.OnDefaultNetworkChanged();
  if (changed)
    MaybeQueueWriteProperties();
}

base::Value HttpServerProperties::GetAlternativeServiceInfoAsValue() const {
  const base::Time now = clock_->Now();
  const base::TimeTicks now_ticks = tick_clock_->NowTicks();
  base::Value::List dict_list;
  for (const auto& server_info : server_info_map_) {
    if (!server_info.second.alternative_services.has_value())
      continue;
    base::Value::List alternative_service_list;
    const ServerInfoMapKey& key = server_info.first;
    for (const AlternativeServiceInfo& alternative_service_info :
         server_info.second.alternative_services.value()) {
      std::string alternative_service_string(
          alternative_service_info.ToString());
      AlternativeService alternative_service(
          alternative_service_info.alternative_service());
      if (alternative_service.host.empty()) {
        alternative_service.host = key.server.host();
      }
      base::TimeTicks brokenness_expiration_ticks;
      if (broken_alternative_services_.IsBroken(
              BrokenAlternativeService(
                  alternative_service,
                  server_info.first.network_anonymization_key,
                  use_network_anonymization_key_),
              &brokenness_expiration_ticks)) {
        // Convert |brokenness_expiration| from TimeTicks to Time.
        //
        // Note: Cannot use `base::UnlocalizedTimeFormatWithPattern()` since
        // `net/DEPS` disallows `base/i18n`.
        base::Time brokenness_expiration =
            now + (brokenness_expiration_ticks - now_ticks);
        base::Time::Exploded exploded;
        brokenness_expiration.LocalExplode(&exploded);
        std::string broken_info_string =
            " (broken until " +
            base::StringPrintf("%04d-%02d-%02d %0d:%0d:%0d", exploded.year,
                               exploded.month, exploded.day_of_month,
                               exploded.hour, exploded.minute,
                               exploded.second) +
            ")";
        alternative_service_string.append(broken_info_string);
      }
      alternative_service_list.Append(std::move(alternative_service_string));
    }
    if (alternative_service_list.empty())
      continue;
    base::Value::Dict dict;
    dict.Set("server", key.server.Serialize());
    dict.Set("network_anonymization_key",
             key.network_anonymization_key.ToDebugString());
    dict.Set("alternative_service", std::move(alternative_service_list));
    dict_list.Append(std::move(dict));
  }
  return base::Value(std::move(dict_list));
}

bool HttpServerProperties::WasLastLocalAddressWhenQuicWorked(
    const IPAddress& local_address) const {
  return !last_local_address_when_quic_worked_.empty() &&
         last_local_address_when_quic_worked_ == local_address;
}

bool HttpServerProperties::HasLastLocalAddressWhenQuicWorked() const {
  return !last_local_address_when_quic_worked_.empty();
}

void HttpServerProperties::SetLastLocalAddressWhenQuicWorked(
    IPAddress last_local_address_when_quic_worked) {
  DCHECK(!last_local_address_when_quic_worked.empty());
  if (last_local_address_when_quic_worked_ ==
      last_local_address_when_quic_worked) {
    return;
  }

  last_local_address_when_quic_worked_ = last_local_address_when_quic_worked;
  MaybeQueueWriteProperties();
}

void HttpServerProperties::ClearLastLocalAddressWhenQuicWorked() {
  if (last_local_address_when_quic_worked_.empty())
    return;

  last_local_address_when_quic_worked_ = IPAddress();
  MaybeQueueWriteProperties();
}

void HttpServerProperties::SetServerNetworkStats(
    const url::SchemeHostPort& server,
    const NetworkAnonymizationKey& network_anonymization_key,
    ServerNetworkStats stats) {
  SetServerNetworkStatsInternal(NormalizeSchemeHostPort(server),
                                network_anonymization_key, std::move(stats));
}

void HttpServerProperties::ClearServerNetworkStats(
    const url::SchemeHostPort& server,
    const NetworkAnonymizationKey& network_anonymization_key) {
  ClearServerNetworkStatsInternal(NormalizeSchemeHostPort(server),
                                  network_anonymization_key);
}

const ServerNetworkStats* HttpServerProperties::GetServerNetworkStats(
    const url::SchemeHostPort& server,
    const NetworkAnonymizationKey& network_anonymization_key) {
  return GetServerNetworkStatsInternal(NormalizeSchemeHostPort(server),
                                       network_anonymization_key);
}

void HttpServerProperties::SetQuicServerInfo(
    const quic::QuicServerId& server_id,
    PrivacyMode privacy_mode,
    const NetworkAnonymizationKey& network_anonymization_key,
    const std::string& server_info) {
  QuicServerInfoMapKey key = CreateQuicServerInfoKey(server_id, privacy_mode,
                                                     network_anonymization_key);
  auto it = quic_server_info_map_.Peek(key);
  bool changed =
      (it == quic_server_info_map_.end() || it->second != server_info);
  quic_server_info_map_.Put(key, server_info);
  UpdateCanonicalServerInfoMap(key);
  if (changed)
    MaybeQueueWriteProperties();
}

const std::string* HttpServerProperties::GetQuicServerInfo(
    const quic::QuicServerId& server_id,
    PrivacyMode privacy_mode,
    const NetworkAnonymizationKey& network_anonymization_key) {
  QuicServerInfoMapKey key = CreateQuicServerInfoKey(server_id, privacy_mode,
                                                     network_anonymization_key);
  auto it = quic_server_info_map_.Get(key);
  if (it != quic_server_info_map_.end()) {
    // Since |canonical_server_info_map_| should always map to the most
    // recent host, update it with the one that became MRU in
    // |quic_server_info_map_|.
    UpdateCanonicalServerInfoMap(key);
    return &it->second;
  }

  // If the exact match for |server_id| wasn't found, check
  // |canonical_server_info_map_| whether there is server info for a host with
  // the same canonical host suffix.
  auto canonical_itr = GetCanonicalServerInfoHost(key);
  if (canonical_itr == canonical_server_info_map_.end())
    return nullptr;

  // When search in |quic_server_info_map_|, do not change the MRU order.
  it = quic_server_info_map_.Peek(CreateQuicServerInfoKey(
      canonical_itr->second, privacy_mode, network_anonymization_key));
  if (it != quic_server_info_map_.end())
    return &it->second;

  return nullptr;
}

const HttpServerProperties::QuicServerInfoMap&
HttpServerProperties::quic_server_info_map() const {
  return quic_server_info_map_;
}

size_t HttpServerProperties::max_server_configs_stored_in_properties() const {
  return max_server_configs_stored_in_properties_;
}

void HttpServerProperties::SetMaxServerConfigsStoredInProperties(
    size_t max_server_configs_stored_in_properties) {
  // Do nothing if the new size is the same as the old one.
  if (max_server_configs_stored_in_properties_ ==
      max_server_configs_stored_in_properties) {
    return;
  }

  max_server_configs_stored_in_properties_ =
      max_server_configs_stored_in_properties;

  // LRUCache doesn't allow the capacity of the cache to be changed. Thus
  // create a new map with the new size and add current elements and swap the
  // new map.
  quic_server_info_map_.ShrinkToSize(max_server_configs_stored_in_properties_);
  QuicServerInfoMap temp_map(max_server_configs_stored_in_properties_);
  // Update the |canonical_server_info_map_| as well, so it stays in sync with
  // |quic_server_info_map_|.
  canonical_server_info_map_ = QuicCanonicalMap();
  for (const auto& [key, server_info] : base::Reversed(quic_server_info_map_)) {
    temp_map.Put(key, server_info);
    UpdateCanonicalServerInfoMap(key);
  }

  quic_server_info_map_.Swap(temp_map);
  if (properties_manager_) {
    properties_manager_->set_max_server_configs_stored_in_properties(
        max_server_configs_stored_in_properties);
  }
}

void HttpServerProperties::SetBrokenAlternativeServicesDelayParams(
    std::optional<base::TimeDelta> initial_delay,
    std::optional<bool> exponential_backoff_on_initial_delay) {
  broken_alternative_services_.SetDelayParams(
      initial_delay, exponential_backoff_on_initial_delay);
}

bool HttpServerProperties::IsInitialized() const {
  return is_initialized_;
}

void HttpServerProperties::OnExpireBrokenAlternativeService(
    const AlternativeService& expired_alternative_service,
    const NetworkAnonymizationKey& network_anonymization_key) {
  // Remove every occurrence of |expired_alternative_service| from
  // |alternative_service_map_|.
  for (auto map_it = server_info_map_.begin();
       map_it != server_info_map_.end();) {
    if (!map_it->second.alternative_services.has_value() ||
        map_it->first.network_anonymization_key != network_anonymization_key) {
      ++map_it;
      continue;
    }
    AlternativeServiceInfoVector* service_info =
        &map_it->second.alternative_services.value();
    for (auto it = service_info->begin(); it != service_info->end();) {
      AlternativeService alternative_service(it->alternative_service());
      // Empty hostname in map means hostname of key: substitute before
      // comparing to |expired_alternative_service|.
      if (alternative_service.host.empty()) {
        alternative_service.host = map_it->first.server.host();
      }
      if (alternative_service == expired_alternative_service) {
        it = service_info->erase(it);
        continue;
      }
      ++it;
    }
    // If an origin has an empty list of alternative services, then remove it
    // from both |canonical_alt_svc_map_| and
    // |alternative_service_map_|.
    if (service_info->empty()) {
      RemoveAltSvcCanonicalHost(map_it->first.server,
                                network_anonymization_key);
      map_it->second.alternative_services.reset();
      map_it = server_info_map_.EraseIfEmpty(map_it);
      continue;
    }
    ++map_it;
  }
}

base::TimeDelta HttpServerProperties::GetUpdatePrefsDelayForTesting() {
  return kUpdatePrefsDelay;
}

bool HttpServerProperties::RequiresHTTP11Internal(
    url::SchemeHostPort server,
    const NetworkAnonymizationKey& network_anonymization_key) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(server.scheme(), url::kWsScheme);
  DCHECK_NE(server.scheme(), url::kWssScheme);
  if (server.host().empty())
    return false;

  auto it = servers_requiring_http_11_.Get(
      CreateServerInfoKey(std::move(server), network_anonymization_key));
  return it != servers_requiring_http_11_.end();
}

void HttpServerProperties::SetHTTP11RequiredInternal(
    url::SchemeHostPort server,
    const NetworkAnonymizationKey& network_anonymization_key) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(server.scheme(), url::kWsScheme);
  DCHECK_NE(server.scheme(), url::kWssScheme);
  if (server.host().empty())
    return;

  servers_requiring_http_11_.Put(
      CreateServerInfoKey(std::move(server), network_anonymization_key));
  // No need to call MaybeQueueWriteProperties(), as this information is not
  // persisted to preferences.
}

AlternativeServiceInfoVector
HttpServerProperties::GetAlternativeServiceInfosInternal(
    const url::SchemeHostPort& origin,
    const NetworkAnonymizationKey& network_anonymization_key) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(origin.scheme(), url::kWsScheme);
  DCHECK_NE(origin.scheme(), url::kWssScheme);

  // Copy valid alternative service infos into
  // |valid_alternative_service_infos|.
  AlternativeServiceInfoVector valid_alternative_service_infos;
  const base::Time now = clock_->Now();
  auto map_it = server_info_map_.Get(
      CreateServerInfoKey(origin, network_anonymization_key));
  if (map_it != server_info_map_.end() &&
      map_it->second.alternative_services.has_value()) {
    AlternativeServiceInfoVector* service_info =
        &map_it->second.alternative_services.value();
    HostPortPair host_port_pair(origin.host(), origin.port());
    for (auto it = service_info->begin(); it != service_info->end();) {
      if (it->expiration() < now) {
        it = service_info->erase(it);
        continue;
      }
      AlternativeService alternative_service(it->alternative_service());
      if (alternative_service.host.empty()) {
        alternative_service.host = origin.host();
      }
      // If the alternative service is equivalent to the origin (same host,
      // same port, and both TCP), skip it.
      if (host_port_pair == alternative_service.GetHostPortPair() &&
          alternative_service.protocol == NextProto::kProtoHTTP2) {
        ++it;
        continue;
      }
      if (alternative_service.protocol == NextProto::kProtoQUIC) {
        valid_alternative_service_infos.push_back(
            AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
                alternative_service, it->expiration(),
                it->advertised_versions()));
      } else {
        valid_alternative_service_infos.push_back(
            AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
                alternative_service, it->expiration()));
      }
      ++it;
    }
    if (service_info->empty()) {
      map_it->second.alternative_services.reset();
      server_info_map_.EraseIfEmpty(map_it);
    }
    return valid_alternative_service_infos;
  }

  // If a more specific alternative service has not been found, look for
  // preconfigured known alternative services.
  std::optional<AlternativeService> known_alternative_service =
      GetKnownAltSvcHost(origin);
  if (known_alternative_service) {
    // Update the host to use the full hostname instead of a possible wildcard
    // suffix.
    known_alternative_service->host = origin.host();
    if (known_alternative_service->protocol == NextProto::kProtoQUIC &&
        !IsAlternativeServiceBroken(*known_alternative_service,
                                    network_anonymization_key)) {
      valid_alternative_service_infos.push_back(
          AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
              *known_alternative_service, base::Time::Max(),
              DefaultSupportedQuicVersions()));
      return valid_alternative_service_infos;
    }
  }

  auto canonical = GetCanonicalAltSvcHost(origin, network_anonymization_key);
  if (canonical == canonical_alt_svc_map_.end()) {
    return AlternativeServiceInfoVector();
  }
  map_it = server_info_map_.Get(
      CreateServerInfoKey(canonical->second, network_anonymization_key));
  if (map_it == server_info_map_.end() ||
      !map_it->second.alternative_services.has_value()) {
    return AlternativeServiceInfoVector();
  }
  AlternativeServiceInfoVector* service_info =
      &map_it->second.alternative_services.value();
  for (auto it = service_info->begin(); it != service_info->end();) {
    if (it->expiration() < now) {
      it = service_info->erase(it);
      continue;
    }
    AlternativeService alternative_service(it->alternative_service());
    if (alternative_service.host.empty()) {
      alternative_service.host = canonical->second.host();
      if (IsAlternativeServiceBroken(alternative_service,
                                     network_anonymization_key)) {
        ++it;
        continue;
      }
      alternative_service.host = origin.host();
    } else if (IsAlternativeServiceBroken(alternative_service,
                                          network_anonymization_key)) {
      ++it;
      continue;
    }
    if (alternative_service.protocol == NextProto::kProtoQUIC) {
      valid_alternative_service_infos.push_back(
          AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
              alternative_service, it->expiration(),
              it->advertised_versions()));
    } else {
      valid_alternative_service_infos.push_back(
          AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
              alternative_service, it->expiration()));
    }
    ++it;
  }
  if (service_info->empty())
    server_info_map_.EraseIfEmpty(map_it);
  return valid_alternative_service_infos;
}

void HttpServerProperties::SetAlternativeServicesInternal(
    const url::SchemeHostPort& origin,
    const NetworkAnonymizationKey& network_anonymization_key,
    const AlternativeServiceInfoVector& alternative_service_info_vector) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(origin.scheme(), url::kWsScheme);
  DCHECK_NE(origin.scheme(), url::kWssScheme);

  if (alternative_service_info_vector.empty()) {
    RemoveAltSvcCanonicalHost(origin, network_anonymization_key);
    // Don't bother moving to front when erasing information.
    auto it = server_info_map_.Peek(
        CreateServerInfoKey(origin, network_anonymization_key));

    if (it == server_info_map_.end() ||
        !it->second.alternative_services.has_value()) {
      return;
    }

    it->second.alternative_services.reset();
    server_info_map_.EraseIfEmpty(it);
    MaybeQueueWriteProperties();
    return;
  }

  auto it = server_info_map_.GetOrPut(
      CreateServerInfoKey(origin, network_anonymization_key));
  bool need_update_pref = true;
  if (it->second.alternative_services.has_value()) {
    DCHECK(!it->second.empty());
    if (it->second.alternative_services->size() ==
        alternative_service_info_vector.size()) {
      const base::Time now = clock_->Now();
      need_update_pref = false;
      auto new_it = alternative_service_info_vector.begin();
      for (const auto& old : *it->second.alternative_services) {
        // Persist to disk immediately if new entry has different scheme,
        // host, or port.
        if (old.alternative_service() != new_it->alternative_service()) {
          need_update_pref = true;
          break;
        }
        // Also persist to disk if new expiration it more that twice as far or
        // less than half as far in the future.
        base::Time old_time = old.expiration();
        base::Time new_time = new_it->expiration();
        if (new_time - now > 2 * (old_time - now) ||
            2 * (new_time - now) < (old_time - now)) {
          need_update_pref = true;
          break;
        }
        // Also persist to disk if new entry has a different list of
        // advertised versions.
        if (old.advertised_versions() != new_it->advertised_versions()) {
          need_update_pref = true;
          break;
        }
        ++new_it;
      }
    }
  }

  const bool previously_no_alternative_services =
      (GetIteratorWithAlternativeServiceInfo(
           origin, network_anonymization_key) == server_info_map_.end());

  it->second.alternative_services = alternative_service_info_vector;

  if (previously_no_alternative_services &&
      !GetAlternativeServiceInfos(origin, network_anonymization_key).empty()) {
    // TODO(rch): Consider the case where multiple requests are started
    // before the first completes. In this case, only one of the jobs
    // would reach this code, whereas all of them should should have.
    HistogramAlternateProtocolUsage(ALTERNATE_PROTOCOL_USAGE_MAPPING_MISSING,
                                    IsGoogleHost(origin.host()));
  }

  // If this host ends with a canonical suffix, then set it as the
  // canonical host.
  const char* kCanonicalScheme = "https";
  if (origin.scheme() == kCanonicalScheme) {
    const std::string* canonical_suffix = GetCanonicalSuffix(origin.host());
    if (canonical_suffix != nullptr) {
      url::SchemeHostPort canonical_server(kCanonicalScheme, *canonical_suffix,
                                           origin.port());
      canonical_alt_svc_map_[CreateServerInfoKey(
          canonical_server, network_anonymization_key)] = origin;
    }
  }

  if (need_update_pref)
    MaybeQueueWriteProperties();
}

void HttpServerProperties::SetServerNetworkStatsInternal(
    url::SchemeHostPort server,
    const NetworkAnonymizationKey& network_anonymization_key,
    ServerNetworkStats stats) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(server.scheme(), url::kWsScheme);
  DCHECK_NE(server.scheme(), url::kWssScheme);

  auto server_info = server_info_map_.GetOrPut(
      CreateServerInfoKey(std::move(server), network_anonymization_key));
  bool changed = !server_info->second.server_network_stats.has_value() ||
                 server_info->second.server_network_stats.value() != stats;

  if (changed) {
    server_info->second.server_network_stats = stats;
    MaybeQueueWriteProperties();
  }
}

void HttpServerProperties::ClearServerNetworkStatsInternal(
    url::SchemeHostPort server,
    const NetworkAnonymizationKey& network_anonymization_key) {
  auto server_info = server_info_map_.Peek(
      CreateServerInfoKey(std::move(server), network_anonymization_key));
  // If stats are empty, nothing to do.
  if (server_info == server_info_map_.end() ||
      !server_info->second.server_network_stats.has_value()) {
    return;
  }

  // Otherwise, clear and delete if needed. No need to bring to front of MRU
  // cache when clearing data.
  server_info->second.server_network_stats.reset();
  if (server_info->second.empty())
    server_info_map_.EraseIfEmpty(server_info);
  MaybeQueueWriteProperties();
}

const ServerNetworkStats* HttpServerProperties::GetServerNetworkStatsInternal(
    url::SchemeHostPort server,
    const NetworkAnonymizationKey& network_anonymization_key) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(server.scheme(), url::kWsScheme);
  DCHECK_NE(server.scheme(), url::kWssScheme);

  auto server_info = server_info_map_.Get(
      CreateServerInfoKey(std::move(server), network_anonymization_key));
  if (server_info == server_info_map_.end() ||
      !server_info->second.server_network_stats.has_value()) {
    return nullptr;
  }
  return &server_info->second.server_network_stats.value();
}

HttpServerProperties::QuicServerInfoMapKey
HttpServerProperties::CreateQuicServerInfoKey(
    const quic::QuicServerId& server_id,
    PrivacyMode privacy_mode,
    const NetworkAnonymizationKey& network_anonymization_key) const {
  return QuicServerInfoMapKey(server_id, privacy_mode,
                              network_anonymization_key,
                              use_network_anonymization_key_);
}

HttpServerProperties::ServerInfoMapKey
HttpServerProperties::CreateServerInfoKey(
    const url::SchemeHostPort& server,
    const NetworkAnonymizationKey& network_anonymization_key) const {
  return ServerInfoMapKey(server, network_anonymization_key,
                          use_network_anonymization_key_);
}

HttpServerProperties::ServerInfoMap::const_iterator
HttpServerProperties::GetIteratorWithAlternativeServiceInfo(
    const url::SchemeHostPort& server,
    const NetworkAnonymizationKey& network_anonymization_key) {
  ServerInfoMap::const_iterator it = server_info_map_.Get(
      CreateServerInfoKey(server, network_anonymization_key));
  if (it != server_info_map_.end() && it->second.alternative_services)
    return it;

  auto canonical = GetCanonicalAltSvcHost(server, network_anonymization_key);
  if (canonical == canonical_alt_svc_map_.end()) {
    return server_info_map_.end();
  }

  const url::SchemeHostPort canonical_server = canonical->second;
  it = server_info_map_.Get(
      CreateServerInfoKey(canonical_server, network_anonymization_key));
  if (it == server_info_map_.end() || !it->second.alternative_services)
    return server_info_map_.end();

  for (const AlternativeServiceInfo& alternative_service_info :
       it->second.alternative_services.value()) {
    AlternativeService alternative_service(
        alternative_service_info.alternative_service());
    if (alternative_service.host.empty()) {
      alternative_service.host = canonical_server.host();
    }
    if (!IsAlternativeServiceBroken(alternative_service,
                                    network_anonymization_key)) {
      return it;
    }
  }

  RemoveAltSvcCanonicalHost(canonical_server, network_anonymization_key);
  return server_info_map_.end();
}

std::optional<AlternativeService> HttpServerProperties::GetKnownAltSvcHost(
    const url::SchemeHostPort& server) const {
  const char* kKnownAltSvcScheme = url::kHttpsScheme;
  if (server.scheme() != kKnownAltSvcScheme) {
    return std::nullopt;
  }

  auto it = known_alternative_service_map_.find(server);
  if (it != known_alternative_service_map_.end()) {
    return it->second;
  }
  std::string reversed_host = server.host();
  std::ranges::reverse(reversed_host);
  const auto lower_bound_it =
      reversed_known_alternative_service_suffixes_set_.lower_bound(
          reversed_host);
  // Exact matches cannot happen because wildcard suffixes are required to start
  // with "."
  if (lower_bound_it ==
      reversed_known_alternative_service_suffixes_set_.begin()) {
    return std::nullopt;
  }
  // lower_bound_it points to the first element greater or equal to
  // `reversed_host`. The last element that is less than
  // `reversed_host` contains the most likely wildcard suffix match.
  const auto possible_prefix_it = std::prev(lower_bound_it);
  if (!reversed_host.starts_with(*possible_prefix_it)) {
    return std::nullopt;
  }

  url::SchemeHostPort suffix_server(kKnownAltSvcScheme, *possible_prefix_it,
                                    server.port());
  auto suffix_it = wildcard_known_alternative_service_map_.find(suffix_server);
  if (suffix_it != wildcard_known_alternative_service_map_.end()) {
    return suffix_it->second;
  }

  return std::nullopt;
}

HttpServerProperties::CanonicalMap::const_iterator
HttpServerProperties::GetCanonicalAltSvcHost(
    const url::SchemeHostPort& server,
    const NetworkAnonymizationKey& network_anonymization_key) const {
  const char* kCanonicalScheme = "https";
  if (server.scheme() != kCanonicalScheme)
    return canonical_alt_svc_map_.end();

  const std::string* canonical_suffix = GetCanonicalSuffix(server.host());
  if (canonical_suffix == nullptr)
    return canonical_alt_svc_map_.end();

  url::SchemeHostPort canonical_server(kCanonicalScheme, *canonical_suffix,
                                       server.port());
  return canonical_alt_svc_map_.find(
      CreateServerInfoKey(canonical_server, network_anonymization_key));
}

HttpServerProperties::QuicCanonicalMap::const_iterator
HttpServerProperties::GetCanonicalServerInfoHost(
    const QuicServerInfoMapKey& key) const {
  const std::string* canonical_suffix =
      GetCanonicalSuffix(key.server_id.host());
  if (canonical_suffix == nullptr)
    return canonical_server_info_map_.end();

  quic::QuicServerId canonical_server_id(*canonical_suffix,
                                         key.server_id.port());
  return canonical_server_info_map_.find(CreateQuicServerInfoKey(
      canonical_server_id, key.privacy_mode, key.network_anonymization_key));
}

void HttpServerProperties::RemoveAltSvcCanonicalHost(
    const url::SchemeHostPort& server,
    const NetworkAnonymizationKey& network_anonymization_key) {
  auto canonical = GetCanonicalAltSvcHost(server, network_anonymization_key);
  if (canonical == canonical_alt_svc_map_.end())
    return;

  canonical_alt_svc_map_.erase(canonical->first);
}

void HttpServerProperties::UpdateCanonicalServerInfoMap(
    const QuicServerInfoMapKey& key) {
  const std::string* suffix = GetCanonicalSuffix(key.server_id.host());
  if (!suffix)
    return;
  quic::QuicServerId canonical_server(*suffix, key.server_id.port());

  canonical_server_info_map_[CreateQuicServerInfoKey(
      canonical_server, key.privacy_mode, key.network_anonymization_key)] =
      key.server_id;
}

const std::string* HttpServerProperties::GetCanonicalSuffix(
    const std::string& host) const {
  // If this host ends with a canonical suffix, then return the canonical
  // suffix.
  for (const std::string& canonical_suffix : canonical_suffixes_) {
    if (base::EndsWith(host, canonical_suffix,
                       base::CompareCase::INSENSITIVE_ASCII)) {
      return &canonical_suffix;
    }
  }
  return nullptr;
}

void HttpServerProperties::OnPrefsLoaded(
    std::unique_ptr<ServerInfoMap> server_info_map,
    const IPAddress& last_local_address_when_quic_worked,
    std::unique_ptr<QuicServerInfoMap> quic_server_info_map,
    std::unique_ptr<BrokenAlternativeServiceList>
        broken_alternative_service_list,
    std::unique_ptr<RecentlyBrokenAlternativeServices>
        recently_broken_alternative_services) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  DCHECK(!is_initialized_);

  // Either all of these are nullptr, or none of them are (except the broken
  // alt service fields).
  if (server_info_map) {
    OnServerInfoLoaded(std::move(server_info_map));
    OnLastLocalAddressWhenQuicWorkedLoaded(last_local_address_when_quic_worked);
    OnQuicServerInfoMapLoaded(std::move(quic_server_info_map));
    if (recently_broken_alternative_services) {
      DCHECK(broken_alternative_service_list);
      OnBrokenAndRecentlyBrokenAlternativeServicesLoaded(
          std::move(broken_alternative_service_list),
          std::move(recently_broken_alternative_services));
    }
  }

  is_initialized_ = true;

  if (queue_write_on_load_) {
    // Leaving this as true doesn't actually have any effect, but seems best
    // to be safe.
    queue_write_on_load_ = false;
    MaybeQueueWriteProperties();
  }
}

void HttpServerProperties::OnServerInfoLoaded(
    std::unique_ptr<ServerInfoMap> server_info_map) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Perform a simple sanity check on loaded data, when DCHECKs are enabled.
#if DCHECK_IS_ON()
  if (!use_network_anonymization_key_) {
    for (auto server_info = server_info_map->begin();
         server_info != server_info_map->end(); ++server_info) {
      DCHECK(server_info->first.network_anonymization_key.IsEmpty());
    }
  }
#endif  // DCHECK_IS_ON()

  // Swap in the entries from persisted data. This allows the MRU cache to be
  // sorted based on the order of the entries in the newer in-memory cache.
  server_info_map_.Swap(*server_info_map);

  // Add the entries from the memory cache.
  for (auto& [key, server_info] : base::Reversed(*server_info_map)) {
    // If there's no corresponding old entry, add the new entry directly.
    auto old_entry = server_info_map_.Get(key);
    if (old_entry == server_info_map_.end()) {
      server_info_map_.Put(key, std::move(server_info));
      continue;
    }

    // Otherwise, merge the old and new entries. Prefer values from older
    // entries.
    if (!old_entry->second.supports_spdy.has_value())
      old_entry->second.supports_spdy = server_info.supports_spdy;
    if (!old_entry->second.alternative_services.has_value())
      old_entry->second.alternative_services = server_info.alternative_services;
    if (!old_entry->second.server_network_stats.has_value())
      old_entry->second.server_network_stats = server_info.server_network_stats;
  }

  // Attempt to find canonical servers. Canonical suffix only apply to HTTPS.
  const uint16_t kCanonicalPort = 443;
  const char* kCanonicalScheme = "https";
  for (const auto& it : server_info_map_) {
    if (!it.second.alternative_services ||
        it.first.server.scheme() != kCanonicalScheme) {
      continue;
    }
    const std::string* canonical_suffix =
        GetCanonicalSuffix(it.first.server.host());
    if (!canonical_suffix)
      continue;
    ServerInfoMapKey key = CreateServerInfoKey(
        url::SchemeHostPort(kCanonicalScheme, *canonical_suffix,
                            kCanonicalPort),
        it.first.network_anonymization_key);
    // If we already have a valid canonical server, we're done.
    if (base::Contains(canonical_alt_svc_map_, key)) {
      auto key_it = server_info_map_.Peek(key);
      if (key_it != server_info_map_.end() &&
          key_it->second.alternative_services.has_value()) {
        continue;
      }
    }
    canonical_alt_svc_map_[key] = it.first.server;
  }
}

void HttpServerProperties::OnLastLocalAddressWhenQuicWorkedLoaded(
    const IPAddress& last_local_address_when_quic_worked) {
  last_local_address_when_quic_worked_ = last_local_address_when_quic_worked;
}

void HttpServerProperties::OnQuicServerInfoMapLoaded(
    std::unique_ptr<QuicServerInfoMap> quic_server_info_map) {
  DCHECK_EQ(quic_server_info_map->max_size(), quic_server_info_map_.max_size());

  // Add the entries from persisted data.
  quic_server_info_map_.Swap(*quic_server_info_map);

  // Add the entries from the memory cache.
  for (const auto& [key, server_info] : base::Reversed(*quic_server_info_map)) {
    if (quic_server_info_map_.Get(key) == quic_server_info_map_.end()) {
      quic_server_info_map_.Put(key, server_info);
    }
  }

  // Repopulate |canonical_server_info_map_| to stay in sync with
  // |quic_server_info_map_|.
  canonical_server_info_map_.clear();
  for (const auto& [key, server_info] : base::Reversed(quic_server_info_map_)) {
    UpdateCanonicalServerInfoMap(key);
  }
}

void HttpServerProperties::OnBrokenAndRecentlyBrokenAlternativeServicesLoaded(
    std::unique_ptr<BrokenAlternativeServiceList>
        broken_alternative_service_list,
    std::unique_ptr<RecentlyBrokenAlternativeServices>
        recently_broken_alternative_services) {
  broken_alternative_services_.SetBrokenAndRecentlyBrokenAlternativeServices(
      std::move(broken_alternative_service_list),
      std::move(recently_broken_alternative_services));
}

void HttpServerProperties::MaybeQueueWriteProperties() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (prefs_update_timer_.IsRunning() || !properties_manager_)
    return;

  if (!is_initialized_) {
    queue_write_on_load_ = true;
    return;
  }

  prefs_update_timer_.Start(
      FROM_HERE, kUpdatePrefsDelay,
      base::BindOnce(&HttpServerProperties::WriteProperties,
                     base::Unretained(this), base::OnceClosure()));
}

void HttpServerProperties::FlushWritePropertiesForTesting(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!properties_manager_) {
    return;
  }

  // initialising the |properties_manager_| is not a concern here. So skip
  // it and set |is_initalized_| to true.
  is_initialized_ = true;
  // Stop the timer if it's running, since this will write to the properties
  // file immediately.
  prefs_update_timer_.Stop();
  WriteProperties(std::move(callback));
}

void HttpServerProperties::WriteProperties(base::OnceClosure callback) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(properties_manager_);

  // |this| shouldn't be waiting to load properties cached to disk when this
  // method is invoked, since this method will overwrite any cached properties.
  DCHECK(is_initialized_);

  // There shouldn't be a queued update when this is run, since this method
  // removes the need for any update to be queued.
  DCHECK(!prefs_update_timer_.IsRunning());

  properties_manager_->WriteToPrefs(
      server_info_map_,
      base::BindRepeating(&HttpServerProperties::GetCanonicalSuffix,
                          base::Unretained(this)),
      last_local_address_when_quic_worked_, quic_server_info_map_,
      broken_alternative_services_.broken_alternative_service_list(),
      broken_alternative_services_.recently_broken_alternative_services(),
      std::move(callback));
}

}  // namespace net
