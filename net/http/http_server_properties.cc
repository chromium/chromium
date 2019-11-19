// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_server_properties.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/values.h"
#include "net/base/features.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_manager.h"
#include "net/socket/ssl_client_socket.h"
#include "net/ssl/ssl_config.h"

namespace net {

namespace {

// Time to wait before starting an update the preferences from the
// http_server_properties_impl_ cache. Scheduling another update during this
// period will be a no-op.
constexpr base::TimeDelta kUpdatePrefsDelay = base::TimeDelta::FromSeconds(60);

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
    const NetworkIsolationKey& network_isolation_key,
    bool use_network_isolation_key)
    : server(std::move(server)),
      network_isolation_key(use_network_isolation_key ? network_isolation_key
                                                      : NetworkIsolationKey()) {
  // Scheme should have been normalized before this method was called.
  DCHECK_NE(this->server.scheme(), url::kWsScheme);
  DCHECK_NE(this->server.scheme(), url::kWssScheme);
}

HttpServerProperties::ServerInfoMapKey::~ServerInfoMapKey() = default;

bool HttpServerProperties::ServerInfoMapKey::operator<(
    const ServerInfoMapKey& other) const {
  return std::tie(server, network_isolation_key) <
         std::tie(other.server, other.network_isolation_key);
}

HttpServerProperties::QuicServerInfoMapKey::QuicServerInfoMapKey(
    const quic::QuicServerId& server_id,
    const NetworkIsolationKey& network_isolation_key,
    bool use_network_isolation_key)
    : server_id(server_id),
      network_isolation_key(use_network_isolation_key ? network_isolation_key
                                                      : NetworkIsolationKey()) {
}

HttpServerProperties::QuicServerInfoMapKey::~QuicServerInfoMapKey() = default;

bool HttpServerProperties::QuicServerInfoMapKey::operator<(
    const QuicServerInfoMapKey& other) const {
  return std::tie(server_id, network_isolation_key) <
         std::tie(other.server_id, other.network_isolation_key);
}

// Used in tests.
bool HttpServerProperties::QuicServerInfoMapKey::operator==(
    const QuicServerInfoMapKey& other) const {
  return std::tie(server_id, network_isolation_key) ==
         std::tie(other.server_id, other.network_isolation_key);
}

HttpServerProperties::ServerInfoMap::ServerInfoMap()
    : base::MRUCache<ServerInfoMapKey, ServerInfo>(kMaxServerInfoEntries) {}

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
      use_network_isolation_key_(base::FeatureList::IsEnabled(
          features::kPartitionHttpServerPropertiesByNetworkIsolationKey)),
      is_initialized_(pref_delegate.get() == nullptr),
      queue_write_on_load_(false),
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
                           ".googleusercontent.com"}),
      quic_server_info_map_(kDefaultMaxQuicServerEntries),
      max_server_configs_stored_in_properties_(kDefaultMaxQuicServerEntries) {}

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
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
  }
}

bool HttpServerProperties::SupportsRequestPriority(
    const url::SchemeHostPort& server,
    const net::NetworkIsolationKey& network_isolation_key) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (server.host().empty())
    return false;

  if (GetSupportsSpdy(server, network_isolation_key))
    return true;
  const AlternativeServiceInfoVector alternative_service_info_vector =
      GetAlternativeServiceInfos(server, network_isolation_key);
  for (const AlternativeServiceInfo& alternative_service_info :
       alternative_service_info_vector) {
    if (alternative_service_info.alternative_service().protocol == kProtoQUIC) {
      return true;
    }
  }
  return false;
}

bool HttpServerProperties::GetSupportsSpdy(
    const url::SchemeHostPort& server,
    const net::NetworkIsolationKey& network_isolation_key) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return GetSupportsSpdyInternal(NormalizeSchemeHostPort(server),
                                 network_isolation_key);
}

void HttpServerProperties::SetSupportsSpdy(
    const url::SchemeHostPort& server,
    const net::NetworkIsolationKey& network_isolation_key,
    bool supports_spdy) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SetSupportsSpdyInternal(NormalizeSchemeHostPort(server),
                          network_isolation_key, supports_spdy);
}

