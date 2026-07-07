// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_COOKIE_ACCESS_CHECK_PARAMS_H_
#define NET_DEVICE_BOUND_SESSIONS_COOKIE_ACCESS_CHECK_PARAMS_H_

#include "base/memory/raw_ref.h"
#include "net/base/net_export.h"
#include "url/origin.h"

namespace net::device_bound_sessions {

// Lightweight struct to avoid mixing parameters when calling the
// `SessionService::CookieAccessCallback`.
struct NET_EXPORT CookieAccessCheckParams {
  const raw_ref<const url::Origin> provider_origin;
  const raw_ref<const url::Origin> relying_party_origin;
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_COOKIE_ACCESS_CHECK_PARAMS_H_
