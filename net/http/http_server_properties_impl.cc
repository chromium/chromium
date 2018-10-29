// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_server_properties_impl.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "base/values.h"

namespace net {

HttpServerPropertiesImpl::HttpServerPropertiesImpl(
    const base::TickClock* tick_clock,
    base::Clock* clock)
    : tick_clock_(tick_clock ? tick_clock
                             : base::DefaultTickClock::GetInstance()),
      clock_(clock ? clock : base::DefaultClock::GetInstance()),
      broken_alternative_services_(this, tick_clock_),
      quic_server_info_map_(kDefaultMaxQuicServerEntries),
      max_server_configs_stored_in_properties_(kDefaultMaxQuicServerEntries) {
  canonical_suffixes_.push_back(".ggpht.com");
  canonical_suffixes_.push_back(".c.youtube.com");
  canonical_suffixes_.push_back(".googlevideo.com");
  canonical_suffixes_.push_back(".googleusercontent.com");
}

HttpServerPropertiesImpl::HttpServerPropertiesImpl()
    : HttpServerPropertiesImpl(nullptr, nullptr) {}

HttpServerPropertiesImpl::~HttpServerPropertiesImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void HttpServerPropertiesImpl::SetSpdyServers(
    std::unique_ptr<SpdyServersMap> spdy_servers_map) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Add the entries from persisted data.
  spdy_servers_map_.Swap(*spdy_servers_map);

  // Add the entries from the memory cache.
  for (auto it = spdy_servers_map->rbegin(); it != spdy_servers_map->rend();
       ++it) {
    // Add the entry if it is not in the cache, otherwise move it to the front
    // of recency list.
    if (spdy_servers_map_.Get(it->first) == spdy_servers_map_.end())
      spdy_servers_map_.Put(it->first, it->second);
  }
}

void HttpServerPropertiesImpl::SetAlternativeServiceServers(
    std::unique_ptr<AlternativeServiceMap> alternative_service_map) {
  int32_t size_diff =
      alternative_service_map->size() - alternative_service_map_.size();
  if (size_diff > 0) {
    UMA_HISTOGRAM_COUNTS_1M("Net.AlternativeServiceServers.MorePrefsEntries",
                            size_diff);
  } else {
    UMA_HISTOGRAM_COUNTS_1M(
        "Net.AlternativeServiceServers.MoreOrEqualCacheEntries", -size_diff);
  }

  // Add the entries from persisted data.
  alternative_service_map_.Swap(*alternative_service_map);

  // Add the entries from the memory cache.
  for (auto input_it = alternative_service_map->rbegin();
       input_it != alternative_service_map->rend(); ++input_it) {
    if (alternative_service_map_.Get(input_it->first) ==
        alternative_service_map_.end()) {
      alternative_service_map_.Put(input_it->first, input_it->second);
    }
  }

  // Attempt to find canonical servers. Canonical suffix only apply to HTTPS.
  const uint16_t kCanonicalPort = 443;
  const char* kCanonicalScheme = "https";
  for (const std::string& canonical_suffix : canonical_suffixes_) {
    url::SchemeHostPort canonical_server(kCanonicalScheme, canonical_suffix,
                                         kCanonicalPort);
    // If we already have a valid canonical server, we're done.
    if (base::ContainsKey(canonical_alt_svc_map_, canonical_server) &&
        (alternative_service_map_.Peek(
             canonical_alt_svc_map_[canonical_server]) !=
         alternative_service_map_.end())) {
      continue;
    }
    // Now attempt to find a server which matches this origin and set it as
    // canonical.
    for (AlternativeServiceMap::const_iterator it =
             alternative_service_map_.begin();
         it != alternative_service_map_.end(); ++it) {
      if (base::EndsWith(it->first.host(), canonical_suffix,
                         base::CompareCase::INSENSITIVE_ASCII) &&
          it->first.scheme() == canonical_server.scheme()) {
        canonical_alt_svc_map_[canonical_server] = it->first;
        break;
      }
    }
  }
}

