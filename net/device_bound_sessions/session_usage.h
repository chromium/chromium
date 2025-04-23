// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_SESSION_USAGE_H_
#define NET_DEVICE_BOUND_SESSIONS_SESSION_USAGE_H_

namespace net::device_bound_sessions {

// Represents per-request usage of a session for populating use
// counters. This currently has the invariant that requests only move
// from lower-valued usage enums to higher-valued ones.
enum class SessionUsage {
  // Usage is unknown
  kUnknown = 0,
  // Request is not in scope of any session
  kNoUsage = 1,
  // Request is in scope of at least one session, but was not deferred
  kInScopeNotDeferred = 2,
  // Request was deferred by a session
  kDeferred = 3,
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_SESSION_USAGE_H_
