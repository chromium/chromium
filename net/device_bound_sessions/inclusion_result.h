// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_INCLUSION_RESULT_H_
#define NET_DEVICE_BOUND_SESSIONS_INCLUSION_RESULT_H_

namespace net::device_bound_sessions {

enum class InclusionResult {
  // Definitely do not defer a request on behalf of this DBSC session.
  kExclude = 0,
  // Consider a request eligible for deferral on behalf of this session, if
  // other conditions are met.
  kInclude = 1,
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_INCLUSION_RESULT_H_