bool HttpServerProperties::RequiresHTTP11(
    const url::SchemeHostPort& server,
    const net::NetworkIsolationKey& network_isolation_key) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return RequiresHTTP11Internal(NormalizeSchemeHostPort(server),
                                network_isolation_key);
}

void HttpServerProperties::SetHTTP11Required(
    const url::SchemeHostPort& server,
    const net::NetworkIsolationKey& network_isolation_key) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SetHTTP11RequiredInternal(NormalizeSchemeHostPort(server),
                            network_isolation_key);
}

void HttpServerProperties::MaybeForceHTTP11(
    const url::SchemeHostPort& server,
    const net::NetworkIsolationKey& network_isolation_key,
    SSLConfig* ssl_config) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  MaybeForceHTTP11Internal(NormalizeSchemeHostPort(server),
                           network_isolation_key, ssl_config);
}

AlternativeServiceInfoVector HttpServerProperties::GetAlternativeServiceInfos(
    const url::SchemeHostPort& origin,
    const net::NetworkIsolationKey& network_isolation_key) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return GetAlternativeServiceInfosInternal(NormalizeSchemeHostPort(origin),
                                            network_isolation_key);
}

void HttpServerProperties::SetHttp2AlternativeService(
    const url::SchemeHostPort& origin,
    const NetworkIsolationKey& network_isolation_key,
    const AlternativeService& alternative_service,
    base::Time expiration) {
  DCHECK_EQ(alternative_service.protocol, kProtoHTTP2);

  SetAlternativeServices(
      origin, network_isolation_key,
      AlternativeServiceInfoVector(
          /*size=*/1, AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
                          alternative_service, expiration)));
}

void HttpServerProperties::SetQuicAlternativeService(
    const url::SchemeHostPort& origin,
    const NetworkIsolationKey& network_isolation_key,
    const AlternativeService& alternative_service,
    base::Time expiration,
    const quic::ParsedQuicVersionVector& advertised_versions) {
  DCHECK(alternative_service.protocol == kProtoQUIC);

  SetAlternativeServices(
      origin, network_isolation_key,
      AlternativeServiceInfoVector(
          /*size=*/1,
          AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
              alternative_service, expiration, advertised_versions)));
}

void HttpServerProperties::SetAlternativeServices(
    const url::SchemeHostPort& origin,
    const net::NetworkIsolationKey& network_isolation_key,
    const AlternativeServiceInfoVector& alternative_service_info_vector) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SetAlternativeServicesInternal(NormalizeSchemeHostPort(origin),
                                 network_isolation_key,
                                 alternative_service_info_vector);
}

void HttpServerProperties::MarkAlternativeServiceBroken(
    const AlternativeService& alternative_service,
    const net::NetworkIsolationKey& network_isolation_key) {
  broken_alternative_services_.MarkBroken(BrokenAlternativeService(
      alternative_service, network_isolation_key, use_network_isolation_key_));
  MaybeQueueWriteProperties();
}

void HttpServerProperties::
    MarkAlternativeServiceBrokenUntilDefaultNetworkChanges(
        const AlternativeService& alternative_service,
        const net::NetworkIsolationKey& network_isolation_key) {
  broken_alternative_services_.MarkBrokenUntilDefaultNetworkChanges(
      BrokenAlternativeService(alternative_service, network_isolation_key,
                               use_network_isolation_key_));
  MaybeQueueWriteProperties();
}

void HttpServerProperties::MarkAlternativeServiceRecentlyBroken(
    const AlternativeService& alternative_service,
    const net::NetworkIsolationKey& network_isolation_key) {
  broken_alternative_services_.MarkRecentlyBroken(BrokenAlternativeService(
      alternative_service, network_isolation_key, use_network_isolation_key_));
  MaybeQueueWriteProperties();
}

bool HttpServerProperties::IsAlternativeServiceBroken(
    const AlternativeService& alternative_service,
    const net::NetworkIsolationKey& network_isolation_key) const {
  return broken_alternative_services_.IsBroken(BrokenAlternativeService(
      alternative_service, network_isolation_key, use_network_isolation_key_));
}

