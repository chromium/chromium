// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_SESSION_USAGE_H_
#define NET_DEVICE_BOUND_SESSIONS_SESSION_USAGE_H_

#include "base/containers/flat_map.h"
#include "net/base/net_export.h"
#include "net/device_bound_sessions/session_key.h"

namespace net::device_bound_sessions {

// Represents per-request usage of a session for populating use
// counters. This currently has the invariant that requests only move
// from lower-valued usage enums to higher-valued ones.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(DeviceBoundSessionUsage)
enum class SessionUsage {
  // Usage is unknown.
  kUnknown = 0,
  // Request site does not match session site.
  kNoSiteMatchNotInScope = 1,
  // Request site matches session site, but request is not in scope of the
  // session.
  kSiteMatchNotInScope = 2,
  // Request site is in scope of session, but session doesn't need refreshing
  // yet.
  kInScopeRefreshNotYetNeeded = 3,
  // Request site is in scope of session, but request is not allowed to trigger
  // refresh.
  kInScopeRefreshNotAllowed = 4,
  // Request site is in scope of session, but a proactive refresh is not
  // possible.
  kInScopeProactiveRefreshNotPossible = 5,
  // Request site is in scope of session, and a proactive refresh is attempted.
  kInScopeProactiveRefreshAttempted = 6,
  // Request site is in scope of session, and refresh defers request.
  kDeferred = 7,
  kMaxValue = kDeferred
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:DeviceBoundSessionUsage)

// Determines the max usage of the usages contained in the map. Defaults
// to `kNoSiteMatchNotInScope` when the map is empty.
SessionUsage NET_EXPORT GetMaxUsage(
    const base::flat_map<SessionKey, SessionUsage>& device_bound_session_usage);

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_SESSION_USAGE_H_
