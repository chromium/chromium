// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_client_session_cache.h"

#include <tuple>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/trace_event/process_memory_dump.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace net {

namespace {

bool IsTLS13(const SSL_SESSION* session) {
  return SSL_SESSION_get_protocol_version(session) >= TLS1_3_VERSION;
}

}  // namespace

SSLClientSessionCache::Key::Key() = default;
SSLClientSessionCache::Key::Key(const Key& other) = default;
SSLClientSessionCache::Key::Key(Key&& other) = default;
SSLClientSessionCache::Key::~Key() = default;
SSLClientSessionCache::Key& SSLClientSessionCache::Key::operator=(
    const Key& other) = default;
SSLClientSessionCache::Key& SSLClientSessionCache::Key::operator=(Key&& other) =
    default;

bool SSLClientSessionCache::Key::operator==(const Key& other) const {
  return std::tie(server, dest_ip_addr, network_isolation_key, privacy_mode) ==
         std::tie(other.server, other.dest_ip_addr, other.network_isolation_key,
                  other.privacy_mode);
}

bool SSLClientSessionCache::Key::operator<(const Key& other) const {
  return std::tie(server, dest_ip_addr, network_isolation_key, privacy_mode) <
         std::tie(other.server, other.dest_ip_addr, other.network_isolation_key,
                  other.privacy_mode);
}

SSLClientSessionCache::SSLClientSessionCache(const Config& config)
    : clock_(base::DefaultClock::GetInstance()),
      config_(config),
      cache_(config.max_entries),
      lookups_since_flush_(0) {
  memory_pressure_listener_.reset(new base::MemoryPressureListener(base::Bind(
      &SSLClientSessionCache::OnMemoryPressure, base::Unretained(this))));
}

SSLClientSessionCache::~SSLClientSessionCache() {
  Flush();
}

size_t SSLClientSessionCache::size() const {
  return cache_.size();
}

bssl::UniquePtr<SSL_SESSION> SSLClientSessionCache::Lookup(
    const Key& cache_key) {
  // Expire stale sessions.
  lookups_since_flush_++;
  if (lookups_since_flush_ >= config_.expiration_check_count) {
    lookups_since_flush_ = 0;
    FlushExpiredSessions();
  }

  auto iter = cache_.Get(cache_key);
  if (iter == cache_.end())
    return nullptr;

  time_t now = clock_->Now().ToTimeT();
  bssl::UniquePtr<SSL_SESSION> session = iter->second.Pop();
  if (iter->second.ExpireSessions(now))
    cache_.Erase(iter);

  if (IsExpired(session.get(), now))
    session = nullptr;

  if (session != nullptr && IsTLS13(session.get())) {
    base::Time session_created =
        base::Time::FromTimeT(SSL_SESSION_get_time(session.get()));
    base::TimeDelta time_to_use = clock_->Now() - session_created;
    UMA_HISTOGRAM_CUSTOM_TIMES("Net.SSLTLS13SessionTimeToUse", time_to_use,
                               base::TimeDelta::FromMinutes(1),
                               base::TimeDelta::FromDays(7), 50);
  }
  return session;
}

void SSLClientSessionCache::Insert(const Key& cache_key,
                                   bssl::UniquePtr<SSL_SESSION> session) {
  if (IsTLS13(session.get())) {
    base::TimeDelta lifetime =
        base::TimeDelta::FromSeconds(SSL_SESSION_get_timeout(session.get()));
    UMA_HISTOGRAM_CUSTOM_TIMES("Net.SSLTLS13SessionLifetime", lifetime,
                               base::TimeDelta::FromMinutes(1),
                               base::TimeDelta::FromDays(7), 50);
  }

  auto iter = cache_.Get(cache_key);
  if (iter == cache_.end())
    iter = cache_.Put(cache_key, Entry());
  iter->second.Push(std::move(session));
}

void SSLClientSessionCache::FlushForServer(const HostPortPair& server) {
  auto iter = cache_.begin();
  while (iter != cache_.end()) {
    if (iter->first.server == server) {
      iter = cache_.Erase(iter);
    } else {
      ++iter;
    }
  }
}

void SSLClientSessionCache::Flush() {
  cache_.Clear();
}

void SSLClientSessionCache::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