bool HttpServerProperties::WasAlternativeServiceRecentlyBroken(
    const AlternativeService& alternative_service,
    const net::NetworkIsolationKey& network_isolation_key) {
  return broken_alternative_services_.WasRecentlyBroken(
      BrokenAlternativeService(alternative_service, network_isolation_key,
                               use_network_isolation_key_));
}

void HttpServerProperties::ConfirmAlternativeService(
    const AlternativeService& alternative_service,
    const net::NetworkIsolationKey& network_isolation_key) {
  bool old_value =
      IsAlternativeServiceBroken(alternative_service, network_isolation_key);
  broken_alternative_services_.Confirm(BrokenAlternativeService(
      alternative_service, network_isolation_key, use_network_isolation_key_));
  bool new_value =
      IsAlternativeServiceBroken(alternative_service, network_isolation_key);

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

std::unique_ptr<base::Value>
HttpServerProperties::GetAlternativeServiceInfoAsValue() const {
  const base::Time now = clock_->Now();
  const base::TimeTicks now_ticks = tick_clock_->NowTicks();
  std::unique_ptr<base::ListValue> dict_list(new base::ListValue);
  for (const auto& server_info : server_info_map_) {
    if (!server_info.second.alternative_services.has_value())
      continue;
    std::unique_ptr<base::ListValue> alternative_service_list(
        new base::ListValue);
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
              BrokenAlternativeService(alternative_service,
                                       server_info.first.network_isolation_key,
                                       use_network_isolation_key_),
              &brokenness_expiration_ticks)) {
        // Convert |brokenness_expiration| from TimeTicks to Time
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
      alternative_service_list->AppendString(alternative_service_string);
    }
    if (alternative_service_list->empty())
      continue;
    std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
    dict->SetString("server", key.server.Serialize());
    dict->SetString("network_isolation_key",
                    key.network_isolation_key.ToDebugString());
    dict->Set("alternative_service", std::unique_ptr<base::Value>(
                                         std::move(alternative_service_list)));
    dict_list->Append(std::move(dict));
  }
  return std::move(dict_list);
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
    const NetworkIsolationKey& network_isolation_key,
    ServerNetworkStats stats) {
  SetServerNetworkStatsInternal(NormalizeSchemeHostPort(server),
                                network_isolation_key, std::move(stats));
}

void HttpServerProperties::ClearServerNetworkStats(
    const url::SchemeHostPort& server,
    const NetworkIsolationKey& network_isolation_key) {
  ClearServerNetworkStatsInternal(NormalizeSchemeHostPort(server),
                                  network_isolation_key);
}

const ServerNetworkStats* HttpServerProperties::GetServerNetworkStats(
    const url::SchemeHostPort& server,
    const NetworkIsolationKey& network_isolation_key) {
  return GetServerNetworkStatsInternal(NormalizeSchemeHostPort(server),
                                       network_isolation_key);
}

void HttpServerProperties::SetQuicServerInfo(
    const quic::QuicServerId& server_id,
    const NetworkIsolationKey& network_isolation_key,
    const std::string& server_info) {
  QuicServerInfoMapKey key =
      CreateQuicServerInfoKey(server_id, network_isolation_key);
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
    const NetworkIsolationKey& network_isolation_key) {
  QuicServerInfoMapKey key =
      CreateQuicServerInfoKey(server_id, network_isolation_key);
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
  it = quic_server_info_map_.Peek(
      CreateQuicServerInfoKey(canonical_itr->second, network_isolation_key));
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

  // MRUCache doesn't allow the capacity of the cache to be changed. Thus create
  // a new map with the new size and add current elements and swap the new map.
  quic_server_info_map_.ShrinkToSize(max_server_configs_stored_in_properties_);
  QuicServerInfoMap temp_map(max_server_configs_stored_in_properties_);
  // Update the |canonical_server_info_map_| as well, so it stays in sync with
  // |quic_server_info_map_|.
  canonical_server_info_map_ = QuicCanonicalMap();
  for (auto it = quic_server_info_map_.rbegin();
       it != quic_server_info_map_.rend(); ++it) {
    temp_map.Put(it->first, it->second);
    UpdateCanonicalServerInfoMap(it->first);
  }

  quic_server_info_map_.Swap(temp_map);
  if (properties_manager_) {
    properties_manager_->set_max_server_configs_stored_in_properties(
        max_server_configs_stored_in_properties);
  }
}