void HttpServerPropertiesImpl::SetSupportsQuic(const IPAddress& last_address) {
  last_quic_address_ = last_address;
}

void HttpServerPropertiesImpl::SetServerNetworkStats(
    std::unique_ptr<ServerNetworkStatsMap> server_network_stats_map) {
  // Add the entries from persisted data.
  server_network_stats_map_.Swap(*server_network_stats_map);

  // Add the entries from the memory cache.
  for (auto it = server_network_stats_map->rbegin();
       it != server_network_stats_map->rend(); ++it) {
    if (server_network_stats_map_.Get(it->first) ==
        server_network_stats_map_.end()) {
      server_network_stats_map_.Put(it->first, it->second);
    }
  }
}

void HttpServerPropertiesImpl::SetQuicServerInfoMap(
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

const SpdyServersMap& HttpServerPropertiesImpl::spdy_servers_map() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return spdy_servers_map_;
}

void HttpServerPropertiesImpl::SetBrokenAndRecentlyBrokenAlternativeServices(
    std::unique_ptr<BrokenAlternativeServiceList>
        broken_alternative_service_list,
    std::unique_ptr<RecentlyBrokenAlternativeServices>
        recently_broken_alternative_services) {
  broken_alternative_services_.SetBrokenAndRecentlyBrokenAlternativeServices(
      std::move(broken_alternative_service_list),
      std::move(recently_broken_alternative_services));
}

const BrokenAlternativeServiceList&
HttpServerPropertiesImpl::broken_alternative_service_list() const {
  return broken_alternative_services_.broken_alternative_service_list();
}

const RecentlyBrokenAlternativeServices&
HttpServerPropertiesImpl::recently_broken_alternative_services() const {
  return broken_alternative_services_.recently_broken_alternative_services();
}

void HttpServerPropertiesImpl::Clear(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  spdy_servers_map_.Clear();
  alternative_service_map_.Clear();
  broken_alternative_services_.Clear();
  canonical_alt_svc_map_.clear();
  last_quic_address_ = IPAddress();
  server_network_stats_map_.Clear();
  quic_server_info_map_.Clear();
  canonical_server_info_map_.clear();

  if (!callback.is_null()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
  }
}

bool HttpServerPropertiesImpl::SupportsRequestPriority(
    const url::SchemeHostPort& server) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (server.host().empty())
    return false;

  if (GetSupportsSpdy(server))
    return true;
  const AlternativeServiceInfoVector alternative_service_info_vector =
      GetAlternativeServiceInfos(server);
  for (const AlternativeServiceInfo& alternative_service_info :
       alternative_service_info_vector) {
    if (alternative_service_info.alternative_service().protocol == kProtoQUIC) {
      return true;
    }
  }
  return false;
}

bool HttpServerPropertiesImpl::GetSupportsSpdy(
    const url::SchemeHostPort& server) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (server.host().empty())
    return false;

  auto spdy_server = spdy_servers_map_.Get(server.Serialize());
  return spdy_server != spdy_servers_map_.end() && spdy_server->second;
}

void HttpServerPropertiesImpl::SetSupportsSpdy(
    const url::SchemeHostPort& server,
    bool support_spdy) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (server.host().empty())
    return;

  auto spdy_server = spdy_servers_map_.Get(server.Serialize());
  if ((spdy_server != spdy_servers_map_.end()) &&
      (spdy_server->second == support_spdy)) {
    return;
  }
  // Cache the data.
  spdy_servers_map_.Put(server.Serialize(), support_spdy);
}

bool HttpServerPropertiesImpl::RequiresHTTP11(
    const HostPortPair& host_port_pair) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (host_port_pair.host().empty())
    return false;

  return (http11_servers_.find(host_port_pair) != http11_servers_.end());
}

void HttpServerPropertiesImpl::SetHTTP11Required(
    const HostPortPair& host_port_pair) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (host_port_pair.host().empty())
    return;

  http11_servers_.insert(host_port_pair);
}

