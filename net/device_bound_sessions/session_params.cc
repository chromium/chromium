// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_params.h"

namespace net::device_bound_sessions {

SessionParams::SessionParams(std::string id,
                             std::string refresh,
                             Scope incoming_scope,
                             std::vector<Credential> creds)
    : session_id(std::move(id)),
      refresh_url(std::move(refresh)),
      scope(std::move(incoming_scope)),
      credentials(std::move(creds)) {}

SessionParams::SessionParams(SessionParams&& other) = default;

SessionParams& SessionParams::operator=(SessionParams&& other) = default;

SessionParams::~SessionParams() = default;

SessionParams::Scope::Scope() = default;

SessionParams::Scope::Scope(Scope&& other) = default;

SessionParams::Scope& SessionParams::Scope::operator=(
    SessionParams::Scope&& other) = default;

SessionParams::Scope::~Scope() = default;

}  // namespace net::device_bound_sessions
