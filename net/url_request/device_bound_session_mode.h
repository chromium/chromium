// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_DEVICE_BOUND_SESSION_MODE_H_
#define NET_URL_REQUEST_DEVICE_BOUND_SESSION_MODE_H_

namespace net {

// Specifies how a URLRequest should interact with Device Bound Sessions (DBSC).
// This allows controlling whether DBSC headers are processed and whether the
// request can be deferred waiting for a session refresh.
enum class DeviceBoundSessionMode {
  // Default behavior: DBSC processing is enabled, and requests may be deferred
  // if a session refresh is required.
  kAllowed,
  // DBSC processing is enabled, but requests will bypass deferral checks.
  // This is used for internal refresh requests to prevent deadlocks.
  kBypassDeferral,
  // DBSC processing is completely disabled for this request.
  kDisabled,
};

}  // namespace net

#endif  // NET_URL_REQUEST_DEVICE_BOUND_SESSION_MODE_H_