bool SSLClientSessionCache::IsExpired(SSL_SESSION* session, time_t now) {
  if (now < 0)
    return true;
  uint64_t now_u64 = static_cast<uint64_t>(now);

  // now_u64 may be slightly behind because of differences in how
  // time is calculated at this layer versus BoringSSL.
  // Add a second of wiggle room to account for this.
  return now_u64 < SSL_SESSION_get_time(session) - 1 ||
         now_u64 >=
             SSL_SESSION_get_time(session) + SSL_SESSION_get_timeout(session);
}

void SSLClientSessionCache::DumpMemoryStats(
    base::trace_event::ProcessMemoryDump* pmd,
    const std::string& parent_absolute_name) const {
  std::string name = parent_absolute_name + "/ssl_client_session_cache";
  base::trace_event::MemoryAllocatorDump* cache_dump =
      pmd->CreateAllocatorDump(name);
  size_t cert_size = 0;
  size_t cert_count = 0;
  size_t undeduped_cert_size = 0;
  size_t undeduped_cert_count = 0;
  for (const auto& pair : cache_) {
    for (const auto& session : pair.second.sessions) {
      if (!session)
        continue;
      undeduped_cert_count += sk_CRYPTO_BUFFER_num(
          SSL_SESSION_get0_peer_certificates(session.get()));
    }
  }
  // Use a flat_set here to avoid malloc upon insertion.
  base::flat_set<const CRYPTO_BUFFER*> crypto_buffer_set;
  crypto_buffer_set.reserve(undeduped_cert_count);
  for (const auto& pair : cache_) {
    for (const auto& session : pair.second.sessions) {
      if (!session)
        continue;
      for (const CRYPTO_BUFFER* cert :
           SSL_SESSION_get0_peer_certificates(session.get())) {
        undeduped_cert_size += CRYPTO_BUFFER_len(cert);
        auto result = crypto_buffer_set.insert(cert);
        if (!result.second)
          continue;
        cert_size += CRYPTO_BUFFER_len(cert);
        cert_count++;
      }
    }
  }
  cache_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                        base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                        cert_size);
  cache_dump->AddScalar("cert_size",
                        base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                        cert_size);
  cache_dump->AddScalar("cert_count",
                        base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                        cert_count);
  cache_dump->AddScalar("undeduped_cert_size",
                        base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                        undeduped_cert_size);
  cache_dump->AddScalar("undeduped_cert_count",
                        base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                        undeduped_cert_count);
}

SSLClientSessionCache::Entry::Entry() = default;
SSLClientSessionCache::Entry::Entry(Entry&&) = default;
SSLClientSessionCache::Entry::~Entry() = default;

void SSLClientSessionCache::Entry::Push(bssl::UniquePtr<SSL_SESSION> session) {
  if (sessions[0] != nullptr &&
      SSL_SESSION_should_be_single_use(sessions[0].get())) {
    sessions[1] = std::move(sessions[0]);
  }
  sessions[0] = std::move(session);
}

bssl::UniquePtr<SSL_SESSION> SSLClientSessionCache::Entry::Pop() {
  if (sessions[0] == nullptr)
    return nullptr;
  bssl::UniquePtr<SSL_SESSION> session = bssl::UpRef(sessions[0]);
  if (SSL_SESSION_should_be_single_use(session.get())) {
    sessions[0] = std::move(sessions[1]);
    sessions[1] = nullptr;
  }
  return session;
}

bool SSLClientSessionCache::Entry::ExpireSessions(time_t now) {
  if (sessions[0] == nullptr)
    return true;

  if (SSLClientSessionCache::IsExpired(sessions[0].get(), now)) {
    return true;
  }

  if (sessions[1] != nullptr &&
      SSLClientSessionCache::IsExpired(sessions[1].get(), now)) {
    sessions[1] = nullptr;
  }

  return false;
}

void SSLClientSessionCache::FlushExpiredSessions() {
  time_t now = clock_->Now().ToTimeT();
  auto iter = cache_.begin();
  while (iter != cache_.end()) {
    if (iter->second.ExpireSessions(now)) {
      iter = cache_.Erase(iter);
    } else {
      ++iter;
    }
  }
}

void SSLClientSessionCache::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  switch (memory_pressure_level) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
      FlushExpiredSessions();
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      Flush();
      break;
  }
}

}  // namespace net
