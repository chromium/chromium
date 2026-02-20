// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_FAILED_REQUEST_H_
#define NET_DEVICE_BOUND_SESSIONS_FAILED_REQUEST_H_

#include <string>

#include "net/base/net_export.h"
#include "url/gurl.h"

namespace net::device_bound_sessions {

// This class contains details about failed device bound session network
// requests. Used for DevTools.
struct NET_EXPORT FailedRequest {
  GURL request_url;
  std::optional<int> net_error;
  std::optional<int> response_error;
  std::optional<std::string> response_error_body;

  FailedRequest();
  ~FailedRequest();
  FailedRequest(const FailedRequest&);
  FailedRequest& operator=(const FailedRequest&);
  FailedRequest(FailedRequest&&) noexcept;
  FailedRequest& operator=(FailedRequest&&) noexcept;
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_FAILED_REQUEST_H_
