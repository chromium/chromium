// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_DEVICE_BOUND_SESSION_SERVICE_H_
#define NET_DEVICE_BOUND_SESSIONS_DEVICE_BOUND_SESSION_SERVICE_H_

#include <memory>

#include "net/base/net_export.h"
#include "net/device_bound_sessions/device_bound_session_registration_fetcher_param.h"

namespace net {

class IsolationInfo;
class URLRequestContext;

// Main class for Device Bound Session Credentials (DBSC).
// Full information can be found at https://github.com/WICG/dbsc
class NET_EXPORT DeviceBoundSessionService {
 public:
  static std::unique_ptr<DeviceBoundSessionService> Create(
      const URLRequestContext* request_context);

  DeviceBoundSessionService(const DeviceBoundSessionService&) = delete;
  DeviceBoundSessionService& operator=(const DeviceBoundSessionService&) =
      delete;

  virtual ~DeviceBoundSessionService() = default;

  virtual void RegisterBoundSession(
      DeviceBoundSessionRegistrationFetcherParam registration_params,
      const IsolationInfo& isolation_info) = 0;

 protected:
  DeviceBoundSessionService() = default;
};

}  // namespace net

#endif  // NET_DEVICE_BOUND_SESSIONS_DEVICE_BOUND_SESSION_SERVICE_H_