bool HttpServerProperties::IsInitialized() const {
  return is_initialized_;
}

void HttpServerProperties::OnExpireBrokenAlternativeService(
    const AlternativeService& expired_alternative_service,
    const NetworkIsolationKey& network_isolation_key) {
  // Remove every occurrence of |expired_alternative_service| from
  // |alternative_service_map_|.
  for (auto map_it = server_info_map_.begin();
       map_it != server_info_map_.end();) {
    if (!map_it->second.alternative_services.has_value() ||
        map_it->first.network_isolation_key != network_isolation_key) {
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
      RemoveAltSvcCanonicalHost(map_it->first.server, network_isolation_key);
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

bool HttpServerProperties::GetSupportsSpdyInternal(
    url::SchemeHostPort server,
    const net::NetworkIsolationKey& network_isolation_key) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(server.scheme(), url::kWsScheme);
  DCHECK_NE(server.scheme(), url::kWssScheme);
  if (server.host().empty())
    return false;

  auto server_info = server_info_map_.Get(
      CreateServerInfoKey(std::move(server), network_isolation_key));
  return server_info != server_info_map_.end() &&
         server_info->second.supports_spdy.value_or(false);
}

void HttpServerProperties::SetSupportsSpdyInternal(
    url::SchemeHostPort server,
    const net::NetworkIsolationKey& network_isolation_key,
    bool supports_spdy) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(server.scheme(), url::kWsScheme);
  DCHECK_NE(server.scheme(), url::kWssScheme);
  if (server.host().empty())
    return;

  auto server_info = server_info_map_.GetOrPut(
      CreateServerInfoKey(std::move(server), network_isolation_key));
  // If value is already the same as |supports_spdy|, or value is unset and
  // |supports_spdy| is false, don't queue a write.
  bool queue_write =
      server_info->second.supports_spdy.value_or(false) != supports_spdy;
  server_info->second.supports_spdy = supports_spdy;

  if (queue_write)
    MaybeQueueWriteProperties();
}

bool HttpServerProperties::RequiresHTTP11Internal(
    url::SchemeHostPort server,
    const net::NetworkIsolationKey& network_isolation_key) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(server.scheme(), url::kWsScheme);
  DCHECK_NE(server.scheme(), url::kWssScheme);
  if (server.host().empty())
    return false;

  auto spdy_info = server_info_map_.Get(
      CreateServerInfoKey(std::move(server), network_isolation_key));
  return spdy_info != server_info_map_.end() &&
         spdy_info->second.requires_http11.value_or(false);
}

void HttpServerProperties::SetHTTP11RequiredInternal(
    url::SchemeHostPort server,
    const net::NetworkIsolationKey& network_isolation_key) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(server.scheme(), url::kWsScheme);
  DCHECK_NE(server.scheme(), url::kWssScheme);
  if (server.host().empty())
    return;

  server_info_map_
      .GetOrPut(CreateServerInfoKey(std::move(server), network_isolation_key))
      ->second.requires_http11 = true;
  // No need to call MaybeQueueWriteProperties(), as this information is not
  // persisted to preferences.
}

void HttpServerProperties::MaybeForceHTTP11Internal(
    url::SchemeHostPort server,
    const net::NetworkIsolationKey& network_isolation_key,
    SSLConfig* ssl_config) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(server.scheme(), url::kWsScheme);
  DCHECK_NE(server.scheme(), url::kWssScheme);
  if (RequiresHTTP11(std::move(server), network_isolation_key)) {
    ssl_config->alpn_protos.clear();
    ssl_config->alpn_protos.push_back(kProtoHTTP11);
  }
}

