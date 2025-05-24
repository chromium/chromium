// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_params.h"

namespace net::device_bound_sessions {

SessionParams::SessionParams(std::string id,
                             GURL fetcher_url,
                             std::string refresh_url,
                             Scope scope,
                             std::vector<Credential> creds,
                             unexportable_keys::UnexportableKeyId key_id)
    : session_id(std::move(id)),
      fetcher_url(std::move(fetcher_url)),
      refresh_url(std::move(refresh_url)),
      scope(std::move(scope)),
      credentials(std::move(creds)),
      key_id(std::move(key_id)) {}

SessionParams::SessionParams(SessionParams&& other) noexcept = default;

SessionParams& SessionParams::operator=(SessionParams&& other) noexcept =
    default;

SessionParams::~SessionParams() = default;

SessionParams::Scope::Scope() = default;

SessionParams::Scope::Scope(Scope&& other) noexcept = default;

SessionParams::Scope& SessionParams::Scope::operator=(
    SessionParams::Scope&& other) noexcept = default;

SessionParams::Scope::~Scope() = default;

}  // namespace net::device_bound_sessions