void HttpServerPropertiesImpl::MaybeForceHTTP11(const HostPortPair& server,
                                                SSLConfig* ssl_config) {
  if (RequiresHTTP11(server)) {
    ForceHTTP11(ssl_config);
  }
}

const std::string* HttpServerPropertiesImpl::GetCanonicalSuffix(
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

AlternativeServiceInfoVector
HttpServerPropertiesImpl::GetAlternativeServiceInfos(
    const url::SchemeHostPort& origin) {
  // Copy valid alternative service infos into
  // |valid_alternative_service_infos|.
  AlternativeServiceInfoVector valid_alternative_service_infos;
  const base::Time now = clock_->Now();
  auto map_it = alternative_service_map_.Get(origin);
  if (map_it != alternative_service_map_.end()) {
    HostPortPair host_port_pair(origin.host(), origin.port());
    for (auto it = map_it->second.begin(); it != map_it->second.end();) {
      if (it->expiration() < now) {
        it = map_it->second.erase(it);
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
    if (map_it->second.empty()) {
      alternative_service_map_.Erase(map_it);
    }
    return valid_alternative_service_infos;
  }

  auto canonical = GetCanonicalAltSvcHost(origin);
  if (canonical == canonical_alt_svc_map_.end()) {
    return AlternativeServiceInfoVector();
  }
  map_it = alternative_service_map_.Get(canonical->second);
  if (map_it == alternative_service_map_.end()) {
    return AlternativeServiceInfoVector();
  }
  for (auto it = map_it->second.begin(); it != map_it->second.end();) {
    if (it->expiration() < now) {
      it = map_it->second.erase(it);
      continue;
    }
    AlternativeService alternative_service(it->alternative_service());
    if (alternative_service.host.empty()) {
      alternative_service.host = canonical->second.host();
      if (IsAlternativeServiceBroken(alternative_service)) {
        ++it;
        continue;
      }
      alternative_service.host = origin.host();
    } else if (IsAlternativeServiceBroken(alternative_service)) {
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
  if (map_it->second.empty()) {
    alternative_service_map_.Erase(map_it);
  }
  return valid_alternative_service_infos;
}

bool HttpServerPropertiesImpl::SetHttp2AlternativeService(
    const url::SchemeHostPort& origin,
    const AlternativeService& alternative_service,
    base::Time expiration) {
  DCHECK_EQ(alternative_service.protocol, kProtoHTTP2);

  return SetAlternativeServices(
      origin,
      AlternativeServiceInfoVector(
          /*size=*/1, AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
                          alternative_service, expiration)));
}

bool HttpServerPropertiesImpl::SetQuicAlternativeService(
    const url::SchemeHostPort& origin,
    const AlternativeService& alternative_service,
    base::Time expiration,
    const quic::QuicTransportVersionVector& advertised_versions) {
  DCHECK(alternative_service.protocol == kProtoQUIC);

  return SetAlternativeServices(
      origin, AlternativeServiceInfoVector(
                  /*size=*/1,
                  AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
                      alternative_service, expiration, advertised_versions)));
}

bool HttpServerPropertiesImpl::SetAlternativeServices(
    const url::SchemeHostPort& origin,
    const AlternativeServiceInfoVector& alternative_service_info_vector) {
  auto it = alternative_service_map_.Peek(origin);

  if (alternative_service_info_vector.empty()) {
    RemoveAltSvcCanonicalHost(origin);
    if (it == alternative_service_map_.end())
      return false;

    alternative_service_map_.Erase(it);
    return true;
  }

  bool changed = true;
  if (it != alternative_service_map_.end()) {
    DCHECK(!it->second.empty());
    if (it->second.size() == alternative_service_info_vector.size()) {
      const base::Time now = clock_->Now();
      changed = false;
      auto new_it = alternative_service_info_vector.begin();
      for (const auto& old : it->second) {
        // Persist to disk immediately if new entry has different scheme, host,
        // or port.
        if (old.alternative_service() != new_it->alternative_service()) {
          changed = true;
          break;
        }
        // Also persist to disk if new expiration it more that twice as far or
        // less than half as far in the future.
        base::Time old_time = old.expiration();
        base::Time new_time = new_it->expiration();
        if (new_time - now > 2 * (old_time - now) ||
            2 * (new_time - now) < (old_time - now)) {
          changed = true;
          break;
        }
        // Also persist to disk if new entry has a different list of advertised
        // versions.
        if (old.advertised_versions() != new_it->advertised_versions()) {
          changed = true;
          break;
        }
        ++new_it;
      }
    }
  }

  const bool previously_no_alternative_services =
      (GetAlternateProtocolIterator(origin) == alternative_service_map_.end());

  alternative_service_map_.Put(origin, alternative_service_info_vector);

  if (previously_no_alternative_services &&
      !GetAlternativeServiceInfos(origin).empty()) {
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
      canonical_alt_svc_map_[canonical_server] = origin;
    }
  }
  return changed;
}

void HttpServerPropertiesImpl::MarkAlternativeServiceBroken(
    const AlternativeService& alternative_service) {
  broken_alternative_services_.MarkBroken(alternative_service);
}

void HttpServerPropertiesImpl::
    MarkAlternativeServiceBrokenUntilDefaultNetworkChanges(
        const AlternativeService& alternative_service) {
  broken_alternative_services_.MarkBrokenUntilDefaultNetworkChanges(
      alternative_service);
}

void HttpServerPropertiesImpl::MarkAlternativeServiceRecentlyBroken(
    const AlternativeService& alternative_service) {
  broken_alternative_services_.MarkRecentlyBroken(alternative_service);
}

bool HttpServerPropertiesImpl::IsAlternativeServiceBroken(
    const AlternativeService& alternative_service) const {
  return broken_alternative_services_.IsBroken(alternative_service);
}

bool HttpServerPropertiesImpl::WasAlternativeServiceRecentlyBroken(
    const AlternativeService& alternative_service) {
  return broken_alternative_services_.WasRecentlyBroken(alternative_service);
}

void HttpServerPropertiesImpl::ConfirmAlternativeService(
    const AlternativeService& alternative_service) {
  broken_alternative_services_.Confirm(alternative_service);
}

bool HttpServerPropertiesImpl::OnDefaultNetworkChanged() {
  return broken_alternative_services_.OnDefaultNetworkChanged();
}

const AlternativeServiceMap& HttpServerPropertiesImpl::alternative_service_map()
    const {
  return alternative_service_map_;
}

std::unique_ptr<base::Value>
HttpServerPropertiesImpl::GetAlternativeServiceInfoAsValue() const {
  const base::Time now = clock_->Now();
  const base::TimeTicks now_ticks = tick_clock_->NowTicks();
  std::unique_ptr<base::ListValue> dict_list(new base::ListValue);
  for (const auto& alternative_service_map_item : alternative_service_map_) {
    std::unique_ptr<base::ListValue> alternative_service_list(
        new base::ListValue);
    const url::SchemeHostPort& server = alternative_service_map_item.first;
    for (const AlternativeServiceInfo& alternative_service_info :
         alternative_service_map_item.second) {
      std::string alternative_service_string(
          alternative_service_info.ToString());
      AlternativeService alternative_service(
          alternative_service_info.alternative_service());
      if (alternative_service.host.empty()) {
        alternative_service.host = server.host();
      }
      base::TimeTicks brokenness_expiration_ticks;
      if (broken_alternative_services_.IsBroken(alternative_service,
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
    dict->SetString("server", server.Serialize());
    dict->Set("alternative_service", std::unique_ptr<base::Value>(
                                         std::move(alternative_service_list)));
    dict_list->Append(std::move(dict));
  }
  return std::move(dict_list);
}

bool HttpServerPropertiesImpl::GetSupportsQuic(IPAddress* last_address) const {
  if (last_quic_address_.empty())
    return false;

  *last_address = last_quic_address_;
  return true;
}

void HttpServerPropertiesImpl::SetSupportsQuic(bool used_quic,
                                               const IPAddress& address) {
  if (!used_quic) {
    last_quic_address_ = IPAddress();
  } else {
    last_quic_address_ = address;
  }
}

void HttpServerPropertiesImpl::SetServerNetworkStats(
    const url::SchemeHostPort& server,
    ServerNetworkStats stats) {
  server_network_stats_map_.Put(server, stats);
}

void HttpServerPropertiesImpl::ClearServerNetworkStats(
    const url::SchemeHostPort& server) {
  auto it = server_network_stats_map_.Get(server);
  if (it != server_network_stats_map_.end()) {
    server_network_stats_map_.Erase(it);
  }
}

const ServerNetworkStats* HttpServerPropertiesImpl::GetServerNetworkStats(
    const url::SchemeHostPort& server) {
  auto it = server_network_stats_map_.Get(server);
  if (it == server_network_stats_map_.end()) {
    return NULL;
  }
  return &it->second;
}

const ServerNetworkStatsMap&
HttpServerPropertiesImpl::server_network_stats_map() const {
  return server_network_stats_map_;
}

bool HttpServerPropertiesImpl::SetQuicServerInfo(
    const quic::QuicServerId& server_id,
    const std::string& server_info) {
  auto it = quic_server_info_map_.Peek(server_id);
  bool changed =
      (it == quic_server_info_map_.end() || it->second != server_info);
  quic_server_info_map_.Put(server_id, server_info);
  UpdateCanonicalServerInfoMap(server_id);
  return changed;
}

const std::string* HttpServerPropertiesImpl::GetQuicServerInfo(
    const quic::QuicServerId& server_id) {
  auto it = quic_server_info_map_.Get(server_id);
  if (it != quic_server_info_map_.end()) {
    // Since |canonical_server_info_map_| should always map to the most
    // recent host, update it with the one that became MRU in
    // |quic_server_info_map_|.
    UpdateCanonicalServerInfoMap(server_id);
    return &it->second;
  }

  // If the exact match for |server_id| wasn't found, check
  // |canonical_server_info_map_| whether there is server info for a host with
  // the same canonical host suffix.
  auto canonical_itr = GetCanonicalServerInfoHost(server_id);
  if (canonical_itr == canonical_server_info_map_.end())
    return nullptr;

  // When search in |quic_server_info_map_|, do not change the MRU order.
  it = quic_server_info_map_.Peek(canonical_itr->second);
  if (it != quic_server_info_map_.end())
    return &it->second;

  return nullptr;
}

HttpServerPropertiesImpl::CanonicalServerInfoMap::const_iterator
HttpServerPropertiesImpl::GetCanonicalServerInfoHost(
    const quic::QuicServerId& server) const {
  const std::string* canonical_suffix = GetCanonicalSuffix(server.host());
  if (canonical_suffix == nullptr)
    return canonical_server_info_map_.end();

  HostPortPair canonical_pair(*canonical_suffix, server.port());
  return canonical_server_info_map_.find(canonical_pair);
}

const QuicServerInfoMap& HttpServerPropertiesImpl::quic_server_info_map()
    const {
  return quic_server_info_map_;
}

size_t HttpServerPropertiesImpl::max_server_configs_stored_in_properties()
    const {
  return max_server_configs_stored_in_properties_;
}

void HttpServerPropertiesImpl::SetMaxServerConfigsStoredInProperties(
    size_t max_server_configs_stored_in_properties) {
  // Do nothing if the new size is the same as the old one.
  if (max_server_configs_stored_in_properties_ ==
      max_server_configs_stored_in_properties)
    return;

  max_server_configs_stored_in_properties_ =
      max_server_configs_stored_in_properties;

  // MRUCache doesn't allow the capacity of the cache to be changed. Thus create
  // a new map with the new size and add current elements and swap the new map.
  quic_server_info_map_.ShrinkToSize(max_server_configs_stored_in_properties_);
  QuicServerInfoMap temp_map(max_server_configs_stored_in_properties_);
  // Update the |canonical_server_info_map_| as well, so it stays in sync with
  // |quic_server_info_map_|.
  canonical_server_info_map_ = CanonicalServerInfoMap();
  for (auto it = quic_server_info_map_.rbegin();
       it != quic_server_info_map_.rend(); ++it) {
    temp_map.Put(it->first, it->second);
    UpdateCanonicalServerInfoMap(it->first);
  }

  quic_server_info_map_.Swap(temp_map);
}

void HttpServerPropertiesImpl::UpdateCanonicalServerInfoMap(
    const quic::QuicServerId& server) {
  const std::string* suffix = GetCanonicalSuffix(server.host());
  if (suffix) {
    HostPortPair canonical_pair(*suffix, server.port());
    canonical_server_info_map_[canonical_pair] = server;
  }
}

bool HttpServerPropertiesImpl::IsInitialized() const {
  // No initialization is needed.
  return true;
}

AlternativeServiceMap::const_iterator
HttpServerPropertiesImpl::GetAlternateProtocolIterator(
    const url::SchemeHostPort& server) {
  AlternativeServiceMap::const_iterator it =
      alternative_service_map_.Get(server);
  if (it != alternative_service_map_.end())
    return it;

  auto canonical = GetCanonicalAltSvcHost(server);
  if (canonical == canonical_alt_svc_map_.end()) {
    return alternative_service_map_.end();
  }

  const url::SchemeHostPort canonical_server = canonical->second;
  it = alternative_service_map_.Get(canonical_server);
  if (it == alternative_service_map_.end()) {
    return alternative_service_map_.end();
  }

  for (const AlternativeServiceInfo& alternative_service_info : it->second) {
    AlternativeService alternative_service(
        alternative_service_info.alternative_service());
    if (alternative_service.host.empty()) {
      alternative_service.host = canonical_server.host();
    }
    if (!IsAlternativeServiceBroken(alternative_service)) {
      return it;
    }
  }

  RemoveAltSvcCanonicalHost(canonical_server);
  return alternative_service_map_.end();
}

HttpServerPropertiesImpl::CanonicalAltSvcMap::const_iterator
HttpServerPropertiesImpl::GetCanonicalAltSvcHost(
    const url::SchemeHostPort& server) const {
  const char* kCanonicalScheme = "https";
  if (server.scheme() != kCanonicalScheme)
    return canonical_alt_svc_map_.end();

  const std::string* canonical_suffix = GetCanonicalSuffix(server.host());
  if (canonical_suffix == nullptr)
    return canonical_alt_svc_map_.end();

  url::SchemeHostPort canonical_server(kCanonicalScheme, *canonical_suffix,
                                       server.port());
  return canonical_alt_svc_map_.find(canonical_server);
}

void HttpServerPropertiesImpl::RemoveAltSvcCanonicalHost(
    const url::SchemeHostPort& server) {
  auto canonical = GetCanonicalAltSvcHost(server);
  if (canonical == canonical_alt_svc_map_.end())
    return;

  canonical_alt_svc_map_.erase(canonical->first);
}

void HttpServerPropertiesImpl::OnExpireBrokenAlternativeService(
    const AlternativeService& expired_alternative_service) {
  // Remove every occurrence of |expired_alternative_service| from
  // |alternative_service_map_|.
  for (auto map_it = alternative_service_map_.begin();
       map_it != alternative_service_map_.end();) {
    for (auto it = map_it->second.begin(); it != map_it->second.end();) {
      AlternativeService alternative_service(it->alternative_service());
      // Empty hostname in map means hostname of key: substitute before
      // comparing to |expired_alternative_service|.
      if (alternative_service.host.empty()) {
        alternative_service.host = map_it->first.host();
      }
      if (alternative_service == expired_alternative_service) {
        it = map_it->second.erase(it);
        continue;
      }
      ++it;
    }
    // If an origin has an empty list of alternative services, then remove it
    // from both |canonical_alt_svc_map_| and
    // |alternative_service_map_|.
    if (map_it->second.empty()) {
      RemoveAltSvcCanonicalHost(map_it->first);
      map_it = alternative_service_map_.Erase(map_it);
      continue;
    }
    ++map_it;
  }
}

}  // namespace net