AlternativeServiceInfoVector
HttpServerProperties::GetAlternativeServiceInfosInternal(
    const url::SchemeHostPort& origin,
    const net::NetworkIsolationKey& network_isolation_key) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(origin.scheme(), url::kWsScheme);
  DCHECK_NE(origin.scheme(), url::kWssScheme);

  // Copy valid alternative service infos into
  // |valid_alternative_service_infos|.
  AlternativeServiceInfoVector valid_alternative_service_infos;
  const base::Time now = clock_->Now();
  auto map_it =
      server_info_map_.Get(CreateServerInfoKey(origin, network_isolation_key));
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
      // If the alternative service is equivalent to the origin (same host, same
      // port, and both TCP), skip it.
      if (host_port_pair.Equals(alternative_service.host_port_pair()) &&
          alternative_service.protocol == kProtoHTTP2) {
        ++it;
        continue;
      }
      if (alternative_service.protocol == kProtoQUIC) {
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

  auto canonical = GetCanonicalAltSvcHost(origin, network_isolation_key);
  if (canonical == canonical_alt_svc_map_.end()) {
    return AlternativeServiceInfoVector();
  }
  map_it = server_info_map_.Get(
      CreateServerInfoKey(canonical->second, network_isolation_key));
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
                                     network_isolation_key)) {
        ++it;
        continue;
      }
      alternative_service.host = origin.host();
    } else if (IsAlternativeServiceBroken(alternative_service,
                                          network_isolation_key)) {
      ++it;
      continue;
    }
    if (alternative_service.protocol == kProtoQUIC) {
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
    const net::NetworkIsolationKey& network_isolation_key,
    const AlternativeServiceInfoVector& alternative_service_info_vector) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(origin.scheme(), url::kWsScheme);
  DCHECK_NE(origin.scheme(), url::kWssScheme);

  if (alternative_service_info_vector.empty()) {
    RemoveAltSvcCanonicalHost(origin, network_isolation_key);
    // Don't bother moving to front when erasing information.
    auto it = server_info_map_.Peek(
        CreateServerInfoKey(origin, network_isolation_key));

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
      CreateServerInfoKey(origin, network_isolation_key));
  bool need_update_pref = true;
  if (it->second.alternative_services.has_value()) {
    DCHECK(!it->second.empty());
    if (it->second.alternative_services->size() ==
        alternative_service_info_vector.size()) {
      const base::Time now = clock_->Now();
      need_update_pref = false;
      auto new_it = alternative_service_info_vector.begin();
      for (const auto& old : *it->second.alternative_services) {
        // Persist to disk immediately if new entry has different scheme, host,
        // or port.
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
        // Also persist to disk if new entry has a different list of advertised
        // versions.
        if (old.advertised_versions() != new_it->advertised_versions()) {
          need_update_pref = true;
          break;
        }
        ++new_it;
      }
    }
  }

  const bool previously_no_alternative_services =
      (GetIteratorWithAlternativeServiceInfo(origin, network_isolation_key) ==
       server_info_map_.end());

  it->second.alternative_services = alternative_service_info_vector;

  if (previously_no_alternative_services &&
      !GetAlternativeServiceInfos(origin, network_isolation_key).empty()) {
    // TODO(rch): Consider the case where multiple requests are started
    // before the first completes. In this case, only one of the jobs
    // would reach this code, whereas all of them should should have.
    HistogramAlternateProtocolUsage(ALTERNATE_PROTOCOL_USAGE_MAPPING_MISSING,
                                    false);
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
          canonical_server, network_isolation_key)] = origin;
    }
  }

  if (need_update_pref)
    MaybeQueueWriteProperties();
}

void HttpServerProperties::SetServerNetworkStatsInternal(
    url::SchemeHostPort server,
    const NetworkIsolationKey& network_isolation_key,
    ServerNetworkStats stats) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(server.scheme(), url::kWsScheme);
  DCHECK_NE(server.scheme(), url::kWssScheme);

  auto server_info = server_info_map_.GetOrPut(
      CreateServerInfoKey(std::move(server), network_isolation_key));
  bool changed = !server_info->second.server_network_stats.has_value() ||
                 server_info->second.server_network_stats.value() != stats;

  if (changed) {
    server_info->second.server_network_stats = stats;
    MaybeQueueWriteProperties();
  }
}

