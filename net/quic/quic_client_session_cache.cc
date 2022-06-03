// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_client_session_cache.h"

#include "base/time/clock.h"
#include "base/time/default_clock.h"

namespace net {

namespace {

const size_t kDefaultMaxEntries = 1024;
// Returns false if the SSL |session| doesn't exist or it is expired at |now|.
bool IsValid(SSL_SESSION* session, time_t now) {
  if (!session)
    return false;

  if (now < 0)
    return false;

  uint64_t now_u64 = static_cast<uint64_t>(now);

  // now_u64 may be slightly behind because of differences in how
  // time is calculated at this layer versus BoringSSL.
  // Add a second of wiggle room to account for this.
  return !(now_u64 < SSL_SESSION_get_time(session) - 1 ||
           now_u64 >= SSL_SESSION_get_time(session) +
                          SSL_SESSION_get_timeout(session));
}

bool DoApplicationStatesMatch(const quic::ApplicationState* state,
                              quic::ApplicationState* other) {
  if ((state && !other) || (!state && other))
    return false;
  if ((!state && !other) || *state == *other)
    return true;
  return false;
}

}  // namespace

QuicClientSessionCache::QuicClientSessionCache()
    : QuicClientSessionCache(kDefaultMaxEntries) {}

QuicClientSessionCache::QuicClientSessionCache(size_t max_entries)
    : clock_(base::DefaultClock::GetInstance()), cache_(max_entries) {
  memory_pressure_listener_ = std::make_unique<base::MemoryPressureListener>(
      FROM_HERE, base::BindRepeating(&QuicClientSessionCache::OnMemoryPressure,
                                     base::Unretained(this)));
}

QuicClientSessionCache::~QuicClientSessionCache() {
  Flush();
}

void QuicClientSessionCache::Insert(
    const quic::QuicServerId& server_id,
    bssl::UniquePtr<SSL_SESSION> session,
    const quic::TransportParameters& params,
    const quic::ApplicationState* application_state) {
  DCHECK(session) << "TLS session is not inserted into client cache.";
  auto iter = cache_.Get(server_id);
  if (iter == cache_.end()) {
    CreateAndInsertEntry(server_id, std::move(session), params,
                         application_state);
    return;
  }

  DCHECK(iter->second.params);
  // The states are both the same, so only need to insert sessions.
  if (params == *iter->second.params &&
      DoApplicationStatesMatch(application_state,
                               iter->second.application_state.get())) {
    iter->second.PushSession(std::move(session));
    return;
  }
  // Erase the existing entry because this Insert call must come from a
  // different QUIC session.
  cache_.Erase(iter);
  CreateAndInsertEntry(server_id, std::move(session), params,
                       application_state);
}

std::unique_ptr<quic::QuicResumptionState> QuicClientSessionCache::Lookup(
    const quic::QuicServerId& server_id,
    const SSL_CTX* /*ctx*/) {
  auto iter = cache_.Get(server_id);
  if (iter == cache_.end())
    return nullptr;

  time_t now = clock_->Now().ToTimeT();
  if (!IsValid(iter->second.PeekSession(), now)) {
    cache_.Erase(iter);
    return nullptr;
  }
  auto state = std::make_unique<quic::QuicResumptionState>();
  state->tls_session = iter->second.PopSession();
  if (iter->second.params != nullptr) {
    state->transport_params =
        std::make_unique<quic::TransportParameters>(*iter->second.params);
  }
  if (iter->second.application_state != nullptr) {
    state->application_state = std::make_unique<quic::ApplicationState>(
        *iter->second.application_state);
  }

  return state;
}

void QuicClientSessionCache::ClearEarlyData(
    const quic::QuicServerId& server_id) {
  auto iter = cache_.Get(server_id);
  if (iter == cache_.end())
    return;
  for (auto& session : iter->second.sessions) {
    if (session) {
      session.reset(SSL_SESSION_copy_without_early_data(session.get()));
    }
  }
}

void QuicClientSessionCache::FlushInvalidEntries() {
  time_t now = clock_->Now().ToTimeT();
  auto iter = cache_.begin();
  while (iter != cache_.end()) {
    if (!IsValid(iter->second.PeekSession(), now)) {
      iter = cache_.Erase(iter);
    } else {
      ++iter;
    }
  }
}

void QuicClientSessionCache::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  switch (memory_pressure_level) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
      FlushInvalidEntries();
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      Flush();
      break;
  }
}

void QuicClientSessionCache::Flush() {
  cache_.Clear();
}

void QuicClientSessionCache::CreateAndInsertEntry(
    const quic::QuicServerId& server_id,
    bssl::UniquePtr<SSL_SESSION> session,
    const quic::TransportParameters& params,
    const quic::ApplicationState* application_state) {
  Entry entry;
  entry.PushSession(std::move(session));
  entry.params = std::make_unique<quic::TransportParameters>(params);
  if (application_state) {
    entry.application_state =
        std::make_unique<quic::ApplicationState>(*application_state);
  }
  cache_.Put(server_id, std::move(entry));
}

QuicClientSessionCache::Entry::Entry() = default;
QuicClientSessionCache::Entry::Entry(Entry&&) = default;
QuicClientSessionCache::Entry::~Entry() = default;

void QuicClientSessionCache::Entry::PushSession(
    bssl::UniquePtr<SSL_SESSION> session) {
  if (sessions[0] != nullptr) {
    sessions[1] = std::move(sessions[0]);
  }
  sessions[0] = std::move(session);
}

bssl::UniquePtr<SSL_SESSION> QuicClientSessionCache::Entry::PopSession() {
  if (sessions[0] == nullptr)
    return nullptr;
  bssl::UniquePtr<SSL_SESSION> session = std::move(sessions[0]);
  sessions[0] = std::move(sessions[1]);
  sessions[1] = nullptr;
  return session;
}

SSL_SESSION* QuicClientSessionCache::Entry::PeekSession() {
  return sessions[0].get();
}

}  // namespace net
