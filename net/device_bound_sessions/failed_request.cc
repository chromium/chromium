// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/failed_request.h"

namespace net::device_bound_sessions {

FailedRequest::FailedRequest() = default;
FailedRequest::~FailedRequest() = default;
FailedRequest::FailedRequest(const FailedRequest&) = default;
FailedRequest& FailedRequest::operator=(const FailedRequest&) = default;
FailedRequest::FailedRequest(FailedRequest&&) noexcept = default;
FailedRequest& FailedRequest::operator=(FailedRequest&&) noexcept = default;

}  // namespace net::device_bound_sessions