void HttpServerProperties::ClearServerNetworkStatsInternal(
    url::SchemeHostPort server,
    const NetworkIsolationKey& network_isolation_key) {
  auto server_info = server_info_map_.Peek(
      CreateServerInfoKey(std::move(server), network_isolation_key));
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
    const NetworkIsolationKey& network_isolation_key) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(server.scheme(), url::kWsScheme);
  DCHECK_NE(server.scheme(), url::kWssScheme);

  auto server_info = server_info_map_.Get(
      CreateServerInfoKey(std::move(server), network_isolation_key));
  if (server_info == server_info_map_.end() ||
      !server_info->second.server_network_stats.has_value()) {
    return nullptr;
  }
  return &server_info->second.server_network_stats.value();
}

HttpServerProperties::QuicServerInfoMapKey
HttpServerProperties::CreateQuicServerInfoKey(
    const quic::QuicServerId& server_id,
    const NetworkIsolationKey& network_isolation_key) const {
  return QuicServerInfoMapKey(server_id, network_isolation_key,
                              use_network_isolation_key_);
}

HttpServerProperties::ServerInfoMapKey
HttpServerProperties::CreateServerInfoKey(
    const url::SchemeHostPort& server,
    const NetworkIsolationKey& network_isolation_key) const {
  return ServerInfoMapKey(server, network_isolation_key,
                          use_network_isolation_key_);
}

HttpServerProperties::ServerInfoMap::const_iterator
HttpServerProperties::GetIteratorWithAlternativeServiceInfo(
    const url::SchemeHostPort& server,
    const net::NetworkIsolationKey& network_isolation_key) {
  ServerInfoMap::const_iterator it =
      server_info_map_.Get(CreateServerInfoKey(server, network_isolation_key));
  if (it != server_info_map_.end() && it->second.alternative_services)
    return it;

  auto canonical = GetCanonicalAltSvcHost(server, network_isolation_key);
  if (canonical == canonical_alt_svc_map_.end()) {
    return server_info_map_.end();
  }

  const url::SchemeHostPort canonical_server = canonical->second;
  it = server_info_map_.Get(
      CreateServerInfoKey(canonical_server, network_isolation_key));
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
                                    network_isolation_key)) {
      return it;
    }
  }

  RemoveAltSvcCanonicalHost(canonical_server, network_isolation_key);
  return server_info_map_.end();
}

HttpServerProperties::CanonicalMap::const_iterator
HttpServerProperties::GetCanonicalAltSvcHost(
    const url::SchemeHostPort& server,
    const net::NetworkIsolationKey& network_isolation_key) const {
  const char* kCanonicalScheme = "https";
  if (server.scheme() != kCanonicalScheme)
    return canonical_alt_svc_map_.end();

  const std::string* canonical_suffix = GetCanonicalSuffix(server.host());
  if (canonical_suffix == nullptr)
    return canonical_alt_svc_map_.end();

  url::SchemeHostPort canonical_server(kCanonicalScheme, *canonical_suffix,
                                       server.port());
  return canonical_alt_svc_map_.find(
      CreateServerInfoKey(canonical_server, network_isolation_key));
}

HttpServerProperties::QuicCanonicalMap::const_iterator
HttpServerProperties::GetCanonicalServerInfoHost(
    const QuicServerInfoMapKey& key) const {
  const std::string* canonical_suffix =
      GetCanonicalSuffix(key.server_id.host());
  if (canonical_suffix == nullptr)
    return canonical_server_info_map_.end();

  quic::QuicServerId canonical_server_id(*canonical_suffix,
                                         key.server_id.privacy_mode_enabled(),
                                         key.server_id.port());
  return canonical_server_info_map_.find(
      CreateQuicServerInfoKey(canonical_server_id, key.network_isolation_key));
}

void HttpServerProperties::RemoveAltSvcCanonicalHost(
    const url::SchemeHostPort& server,
    const NetworkIsolationKey& network_isolation_key) {
  auto canonical = GetCanonicalAltSvcHost(server, network_isolation_key);
  if (canonical == canonical_alt_svc_map_.end())
    return;

  canonical_alt_svc_map_.erase(canonical->first);
}

