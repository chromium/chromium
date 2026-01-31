// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_CHALLENGE_RESULT_H_
#define NET_DEVICE_BOUND_SESSIONS_CHALLENGE_RESULT_H_

namespace net::device_bound_sessions {

// Result when attempting to set a challenge.
// LINT.IfChange(DeviceBoundSessionChallengeResult)
enum class ChallengeResult {
  kSuccess,             // Successfully set new challenge.
  kNoSessionId,         // No session_id found in header.
  kNoSessionMatch,      // Requested session not found.
  kCantSetBoundCookie,  // Request is not allowed to set cookies and therefore
                        // not challenges either.
};
// LINT.ThenChange(//services/network/public/mojom/device_bound_sessions.mojom:DeviceBoundSessionChallengeResult)

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_CHALLENGE_RESULT_H_
