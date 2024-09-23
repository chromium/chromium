// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/ssl/ssl_client_session_cache.h"

#include <tuple>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace net {

namespace {

// Returns a tuple of references to fields of |key|, for comparison purposes.
auto TieKeyFields(const SSLClientSessionCache::Key& key) {
  return std::tie(key.server, key.dest_ip_addr, key.network_anonymization_key,
                  key.privacy_mode);
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
  return TieKeyFields(*this) == TieKeyFields(other);
}

bool SSLClientSessionCache::Key::operator<(const Key& other) const {
  return TieKeyFields(*this) < TieKeyFields(other);
}

SSLClientSessionCache::SSLClientSessionCache(const Config& config)
    : clock_(base::DefaultClock::GetInstance()),
      config_(config),
      cache_(config.max_entries) {
  memory_pressure_listener_ = std::make_unique<base::MemoryPressureListener>(
      FROM_HERE, base::BindRepeating(&SSLClientSessionCache::OnMemoryPressure,
                                     base::Unretained(this)));
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

  return session;
}

void SSLClientSessionCache::Insert(const Key& cache_key,
                                   bssl::UniquePtr<SSL_SESSION> session) {
  auto iter = cache_.Get(cache_key);
  if (iter == cache_.end())
    iter = cache_.Put(cache_key, Entry());
  iter->second.Push(std::move(session));
}

void SSLClientSessionCache::ClearEarlyData(const Key& cache_key) {
  auto iter = cache_.Get(cache_key);
  if (iter != cache_.end()) {
    for (auto& session : iter->second.sessions) {
      if (session) {
        session.reset(SSL_SESSION_copy_without_early_data(session.get()));
      }
    }
  }
}

void SSLClientSessionCache::FlushForServers(
    const base::flat_set<HostPortPair>& servers) {
  auto iter = cache_.begin();
  while (iter != cache_.end()) {
    if (servers.contains(iter->first.server)) {
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