void HttpServerProperties::UpdateCanonicalServerInfoMap(
    const QuicServerInfoMapKey& key) {
  const std::string* suffix = GetCanonicalSuffix(key.server_id.host());
  if (!suffix)
    return;
  quic::QuicServerId canonical_server(
      *suffix, key.server_id.privacy_mode_enabled(), key.server_id.port());

  canonical_server_info_map_[CreateQuicServerInfoKey(
      canonical_server, key.network_isolation_key)] = key.server_id;
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

  // Either all of these are nullptr, or none of them are (except the broken alt
  // service fields).
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
    // Leaving this as true doesn't actually have any effect, but seems best to
    // be safe.
    queue_write_on_load_ = false;
    MaybeQueueWriteProperties();
  }
}

void HttpServerProperties::OnServerInfoLoaded(
    std::unique_ptr<ServerInfoMap> server_info_map) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Perform a simple sanity check on loaded data, when DCHECKs are enabled.
#if DCHECK_IS_ON()
  if (!use_network_isolation_key_) {
    for (auto server_info = server_info_map->begin();
         server_info != server_info_map->end(); ++server_info) {
      DCHECK(server_info->first.network_isolation_key.IsEmpty());
    }
  }
#endif  // DCHECK_IS_ON()

  // Swap in the entries from persisted data. This allows the MRU cache to be
  // sorted based on the order of the entries in the newer in-memory cache.
  server_info_map_.Swap(*server_info_map);

  // Add the entries from the memory cache.
  for (auto it = server_info_map->rbegin(); it != server_info_map->rend();
       ++it) {
    // If there's no corresponding old entry, add the new entry directly.
    auto old_entry = server_info_map_.Get(it->first);
    if (old_entry == server_info_map_.end()) {
      server_info_map_.Put(it->first, std::move(it->second));
      continue;
    }

    // Otherwise, merge the old and new entries. Prefer values from older
    // entries.
    if (!old_entry->second.supports_spdy.has_value())
      old_entry->second.supports_spdy = it->second.supports_spdy;
    if (!old_entry->second.alternative_services.has_value())
      old_entry->second.alternative_services = it->second.alternative_services;
    if (!old_entry->second.server_network_stats.has_value())
      old_entry->second.server_network_stats = it->second.server_network_stats;

    // |requires_http11| isn't saved to prefs, so the loaded entry should not
    // have it set. Unconditionally copy it from the new entry.
    DCHECK(!old_entry->second.requires_http11.has_value());
    old_entry->second.requires_http11 = it->second.requires_http11;
  }

  // Attempt to find canonical servers. Canonical suffix only apply to HTTPS.
  const uint16_t kCanonicalPort = 443;
  const char* kCanonicalScheme = "https";
  for (auto it = server_info_map_.begin(); it != server_info_map_.end(); ++it) {
    if (!it->second.alternative_services ||
        it->first.server.scheme() != kCanonicalScheme) {
      continue;
    }
    const std::string* canonical_suffix =
        GetCanonicalSuffix(it->first.server.host());
    if (!canonical_suffix)
      continue;
    ServerInfoMapKey key = CreateServerInfoKey(
        url::SchemeHostPort(kCanonicalScheme, *canonical_suffix,
                            kCanonicalPort),
        it->first.network_isolation_key);
    // If we already have a valid canonical server, we're done.
    if (base::Contains(canonical_alt_svc_map_, key)) {
      auto it = server_info_map_.Peek(key);
      if (it != server_info_map_.end() &&
          it->second.alternative_services.has_value()) {
        continue;
      }
    }
    canonical_alt_svc_map_[key] = it->first.server;
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
  for (auto it = quic_server_info_map->rbegin();
       it != quic_server_info_map->rend(); ++it) {
    if (quic_server_info_map_.Get(it->first) == quic_server_info_map_.end()) {
      quic_server_info_map_.Put(it->first, it->second);
    }
  }

  // Repopulate |canonical_server_info_map_| to stay in sync with
  // |quic_server_info_map_|.
  canonical_server_info_map_.clear();
  for (auto it = quic_server_info_map_.rbegin();
       it != quic_server_info_map_.rend(); ++it) {
    UpdateCanonicalServerInfoMap(it->first);
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
